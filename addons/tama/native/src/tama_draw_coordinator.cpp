#include "tama_draw_coordinator.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>

using namespace godot;

// Per-vertex alpha: linear fade-in from tip, fade-out toward tail (idx=0 = tip).
static float _curved_trail_alpha(int idx, int trail_count) {
    int taper = (trail_count / 5 < 2) ? 2 : trail_count / 5;
    float tip_alpha  = float(idx + 1) / float(taper);
    float tail_alpha = float(trail_count - 1 - idx + 1) / float(taper);
    if (tip_alpha  > 1.0f) tip_alpha  = 1.0f;
    if (tail_alpha > 1.0f) tail_alpha = 1.0f;
    return tip_alpha * tail_alpha;
}

void _TamaDrawCoordinator::_bind_methods() {}

void _TamaDrawCoordinator::_ready() {
    set_physics_process(true);
    ProjectSettings *ps = ProjectSettings::get_singleton();
    _z_order_by_type     = (bool)ps->get_setting("tama/z_order_by_type", true);
    _composite_threshold = (int)ps->get_setting("tama/server_bullet_composite_threshold", 1000);
}

// ---------------------------------------------------------------------------
// Canvas item management
// ---------------------------------------------------------------------------

RID _TamaDrawCoordinator::_get_or_create_blend_ci(int blend_mode) {
    auto it = _blend_cis.find(blend_mode);
    if (it != _blend_cis.end()) return it->second;

    RenderingServer *rs = RenderingServer::get_singleton();
    RID ci = rs->canvas_item_create();
    rs->canvas_item_set_parent(ci, get_canvas_item());
    rs->canvas_item_set_z_index(ci, blend_mode);  // normal(0) below additive(1), etc.

    Ref<CanvasItemMaterial> mat;
    if (blend_mode != 0) {
        mat.instantiate();
        mat->set_blend_mode((CanvasItemMaterial::BlendMode)blend_mode);
        rs->canvas_item_set_material(ci, mat->get_rid());
        _blend_mats[blend_mode] = mat;
    }
    _blend_cis[blend_mode] = ci;
    return ci;
}

// ---------------------------------------------------------------------------
// Physics process — collect and sort draw jobs from all pools
// ---------------------------------------------------------------------------

void _TamaDrawCoordinator::_physics_process(double /*delta*/) {
    // Rebuild sorted type list when new types have been registered since last frame.
    // In normal usage types are registered once before gameplay, so this runs exactly once.
    if (_z_order_by_type) {
        int total = (_bullet_pool ? (int)_bullet_pool->_types.size() : 0)
                  + (_laser_pool  ? (int)_laser_pool->_types.size()  : 0)
                  + (_curved_pool ? (int)_curved_pool->_types.size() : 0);
        if (total != _cached_type_count) {
            _type_entries.clear();
            if (_bullet_pool)
                for (auto &[key, td] : _bullet_pool->_types)
                    _type_entries.push_back({td->z_index, 0, td});
            if (_laser_pool)
                for (auto &[key, td] : _laser_pool->_types)
                    _type_entries.push_back({td->z_index, 1, td});
            if (_curved_pool)
                for (auto &[key, td] : _curved_pool->_types)
                    _type_entries.push_back({td->z_index, 2, td});
            std::stable_sort(_type_entries.begin(), _type_entries.end(),
                [](const TypeEntry &a, const TypeEntry &b) { return a.z_index < b.z_index; });
            _cached_type_count = total;
        }
    }

    _jobs.clear();

    // Bullets: one job per active batch per type.
    // Regular batch jobs are pushed first, then spawn animation jobs for the same batch.
    // stable_sort preserves this relative order so spawn animations always paint on top
    // of their batch's regular bullets while still respecting spawn_frame ordering.
    if (_bullet_pool) {
        for (auto &[key, td] : _bullet_pool->_types) {
            for (TamaServerBulletPool::BatchData *batch : td->active_batches) {
                DrawJob j;
                j.spawn_frame = batch->birth_frame;
                j.blend_mode  = td->blend_mode;
                j.kind        = DRAW_BULLET_BATCH;
                j.data        = batch;
                j.extra       = td;
                _jobs.push_back(j);
            }
        }
        // Second pass: spawn animation jobs (same spawn_frame, pushed after regular → stable_sort keeps order).
        for (auto &[key, td] : _bullet_pool->_types) {
            for (TamaServerBulletPool::BatchData *batch : td->active_batches) {
                if (batch->spawn_active_count <= 0) continue;
                if (!batch->spawn_multimesh_res.is_valid()) continue;
                DrawJob j;
                j.spawn_frame = batch->birth_frame;
                j.blend_mode  = td->spawn_blend_mode;
                j.kind        = DRAW_BULLET_BATCH_SPAWN;
                j.data        = batch;
                j.extra       = td;
                _jobs.push_back(j);
            }
        }
    }

    // Straight lasers: one job per active laser.
    if (_laser_pool) {
        for (LaserState *l : _laser_pool->_active) {
            if (!l->active) continue;
            TamaServerLaserPool::TypeData *td =
                static_cast<TamaServerLaserPool::TypeData *>(l->type_data_ptr);
            DrawJob j;
            j.spawn_frame = l->spawn_frame;
            j.blend_mode  = td->blend_mode;
            j.kind        = DRAW_STRAIGHT_LASER;
            j.data        = l;
            j.extra       = td;
            _jobs.push_back(j);
        }
    }

    // Curved lasers: one job per active laser.
    if (_curved_pool) {
        for (CurvedLaserState *l : _curved_pool->_active) {
            if (!l->active) continue;
            TamaServerCurvedLaserPool::TypeData *td =
                static_cast<TamaServerCurvedLaserPool::TypeData *>(l->type_data_ptr);
            DrawJob j;
            j.spawn_frame = l->spawn_frame;
            j.blend_mode  = td->blend_mode;
            j.kind        = DRAW_CURVED_LASER;
            j.data        = l;
            j.extra       = td;
            _jobs.push_back(j);
        }
    }

    if (!_z_order_by_type) {
        std::stable_sort(_jobs.begin(), _jobs.end(),
            [](const DrawJob &a, const DrawJob &b) {
                return a.spawn_frame < b.spawn_frame;
            });
    }

    queue_redraw();
}

