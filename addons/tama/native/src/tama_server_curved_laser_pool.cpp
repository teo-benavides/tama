#include "tama_server_curved_laser_pool.h"
#include "tama_interpreter.h"
#include "tama_manager.h"
#include "tama_server_curved_laser_config.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/physics_server2d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
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
                     PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "TamaServerCurvedLaser"),
        PropertyInfo(Variant::INT, "body_instance_id")));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

TamaServerCurvedLaserPool::~TamaServerCurvedLaserPool() {
    for (auto &[key, td] : _types) {
        PhysicsServer2D *ps = PhysicsServer2D::get_singleton();
        for (auto &shape : td->shapes) ps->free_rid(shape);
        if (td->area != RID()) ps->free_rid(td->area);
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
    td->collision_layer = (int)  p_config->get("collision_layer");
    td->collision_mask  = (int)  p_config->get("collision_mask");
    {
        Variant v = p_config->get("texture");
        if (v.get_type() == Variant::OBJECT) {
            TamaAnimatedTexture *anim = Object::cast_to<TamaAnimatedTexture>(v.operator Object *());
            if (anim) td->texture_frames = anim->build_anim_frames();
        }
    }

    int n     = td->pool_size;
    // One shape per trail buffer slot (indexed by the newer node's slot).
    // The tail slot's shape is always disabled, so length slots cover up to
    // length-1 simultaneously-active segments.
    int slots = td->length;
    _types[key] = td;

    PhysicsServer2D *ps = PhysicsServer2D::get_singleton();
    RID space = get_world_2d()->get_space();
    td->area = ps->area_create();
    ps->area_set_space(td->area, space);
    ps->area_set_collision_layer(td->area, td->collision_layer);
    ps->area_set_collision_mask(td->area, td->collision_mask);
    ps->area_set_monitorable(td->area, false);

    // Pre-allocate n * slots rectangle shapes: index = global_slot * slots + buffer_slot
    td->shapes.reserve(n * slots);
    for (int i = 0; i < n; ++i) {
        for (int s = 0; s < slots; ++s) {
            RID shape = ps->rectangle_shape_create();
            ps->shape_set_data(shape, Vector2(1.0f, 1.0f));
            ps->area_add_shape(td->area, shape);
            ps->area_set_shape_disabled(td->area, i * slots + s, true);
            td->shapes.push_back(shape);
        }
    }

    Callable cb = callable_mp_static(TamaServerCurvedLaserPool::_area_monitor_callback)
                      .bind(this, p_key);
    ps->area_set_monitor_callback(td->area, cb);

    td->lasers.resize(n);
    td->wrappers.resize(n);
    for (int i = 0; i < n; ++i) {
        td->lasers[i].global_slot = i;
        td->lasers[i].area_rid    = td->area;
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
// Collision callback
// ---------------------------------------------------------------------------

void TamaServerCurvedLaserPool::_area_monitor_callback(
        int status, RID /*body_rid*/, int64_t body_iid,
        int /*body_shape*/, int local_shape,
        TamaServerCurvedLaserPool *self, String type_key)
{
    if (status != PhysicsServer2D::AREA_BODY_ADDED) return;
    std::string key = type_key.utf8().get_data();
    auto it = self->_types.find(key);
    if (it == self->_types.end()) return;
    TypeData *td = it->second;
    if (td->length <= 0) return;
    int laser_slot = local_shape / td->length;
    if (laser_slot < 0 || laser_slot >= (int)td->lasers.size()) return;
    CurvedLaserState &l = td->lasers[laser_slot];
    if (!l.active) return;
    self->emit_signal("laser_hit", l.wrapper, body_iid);
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
    l.collision_built        = false;
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

    PhysicsServer2D *ps = PhysicsServer2D::get_singleton();

    // Disable all segment shapes for this laser
    for (auto &[k, td] : _types) {
        if (td->area == l->area_rid) {
            int slots = td->length;
            int base = l->global_slot * slots;
            for (int s = 0; s < slots; ++s)
                ps->area_set_shape_disabled(l->area_rid, base + s, true);
            break;
        }
    }

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

    for (auto &[key, td] : _types) {
        if (td->area == l->area_rid) {
            int n = (int)td->lasers.size();
            td->ring[td->ring_w] = l->global_slot;
            td->ring_w = (td->ring_w + 1) % n;
            ++td->ring_count;
            break;
        }
    }
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
    PhysicsServer2D *ps = PhysicsServer2D::get_singleton();
    Rect2 world = _world_bounds();
    TamaManager *mgr = TamaManager::get_instance();
    float global_margin = mgr ? mgr->get_global_out_of_bounds_margin() : -1.0f;

    _to_recycle.clear();

    for (auto &[key, td_iter] : _types) {
        TypeData *td = td_iter;
        int slots = td->length;

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

            // Push new position to circular trail buffer
            l.trail_head = (l.trail_head - 1 + td->length) % td->length;
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
                    for (int k = 0; k < l.trail_count; ++k)
                        _cache_edge_slot(l, (l.trail_head + k) % td->length, td->length, hw);
                    l.edge_built = true;
                } else {
                    // New head: only has an older neighbour (no newer one yet).
                    _cache_edge_slot(l, l.trail_head, td->length, hw);
                    // Previous head: now has the new head as its newer neighbour.
                    if (l.trail_count >= 2)
                        _cache_edge_slot(l, (l.trail_head + 1) % td->length, td->length, hw);
                }
            }

            // ----------------------------------------------------------------
            // Collision — incremental update.
            //
            // Shapes are indexed by the buffer slot of the newer node of each
            // segment (stable as the trail scrolls). History nodes never move,
            // so each frame only the new head segment is (re)built and the
            // just-scrolled-off tail slot is disabled (~4 calls/laser vs N).
            // ----------------------------------------------------------------
            {
                int base = l.global_slot * slots;

                auto build_segment = [&](int newer_slot, int older_slot) {
                    Vector2 A = l.trail[newer_slot];
                    Vector2 B = l.trail[older_slot];
                    float seg_len   = A.distance_to(B);
                    float seg_angle = std::atan2(B.y - A.y, B.x - A.x);
                    Vector2 half_extents(seg_len * 0.5f + td->width * 0.5f, td->width * 0.5f);
                    int shape_idx = base + newer_slot;
                    ps->shape_set_data(td->shapes[shape_idx], half_extents);
                    ps->area_set_shape_transform(l.area_rid, shape_idx,
                        Transform2D(seg_angle, (A + B) * 0.5f));
                    ps->area_set_shape_disabled(l.area_rid, shape_idx, false);
                };

                if (!l.collision_built) {
                    for (int k = 0; k < l.trail_count - 1; ++k) {
                        int ns = (l.trail_head + k)     % td->length;
                        int os = (l.trail_head + k + 1) % td->length;
                        build_segment(ns, os);
                    }
                    l.collision_built = true;
                } else if (l.trail_count >= 2) {
                    int ns = l.trail_head;
                    int os = (l.trail_head + 1) % td->length;
                    build_segment(ns, os);
                }

                // The oldest node's slot has no older neighbour — disable it.
                // When the buffer is full this is the segment that just scrolled off.
                int tail_slot = (l.trail_head + l.trail_count - 1) % td->length;
                ps->area_set_shape_disabled(l.area_rid, base + tail_slot, true);
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

    for (auto *l : _to_recycle) _recycle_internal(l);
    _to_recycle.clear();

    if (!_active.empty()) queue_redraw();
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
// Draw — batched via canvas_item_add_triangle_array
//
// All active lasers that share a texture are accumulated into a single
// DrawBucket (std::vectors, retained between frames for capacity), then
// flushed with one canvas_item_add_triangle_array call per texture.
// This collapses the previous O(lasers × segments) draw_primitive calls
// down to O(distinct textures) — typically 1.
// ---------------------------------------------------------------------------

void TamaServerCurvedLaserPool::_draw() {
    // Clear bucket contents but retain allocated capacity
    for (auto &[bkey, bucket] : _draw_buckets) {
        bucket.points.clear();
        bucket.colors.clear();
        bucket.uvs.clear();
        bucket.indices.clear();
    }

    for (auto &[key, td_iter] : _types) {
        TypeData *td = td_iter;

        for (int i = 0; i < (int)td->lasers.size(); ++i) {
            CurvedLaserState &l = td->lasers[i];
            if (!l.active || l.trail_count < 2) continue;

            // Resolve texture RID for this laser this frame
            RID tex_rid;
            if (!td->texture_frames.empty()) {
                Ref<Texture2D> tex = (td->texture_frames.size() > 1)
                    ? td->texture_frames[l.tex_anim_frame].texture
                    : td->texture_frames[0].texture;
                if (tex.is_valid()) tex_rid = tex->get_rid();
            }

            // Get or create bucket keyed by texture RID integer
            int64_t tex_key = tex_rid.get_id();
            DrawBucket &bucket = _draw_buckets[tex_key];
            bucket.texture_rid = tex_rid;

            int count     = l.trail_count;
            int n         = count - 1;
            int vert_base = (int)bucket.points.size();  // offset for this laser's verts

            // ------------------------------------------------------------------
            // Append 2 vertices per trail node into the bucket.
            // Edge vertices come from the physics-updated cache (edge_l/edge_r,
            // indexed by buffer slot) — no per-frame sqrt/normalize here.
            // Layout: node k → left  = vert_base + k*2
            //                   right = vert_base + k*2 + 1
            // ------------------------------------------------------------------
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

            // ------------------------------------------------------------------
            // Append 6 indices per non-degenerate segment (2 CCW triangles).
            // Winding matches the previous draw_primitive quad order.
            // ------------------------------------------------------------------
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
    }

    // ------------------------------------------------------------------
    // Flush: one canvas_item_add_triangle_array call per texture bucket
    // ------------------------------------------------------------------
    RID           canvas_rid = get_canvas_item();
    RenderingServer *rs      = RenderingServer::get_singleton();

    for (auto &[bkey, bucket] : _draw_buckets) {
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
