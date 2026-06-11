#include "tama_server_curved_laser_pool.h"
#include "tama_interpreter.h"
#include "tama_manager.h"
#include "tama_server_curved_laser_config.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static constexpr float DEG2RAD = 3.14159265f / 180.0f;

// ---------------------------------------------------------------------------
// Bind
// ---------------------------------------------------------------------------

void TamaServerCurvedLaserPool::_bind_methods() {
    ClassDB::bind_method(D_METHOD("recycle","laser_wrapper"), &TamaServerCurvedLaserPool::recycle);
    ClassDB::bind_method(D_METHOD("recycle_all"),             &TamaServerCurvedLaserPool::recycle_all);
    ClassDB::bind_method(D_METHOD("get_active_count"),        &TamaServerCurvedLaserPool::get_active_count);

    ADD_SIGNAL(MethodInfo("laser_hit",
        PropertyInfo(Variant::OBJECT, "laser",
                     PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "TamaServerCurvedLaser")));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

TamaServerCurvedLaserPool::~TamaServerCurvedLaserPool() {
    for (auto &[key, td] : _types) {
        for (auto *w : td->wrappers) memdelete(w);
        delete td;
    }
}

void TamaServerCurvedLaserPool::_ready() {
    set_physics_process(true);
    for (auto &pr : _pending_regs)
        register_type(pr.key, pr.config);
    _pending_regs.clear();
}

void TamaServerCurvedLaserPool::_exit_tree() {
    recycle_all();
}

// ---------------------------------------------------------------------------
// Register type
// ---------------------------------------------------------------------------

void TamaServerCurvedLaserPool::register_type(const String &p_key, Object *p_config) {
    if (!is_inside_tree()) {
        _pending_regs.push_back({p_key, p_config});
        return;
    }
    std::string key = p_key.utf8().get_data();
    if (_types.count(key)) {
        UtilityFunctions::push_warning("TamaServerCurvedLaserPool: type '" + p_key + "' already registered.");
        return;
    }

    TypeData *td    = new TypeData();
    td->config_obj  = p_config;
    td->width                = (float)p_config->get("width");
    td->length               = std::max(2, (int)p_config->get("length"));
    td->pool_size            = (int)  p_config->get("pool_size");
    td->out_of_bounds_margin = (float)p_config->get("out_of_bounds_margin");
    {
        Variant v = p_config->get("texture");
        if (v.get_type() == Variant::OBJECT) {
            TamaAnimatedTexture *anim = Object::cast_to<TamaAnimatedTexture>(v.operator Object *());
            if (anim) {
                td->texture_frames = anim->build_anim_frames();
                td->blend_mode     = anim->get_blend_mode();
            }
        }
    }

    int n = td->pool_size;
    _types[key] = td;

    // Per-type draw node for blend mode isolation (same pattern as TamaServerBulletPool).
    td->draw_node = memnew(Node2D);
    add_child(td->draw_node);
    if (td->blend_mode != 0) {
        td->material.instantiate();
        td->material->set_blend_mode((CanvasItemMaterial::BlendMode)td->blend_mode);
        td->draw_node->set_material(td->material);
    }

    td->lasers.resize(n);
    td->wrappers.resize(n);
    for (int i = 0; i < n; ++i) {
        td->lasers[i].global_slot = i;
        td->lasers[i].trail.resize(td->length);
        td->lasers[i].edge_l.resize(td->length);
        td->lasers[i].edge_r.resize(td->length);
        TamaServerCurvedLaser *w = memnew(TamaServerCurvedLaser);
        w->_init_slot(this, &td->lasers[i], key);
        td->wrappers[i] = w;
    }

    td->ring.resize(n);
    for (int i = 0; i < n; ++i) td->ring[i] = i;
    td->ring_r     = 0;
    td->ring_w     = 0;
    td->ring_count = n;
}

// ---------------------------------------------------------------------------
// Spawn
// ---------------------------------------------------------------------------

Object *TamaServerCurvedLaserPool::spawn(
        const TamaBulletFireData &p_data, Object *p_config,
        float angle, float speed, Vector2 position,
        Object *p_context)
{
    if (!p_config) return nullptr;

    const std::string &key = p_data.bullet_type;
    auto it = _types.find(key);
    if (it == _types.end()) {
        for (auto &[k, td_ptr] : _types) {
            if (td_ptr->config_obj == p_config) { it = _types.find(k); break; }
        }
        if (it == _types.end()) return nullptr;
    }
    TypeData &td = *it->second;
    if (td.ring_count == 0) return nullptr;

    int n           = (int)td.lasers.size();
    int global_slot = td.ring[td.ring_r];
    td.ring_r       = (td.ring_r + 1) % n;
    --td.ring_count;

    int64_t frame = Engine::get_singleton()->get_physics_frames();

    CurvedLaserState &l = td.lasers[global_slot];
    l.active         = true;
    l.spawn_frame    = frame;
    l.active_idx     = (int32_t)_active.size();
    _active.push_back(&l);

    l.position         = position;
    l.angle            = angle;
    l.last_angle       = angle;
    l.speed            = speed;
    l.last_speed       = speed;
    l.rot_speed        = 0.0f;
    l.last_rot_speed   = 0.0f;
    l.current_width          = td.width;
    l.angle_tween.active     = false;
    l.speed_tween.active     = false;
    l.rot_speed_tween.active = false;
    l.tex_anim_frame         = 0;
    l.tex_anim_time          = 0.0f;
    l.trail_head             = 0;
    l.trail_count            = 0;
    l.edge_built             = false;

    l.runner = nullptr;
    if (p_data.bullet_act) {
        _TamaInterpreter *runner = new _TamaInterpreter();
        runner->set_context(p_context);
        l.runner = runner;

        TamaScope act_scope;
        int na = (int)std::min(p_data.bullet_params.size(), p_data.bullet_args.size());
        for (int i = 0; i < na; ++i)
            act_scope[p_data.bullet_params[i]] = TamaScopeVal(p_data.bullet_args[i]);
        act_scope["spawn_x"] = TamaScopeVal(position.x);
        act_scope["spawn_y"] = TamaScopeVal(position.y);

        TamaServerCurvedLaser *wrapper = Object::cast_to<TamaServerCurvedLaser>(l.wrapper);
        runner->set_event_handler(wrapper);

        TamaManager *mgr = TamaManager::get_instance();
        _TamaSpawnManager *sm = mgr ? mgr->_get_spawn_manager() : nullptr;
        if (sm) sm->connect_interpreter(runner, l.wrapper);

        runner->start_act(p_data.source_program, p_data.bullet_act, std::move(act_scope));
    }

    queue_redraw();
    return l.wrapper;
}

// ---------------------------------------------------------------------------
// Recycle
// ---------------------------------------------------------------------------

void TamaServerCurvedLaserPool::recycle(Object *p_wrapper) {
    if (!p_wrapper) return;
    TamaServerCurvedLaser *w = Object::cast_to<TamaServerCurvedLaser>(p_wrapper);
    if (!w || !w->_state) return;
    _recycle_internal(w->_state);
}

void TamaServerCurvedLaserPool::recycle_all() {
    std::vector<CurvedLaserState *> copy = _active;
    for (auto *l : copy) _recycle_internal(l);
    queue_redraw();
}

void TamaServerCurvedLaserPool::_recycle_internal(CurvedLaserState *l) {
    if (!l || !l->active) return;
    l->active = false;

    int idx  = l->active_idx;
    int last = (int)_active.size() - 1;
    if (idx != last) {
        _active[idx] = _active[last];
        _active[idx]->active_idx = idx;
    }
    _active.pop_back();
    l->active_idx = -1;

    if (l->runner) {
        delete l->runner;
        l->runner = nullptr;
    }

    // Return slot to the free ring — find the TypeData that owns this laser
    for (auto &[key, td] : _types) {
        int n = (int)td->lasers.size();
        if (l->global_slot >= 0 && l->global_slot < n && &td->lasers[l->global_slot] == l) {
            td->ring[td->ring_w] = l->global_slot;
            td->ring_w = (td->ring_w + 1) % n;
            ++td->ring_count;
            break;
        }
    }

    queue_redraw();
}

// ---------------------------------------------------------------------------
// Physics process helpers
// ---------------------------------------------------------------------------

// Compute and cache the left/right ribbon edge vertices for one buffer slot.
// Slot j's perpendicular is the average of the directions to its neighbors:
//   newer neighbor = (j-1+length)%length,  older = (j+1)%length.
// Neighbors that don't exist (tip/tail) are simply omitted.
// Indexed by buffer slot (not logical index), so the cache is stable as the
// trail scrolls — only two slots change per frame (new head + previous head).
static void _cache_edge_slot(CurvedLaserState &l, int j, int length, float hw) {
    int logical = (j - l.trail_head + length) % length;
    Vector2 A   = l.trail[j];
    Vector2 dir(0.0f, 0.0f);
    if (logical > 0) {
        Vector2 d = A - l.trail[(j - 1 + length) % length];
        float len = d.length();
        if (len > 0.001f) dir += d / len;
    }
    if (logical < l.trail_count - 1) {
        Vector2 d = l.trail[(j + 1) % length] - A;
        float len = d.length();
        if (len > 0.001f) dir += d / len;
    }
    float dlen = dir.length();
    Vector2 perp(0.0f, 1.0f);
    if (dlen > 0.001f) {
        dir /= dlen;
        perp = Vector2(-dir.y, dir.x);
    }
    l.edge_l[j] = A + perp * hw;
    l.edge_r[j] = A - perp * hw;
}

// ---------------------------------------------------------------------------
// Physics process
// ---------------------------------------------------------------------------

void TamaServerCurvedLaserPool::_physics_process(double p_delta) {
    float delta = (float)p_delta;
    int64_t cur_frame = Engine::get_singleton()->get_physics_frames();
    Rect2 world = _world_bounds();
    TamaManager *mgr = TamaManager::get_instance();
    float global_margin = mgr ? mgr->get_global_out_of_bounds_margin() : -1.0f;
    Vector2 player     = mgr ? mgr->get_player_position() : Vector2();
    float hitbox_r     = mgr ? mgr->get_player_hitbox_radius() : 3.0f;

    _to_recycle.clear();

    for (auto &[key, td_iter] : _types) {
        TypeData *td = td_iter;

        for (int i = 0; i < (int)td->lasers.size(); ++i) {
            CurvedLaserState &l = td->lasers[i];
            if (!l.active) continue;
            if (l.spawn_frame == cur_frame) continue;

            // Step runner (may fire chdir/chspd/chrotspd events)
            if (l.runner && l.runner->is_running()) l.runner->step(delta);
            if (!l.active) continue;

            // Step tweens
            if (l.angle_tween.active)     l.angle     = l.angle_tween.step(delta);
            if (l.speed_tween.active)     l.speed     = l.speed_tween.step(delta);
            if (l.rot_speed_tween.active) l.rot_speed = l.rot_speed_tween.step(delta);

            // Apply rotation and movement
            if (l.rot_speed != 0.0f) l.angle += l.rot_speed * DEG2RAD * delta;
            l.position += Vector2(std::cos(l.angle), std::sin(l.angle)) * l.speed * delta;

            // Capture dropped tail info before overwriting (needed for AABB update below)
            bool    was_full     = (l.trail_count == td->length);
            int     new_head_slot = (l.trail_head - 1 + td->length) % td->length;
            Vector2 dropped_tail = was_full ? l.trail[new_head_slot] : Vector2();

            // Push new position to circular trail buffer
            l.trail_head = new_head_slot;
            l.trail[l.trail_head] = l.position;
            if (l.trail_count < td->length) ++l.trail_count;

            // Out-of-bounds check on the head (tip) position
            {
                float m = (global_margin >= 0.0f) ? global_margin : td->out_of_bounds_margin;
                if (l.position.x < world.position.x - m ||
                    l.position.y < world.position.y - m ||
                    l.position.x > world.get_end().x   + m ||
                    l.position.y > world.get_end().y   + m)
                {
                    _to_recycle.push_back(&l);
                    continue;
                }
            }

            // ----------------------------------------------------------------
            // Edge vertex cache — incremental update.
            // Full build on the first active frame; afterwards only the two
            // slots that gained a new/changed neighbour need recomputing.
            // ----------------------------------------------------------------
            {
                float hw = l.current_width * 0.5f;
                if (!l.edge_built) {
                    // Full build — compute edge cache and AABB in the same loop.
                    float min_x = 1e30f, max_x = -1e30f, min_y = 1e30f, max_y = -1e30f;
                    for (int k = 0; k < l.trail_count; ++k) {
                        int slot = (l.trail_head + k) % td->length;
                        _cache_edge_slot(l, slot, td->length, hw);
                        Vector2 p = l.trail[slot];
                        if (p.x < min_x) min_x = p.x; if (p.x > max_x) max_x = p.x;
                        if (p.y < min_y) min_y = p.y; if (p.y > max_y) max_y = p.y;
                    }
                    l.aabb_min = Vector2(min_x, min_y);
                    l.aabb_max = Vector2(max_x, max_y);
                    l.edge_built = true;
                } else {
                    // New head: only has an older neighbour (no newer one yet).
                    _cache_edge_slot(l, l.trail_head, td->length, hw);
                    // Previous head: now has the new head as its newer neighbour.
                    if (l.trail_count >= 2)
                        _cache_edge_slot(l, (l.trail_head + 1) % td->length, td->length, hw);
                    // Expand AABB for the new head position.
                    if (l.position.x < l.aabb_min.x) l.aabb_min.x = l.position.x;
                    else if (l.position.x > l.aabb_max.x) l.aabb_max.x = l.position.x;
                    if (l.position.y < l.aabb_min.y) l.aabb_min.y = l.position.y;
                    else if (l.position.y > l.aabb_max.y) l.aabb_max.y = l.position.y;
                    // If the dropped tail was a boundary point the AABB may have shrunk;
                    // recompute fully in that case.
                    if (was_full && (
                        dropped_tail.x == l.aabb_min.x || dropped_tail.x == l.aabb_max.x ||
                        dropped_tail.y == l.aabb_min.y || dropped_tail.y == l.aabb_max.y))
                    {
                        float min_x = 1e30f, max_x = -1e30f, min_y = 1e30f, max_y = -1e30f;
                        for (int k = 0; k < l.trail_count; ++k) {
                            Vector2 p = l.trail[(l.trail_head + k) % td->length];
                            if (p.x < min_x) min_x = p.x; if (p.x > max_x) max_x = p.x;
                            if (p.y < min_y) min_y = p.y; if (p.y > max_y) max_y = p.y;
                        }
                        l.aabb_min = Vector2(min_x, min_y);
                        l.aabb_max = Vector2(max_x, max_y);
                    }
                }
            }

            // ----------------------------------------------------------------
            // Collision — CPU player-point test (danmakufu style).
            // Test the tracked player position against each live trail segment
            // using point-to-segment distance. No PhysicsServer2D involvement.
            // ----------------------------------------------------------------
            if (l.trail_count >= 2) {
                float threshold    = td->width * 0.5f + hitbox_r;
                float threshold_sq = threshold * threshold;
                // AABB early-out: skip all segment tests when the player is
                // clearly outside the laser's bounding box.
                if (player.x >= l.aabb_min.x - threshold && player.x <= l.aabb_max.x + threshold &&
                    player.y >= l.aabb_min.y - threshold && player.y <= l.aabb_max.y + threshold)
                {
                    for (int k = 0; k < l.trail_count - 1; ++k) {
                        Vector2 A  = l.trail[(l.trail_head + k)     % td->length];
                        Vector2 B  = l.trail[(l.trail_head + k + 1) % td->length];
                        Vector2 AB = B - A;
                        float ab_sq = AB.length_squared();
                        if (ab_sq < 0.0001f) continue;
                        float t = (player - A).dot(AB) / ab_sq;
                        if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
                        if ((player - (A + AB * t)).length_squared() < threshold_sq) {
                            emit_signal("laser_hit", l.wrapper);
                            break;
                        }
                    }
                }
            }

            // Advance texture animation
            if (td->texture_frames.size() > 1) {
                l.tex_anim_time += delta;
                while (l.tex_anim_time >= td->texture_frames[l.tex_anim_frame].duration_sec) {
                    l.tex_anim_time -= td->texture_frames[l.tex_anim_frame].duration_sec;
                    l.tex_anim_frame = (l.tex_anim_frame + 1) % (int)td->texture_frames.size();
                }
            }
        }
    }

    bool had_recycles = !_to_recycle.empty();
    for (auto *l : _to_recycle) _recycle_internal(l);
    _to_recycle.clear();

    if (!_active.empty() || had_recycles) queue_redraw();
}

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------

// Per-vertex alpha: linear fade-in from tip and fade-out toward tail.
// idx=0 is the tip, idx=trail_count-1 is the tail.
static float _trail_alpha(int idx, int trail_count, float opacity) {
    int taper = (trail_count / 5 < 2) ? 2 : trail_count / 5;
    float tip_alpha  = (float)(idx + 1)                    / (float)taper;
    float tail_alpha = (float)(trail_count - 1 - idx + 1)  / (float)taper;
    if (tip_alpha  > 1.0f) tip_alpha  = 1.0f;
    if (tail_alpha > 1.0f) tail_alpha = 1.0f;
    return tip_alpha * tail_alpha * opacity;
}

// ---------------------------------------------------------------------------
// Draw — per-type batched via canvas_item_add_triangle_array
//
// Each TypeData has its own DrawBucket map (keyed by texture RID) and its own
// Node2D child (draw_node) which carries the CanvasItemMaterial for blend mode.
// Lasers sharing a texture within a type go into one bucket → one draw call.
// ---------------------------------------------------------------------------

void TamaServerCurvedLaserPool::_draw() {
    RenderingServer *rs = RenderingServer::get_singleton();

    for (auto &[key, td_iter] : _types) {
        TypeData *td = td_iter;

        // Clear this type's bucket contents but retain allocated capacity.
        for (auto &[bkey, bucket] : td->draw_buckets) {
            bucket.points.clear();
            bucket.colors.clear();
            bucket.uvs.clear();
            bucket.indices.clear();
        }

        // Accumulate geometry for every active laser of this type.
        for (int i = 0; i < (int)td->lasers.size(); ++i) {
            CurvedLaserState &l = td->lasers[i];
            if (!l.active || l.trail_count < 2) continue;

            RID tex_rid;
            if (!td->texture_frames.empty()) {
                Ref<Texture2D> tex = (td->texture_frames.size() > 1)
                    ? td->texture_frames[l.tex_anim_frame].texture
                    : td->texture_frames[0].texture;
                if (tex.is_valid()) tex_rid = tex->get_rid();
            }

            int64_t tex_key = tex_rid.get_id();
            DrawBucket &bucket = td->draw_buckets[tex_key];
            bucket.texture_rid = tex_rid;

            int count     = l.trail_count;
            int n         = count - 1;
            int vert_base = (int)bucket.points.size();

            for (int k = 0; k < count; ++k) {
                int   slot = (l.trail_head + k) % td->length;
                float v    = (n > 0) ? ((float)k / (float)n) : 0.0f;
                float a    = _trail_alpha(k, count, 1.0f);
                Color c(1.0f, 1.0f, 1.0f, a);
                bucket.points.push_back(l.edge_l[slot]);
                bucket.points.push_back(l.edge_r[slot]);
                bucket.colors.push_back(c);
                bucket.colors.push_back(c);
                bucket.uvs.push_back(Vector2(0.0f, v));
                bucket.uvs.push_back(Vector2(1.0f, v));
            }

            for (int s = 0; s < n; ++s) {
                Vector2 A = l.trail[(l.trail_head + s)     % td->length];
                Vector2 B = l.trail[(l.trail_head + s + 1) % td->length];
                if ((B - A).length_squared() < 0.0001f) continue;

                int lA = vert_base + s * 2;
                int rA = vert_base + s * 2 + 1;
                int lB = vert_base + (s + 1) * 2;
                int rB = vert_base + (s + 1) * 2 + 1;

                bucket.indices.push_back(lA);
                bucket.indices.push_back(rA);
                bucket.indices.push_back(rB);

                bucket.indices.push_back(rB);
                bucket.indices.push_back(lB);
                bucket.indices.push_back(lA);
            }
        }

        // Flush: one draw call per texture bucket, onto this type's draw_node
        // canvas item (which has the CanvasItemMaterial for blend mode).
        RID canvas_rid = td->draw_node->get_canvas_item();
        rs->canvas_item_clear(canvas_rid);

        for (auto &[bkey, bucket] : td->draw_buckets) {
            if (bucket.indices.empty()) continue;

            int nv = (int)bucket.points.size();
            int ni = (int)bucket.indices.size();

            _flush_pts.resize(nv);
            _flush_col.resize(nv);
            _flush_uv.resize(nv);
            _flush_idx.resize(ni);

            {
                Vector2 *p = _flush_pts.ptrw();
                for (int j = 0; j < nv; ++j) p[j] = bucket.points[j];
            }
            {
                Color *p = _flush_col.ptrw();
                for (int j = 0; j < nv; ++j) p[j] = bucket.colors[j];
            }
            {
                Vector2 *p = _flush_uv.ptrw();
                for (int j = 0; j < nv; ++j) p[j] = bucket.uvs[j];
            }
            {
                int32_t *p = _flush_idx.ptrw();
                for (int j = 0; j < ni; ++j) p[j] = bucket.indices[j];
            }

            rs->canvas_item_add_triangle_array(
                canvas_rid,
                _flush_idx,
                _flush_pts,
                _flush_col,
                _flush_uv,
                PackedInt32Array(),    // bones  (unused)
                PackedFloat32Array(),  // weights (unused)
                bucket.texture_rid
            );
        }
    }
}

// ---------------------------------------------------------------------------
// World bounds helper
// ---------------------------------------------------------------------------

godot::Rect2 TamaServerCurvedLaserPool::_world_bounds() const {
    Viewport *vp = get_viewport();
    if (!vp) return Rect2(-10000, -10000, 20000, 20000);
    Transform2D inv = vp->get_canvas_transform().affine_inverse();
    Rect2 vp_rect   = vp->get_visible_rect();
    Vector2 tl = inv.xform(vp_rect.position);
    Vector2 br = inv.xform(vp_rect.get_end());
    return Rect2(
        std::min(tl.x, br.x), std::min(tl.y, br.y),
        std::abs(br.x - tl.x), std::abs(br.y - tl.y));
}