// ---------------------------------------------------------------------------
// _draw — emit sorted draw commands into per-blend-mode canvas items
// ---------------------------------------------------------------------------

int _TamaDrawCoordinator::_accumulate_curved(
    CurvedLaserState *l,
    TamaServerCurvedLaserPool::TypeData *td,
    int vert_base)
{
    int count  = l->trail_count;
    int length = td->length;
    int n      = count - 1;

    for (int k = 0; k < count; ++k) {
        int   slot = (l->trail_head + k) % length;
        float v    = (n > 0) ? (float(k) / float(n)) : 0.0f;
        float a    = _curved_trail_alpha(k, count);
        Color c(1.0f, 1.0f, 1.0f, a);
        _acc_pts.push_back(l->edge_l[slot]);
        _acc_pts.push_back(l->edge_r[slot]);
        _acc_col.push_back(c);
        _acc_col.push_back(c);
        _acc_uv.push_back(Vector2(0.0f, v));
        _acc_uv.push_back(Vector2(1.0f, v));
    }
    for (int s = 0; s < n; ++s) {
        Vector2 A = l->trail[(l->trail_head + s)     % length];
        Vector2 B = l->trail[(l->trail_head + s + 1) % length];
        if ((B - A).length_squared() < 0.0001f) continue;
        int lA = vert_base + s * 2,   rA = vert_base + s * 2 + 1;
        int lB = vert_base + (s+1)*2, rB = vert_base + (s+1)*2 + 1;
        _idx_buf.push_back(lA); _idx_buf.push_back(rA); _idx_buf.push_back(rB);
        _idx_buf.push_back(rB); _idx_buf.push_back(lB); _idx_buf.push_back(lA);
    }
    return vert_base + count * 2;
}

void _TamaDrawCoordinator::_draw() {
    RenderingServer *rs = RenderingServer::get_singleton();

    // Clear all blend-mode canvas items.
    for (auto &[bm, ci] : _blend_cis)
        rs->canvas_item_clear(ci);

    // Curved laser batch accumulation state.
    // Default mode: consecutive DRAW_CURVED_LASER jobs with the same
    //   (spawn_frame, blend_mode, texture_rid) are merged into one triangle_array call.
    // Optimized mode: spawn_frame is NOT part of the key, so all lasers sharing
    //   (blend_mode, texture_rid) merge regardless of when they were fired.
    int64_t curved_frame     = -1;
    int     curved_bm        = -1;
    RID     curved_tex;
    RID     curved_ci;
    int     curved_vert_base = 0;

    auto flush_curved = [&]() {
        if (_idx_buf.empty()) return;
        int ni = (int)_idx_buf.size();
        int nv = (int)_acc_pts.size();
        _flush_idx.resize(ni);
        _flush_pts.resize(nv);
        _flush_col.resize(nv);
        _flush_uv.resize(nv);
        int32_t *ip  = _flush_idx.ptrw();
        Vector2 *pp  = _flush_pts.ptrw();
        Color   *cp  = _flush_col.ptrw();
        Vector2 *uvp = _flush_uv.ptrw();
        for (int j = 0; j < ni; ++j) ip[j]  = _idx_buf[j];
        for (int j = 0; j < nv; ++j) pp[j]  = _acc_pts[j];
        for (int j = 0; j < nv; ++j) cp[j]  = _acc_col[j];
        for (int j = 0; j < nv; ++j) uvp[j] = _acc_uv[j];
        rs->canvas_item_add_triangle_array(
            curved_ci, _flush_idx, _flush_pts, _flush_col, _flush_uv,
            PackedInt32Array(), PackedFloat32Array(), curved_tex);
        _idx_buf.clear();
        _acc_pts.clear();
        _acc_col.clear();
        _acc_uv.clear();
        curved_vert_base = 0;
    };

    // Helper: emit a non-curved job immediately.
    auto emit_non_curved = [&](const DrawJob &job) {
        RID ci = _get_or_create_blend_ci(job.blend_mode);
        switch (job.kind) {
            case DRAW_BULLET_BATCH:
                _draw_bullet_batch(
                    static_cast<TamaServerBulletPool::TypeData *>(job.extra),
                    static_cast<TamaServerBulletPool::BatchData *>(job.data),
                    ci, rs);
                break;
            case DRAW_BULLET_BATCH_SPAWN:
                _draw_bullet_batch_spawn(
                    static_cast<TamaServerBulletPool::TypeData *>(job.extra),
                    static_cast<TamaServerBulletPool::BatchData *>(job.data),
                    ci, rs);
                break;
            case DRAW_STRAIGHT_LASER:
                _draw_straight_laser(
                    static_cast<TamaServerLaserPool::TypeData *>(job.extra),
                    static_cast<LaserState *>(job.data),
                    ci, rs);
                break;
            default: break;
        }
    };

    if (_z_order_by_type) {
        // Z-order-by-type mode: all registered types across all pools are sorted by their
        // z_index and drawn in that order (lower z_index first = rendered below).
        // Within a type, efficient batching is used: composite for static bullet types above
        // the threshold, per-batch for animated, and per-texture for curved lasers.

        bool use_composite = _bullet_pool &&
            (int)_bullet_pool->_active.size() > _composite_threshold;

        for (const TypeEntry &entry : _type_entries) {
            if (entry.pool == 0) {
                auto *td = static_cast<TamaServerBulletPool::TypeData *>(entry.td);
                if (td->active_batches.empty()) continue;
                bool animated = td->anim_frames.size() > 1;
                RID  ci       = _get_or_create_blend_ci(td->blend_mode);

                if (!animated && use_composite && td->composite.is_valid()) {
                    int total_floats = 0;
                    for (TamaServerBulletPool::BatchData *batch : td->active_batches)
                        total_floats += batch->used * 8;
                    if (total_floats > 0) {
                        _composite_buf.resize(total_floats);
                        float *dst = _composite_buf.ptrw();
                        int offset = 0;
                        for (TamaServerBulletPool::BatchData *batch : td->active_batches) {
                            if (batch->used <= 0) continue;
                            PackedFloat32Array src = rs->multimesh_get_buffer(batch->multimesh);
                            int count = batch->used * 8;
                            if (src.size() >= count) {
                                std::memcpy(dst + offset, src.ptr(), count * sizeof(float));
                                offset += count;
                            }
                        }
                        if (offset > 0) {
                            td->composite_res->set_instance_count(offset / 8);
                            rs->multimesh_set_buffer(td->composite, _composite_buf);
                            RID tex = td->first_texture.is_valid() ? td->first_texture->get_rid() : RID();
                            rs->canvas_item_add_multimesh(ci, td->composite, tex);
                        }
                    }
                } else {
                    for (TamaServerBulletPool::BatchData *batch : td->active_batches)
                        _draw_bullet_batch(td, batch, ci, rs);
                }

                RID spawn_ci = _get_or_create_blend_ci(td->spawn_blend_mode);
                for (TamaServerBulletPool::BatchData *batch : td->active_batches) {
                    if (batch->spawn_active_count <= 0 || !batch->spawn_multimesh_res.is_valid()) continue;
                    _draw_bullet_batch_spawn(td, batch, spawn_ci, rs);
                }

            } else if (entry.pool == 1) {
                auto *td = static_cast<TamaServerLaserPool::TypeData *>(entry.td);
                RID ci   = _get_or_create_blend_ci(td->blend_mode);
                for (LaserState &l : td->lasers) {
                    if (!l.active) continue;
                    _draw_straight_laser(td, &l, ci, rs);
                }

            } else {
                // Curved laser type: batch lasers by texture within this type.
                auto *td = static_cast<TamaServerCurvedLaserPool::TypeData *>(entry.td);
                RID ci   = _get_or_create_blend_ci(td->blend_mode);
                curved_bm  = td->blend_mode;
                curved_ci  = ci;
                for (CurvedLaserState &l : td->lasers) {
                    if (!l.active || l.trail_count < 2) continue;
                    RID tex_rid;
                    if (!td->texture_frames.empty()) {
                        Ref<Texture2D> tex = (td->texture_frames.size() > 1)
                            ? td->texture_frames[l.tex_anim_frame].texture
                            : td->texture_frames[0].texture;
                        if (tex.is_valid()) tex_rid = tex->get_rid();
                    }
                    if (!_idx_buf.empty() && curved_tex != tex_rid) flush_curved();
                    curved_tex = tex_rid;
                    curved_vert_base = _accumulate_curved(&l, td, curved_vert_base);
                }
                flush_curved();
            }
        }

    } else {
        // Default mode: jobs are sorted by spawn_frame; curved lasers batch only when
        // consecutive with the same (spawn_frame, blend_mode, tex_rid).
        for (const DrawJob &job : _jobs) {
            if (job.kind == DRAW_CURVED_LASER) {
                auto *td = static_cast<TamaServerCurvedLaserPool::TypeData *>(job.extra);
                auto *l  = static_cast<CurvedLaserState *>(job.data);
                if (l->trail_count < 2) continue;

                RID tex_rid;
                if (!td->texture_frames.empty()) {
                    Ref<Texture2D> tex = (td->texture_frames.size() > 1)
                        ? td->texture_frames[l->tex_anim_frame].texture
                        : td->texture_frames[0].texture;
                    if (tex.is_valid()) tex_rid = tex->get_rid();
                }
                RID ci = _get_or_create_blend_ci(job.blend_mode);

                bool same_batch = !_idx_buf.empty()
                    && curved_frame == job.spawn_frame
                    && curved_bm    == job.blend_mode
                    && curved_tex   == tex_rid
                    && curved_ci    == ci;
                if (!same_batch) {
                    flush_curved();
                    curved_frame = job.spawn_frame;
                    curved_bm    = job.blend_mode;
                    curved_tex   = tex_rid;
                    curved_ci    = ci;
                }
                curved_vert_base = _accumulate_curved(l, td, curved_vert_base);

            } else {
                flush_curved();
                emit_non_curved(job);
            }
        }
        flush_curved();
    }
}

// ---------------------------------------------------------------------------
// Exit — free coordinator-owned canvas items
// ---------------------------------------------------------------------------

void _TamaDrawCoordinator::_exit_tree() {
    RenderingServer *rs = RenderingServer::get_singleton();
    for (auto &[bm, ci] : _blend_cis)
        rs->free_rid(ci);
    _blend_cis.clear();
    _blend_mats.clear();
}

// ---------------------------------------------------------------------------
// Per-kind draw helpers
// ---------------------------------------------------------------------------

void _TamaDrawCoordinator::_draw_bullet_batch(
        TamaServerBulletPool::TypeData *td,
        TamaServerBulletPool::BatchData *batch,
        RID ci, RenderingServer *rs)
{
    RID tex = batch->current_texture.is_valid() ? batch->current_texture->get_rid() : RID();
    rs->canvas_item_add_multimesh(ci, batch->multimesh, tex);
}

void _TamaDrawCoordinator::_draw_bullet_batch_spawn(
        TamaServerBulletPool::TypeData *td,
        TamaServerBulletPool::BatchData *batch,
        RID ci, RenderingServer *rs)
{
    batch->spawn_multimesh_res->set_visible_instance_count(batch->used);
    Ref<Texture2D> stex;
    if (!td->spawn_anim_frames.empty()) {
        stex = td->spawn_anim_frames[batch->spawn_anim_frame].texture;
    } else {
        stex = td->spawn_texture.is_valid() ? td->spawn_texture : td->first_texture;
    }
    RID stex_rid = stex.is_valid() ? stex->get_rid() : RID();
    rs->canvas_item_add_multimesh(ci, batch->spawn_multimesh, stex_rid);
}

void _TamaDrawCoordinator::_draw_straight_laser(
        TamaServerLaserPool::TypeData *td,
        LaserState *l,
        RID ci, RenderingServer *rs)
{
    Color modulate(1.0f, 1.0f, 1.0f, l->current_opacity);
    float half_w = l->current_width * 0.5f;

    RID tex_rid;
    if (!td->texture_frames.empty()) {
        Ref<Texture2D> t = td->texture_frames[l->tex_anim_frame].texture;
        if (t.is_valid()) tex_rid = t->get_rid();
    }
    RID base_tex_rid;
    if (!td->base_texture_frames.empty()) {
        Ref<Texture2D> t = td->base_texture_frames[l->base_anim_frame].texture;
        if (t.is_valid()) base_tex_rid = t->get_rid();
    }

    // All draw commands in the laser's local frame: origin at base, +X along the beam.
    rs->canvas_item_add_set_transform(ci,
        Transform2D(l->angle, Vector2(1.0f, 1.0f), 0.0f, l->position));

    const Rect2 body_rect(0.0f, -half_w, td->length, l->current_width);
    if (tex_rid.is_valid()) {
        if (!td->tile_x && !td->tile_y) {
            rs->canvas_item_add_texture_rect(ci, body_rect, tex_rid, false, modulate);
        } else if (td->tile_x && td->tile_y) {
            rs->canvas_item_add_texture_rect(ci, body_rect, tex_rid, true, modulate);
        } else if (td->tile_x) {
            Ref<Texture2D> tex = td->texture_frames[l->tex_anim_frame].texture;
            Vector2i tsz = tex->get_size();
            if (tsz.x > 0) {
                float x = 0.0f, rem = td->length;
                while (rem > 0.0f) {
                    float w = std::min((float)tsz.x, rem);
                    rs->canvas_item_add_texture_rect_region(ci,
                        Rect2(x, -half_w, w, l->current_width),
                        tex_rid, Rect2(0, 0, w, (float)tsz.y), modulate);
                    x   += (float)tsz.x;
                    rem -= (float)tsz.x;
                }
            }
        } else {
            Ref<Texture2D> tex = td->texture_frames[l->tex_anim_frame].texture;
            Vector2i tsz = tex->get_size();
            if (tsz.y > 0) {
                float y = -half_w, rem = l->current_width;
                while (rem > 0.0f) {
                    float h = std::min((float)tsz.y, rem);
                    rs->canvas_item_add_texture_rect_region(ci,
                        Rect2(0.0f, y, td->length, h),
                        tex_rid, Rect2(0, 0, (float)tsz.x, h), modulate);
                    y   += (float)tsz.y;
                    rem -= (float)tsz.y;
                }
            }
        }
    } else {
        rs->canvas_item_add_rect(ci, body_rect, modulate);
    }

    if (base_tex_rid.is_valid()) {
        Ref<Texture2D> base_tex = td->base_texture_frames[l->base_anim_frame].texture;
        Vector2i bsz = base_tex->get_size();
        rs->canvas_item_add_texture_rect(ci,
            Rect2(-bsz.x * 0.5f, -bsz.y * 0.5f, (float)bsz.x, (float)bsz.y),
            base_tex_rid, false, modulate);
    }

    // Reset transform so subsequent lasers in the same canvas item start clean.
    rs->canvas_item_add_set_transform(ci, Transform2D());
}

