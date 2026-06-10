#include "tama_server_laser_pool.h"
#include "tama_interpreter.h"
#include "tama_manager.h"
#include "tama_server_straight_laser_config.h"

#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/physics_server2d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/world2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>

using namespace godot;

// ---------------------------------------------------------------------------
// Bind
// ---------------------------------------------------------------------------

void TamaServerLaserPool::_bind_methods() {
    ClassDB::bind_method(D_METHOD("recycle", "laser_wrapper"), &TamaServerLaserPool::recycle);
    ClassDB::bind_method(D_METHOD("recycle_all"),              &TamaServerLaserPool::recycle_all);
    ClassDB::bind_method(D_METHOD("get_active_count"),         &TamaServerLaserPool::get_active_count);

    ADD_SIGNAL(MethodInfo("laser_hit",
        PropertyInfo(Variant::OBJECT, "laser", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "TamaServerLaser"),
        PropertyInfo(Variant::INT, "body_instance_id")));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

TamaServerLaserPool::~TamaServerLaserPool() {
    for (auto &[key, td] : _types) {
        PhysicsServer2D *ps = PhysicsServer2D::get_singleton();
        for (auto &shape : td->shapes) ps->free_rid(shape);
        if (td->area != RID()) ps->free_rid(td->area);
        for (auto *w : td->wrappers) memdelete(w);
        delete td;
    }
}

void TamaServerLaserPool::_ready() {
    set_physics_process(true);
    for (auto &pr : _pending_regs)
        register_type(pr.key, pr.config);
    _pending_regs.clear();
}

void TamaServerLaserPool::_exit_tree() {
    recycle_all();
}

// ---------------------------------------------------------------------------
// Register type
// ---------------------------------------------------------------------------

void TamaServerLaserPool::register_type(const String &p_key, Object *p_config) {
    if (!is_inside_tree()) {
        _pending_regs.push_back({p_key, p_config});
        return;
    }
    std::string key = p_key.utf8().get_data();
    if (_types.count(key)) {
        UtilityFunctions::push_warning("TamaServerLaserPool: type '" + p_key + "' already registered.");
        return;
    }

    TypeData *td    = new TypeData();
    td->config_obj  = p_config;
    td->width           = (float)p_config->get("width");
    td->length          = (float)p_config->get("length");
    td->tile            = (bool) p_config->get("tile");
    td->delay_frames    = (int)  p_config->get("delay_frames");
    td->expand_frames   = (int)  p_config->get("expand_frames");
    td->duration_frames = (int)  p_config->get("duration_frames");
    td->fade_frames     = (int)  p_config->get("fade_frames");
    td->pool_size       = (int)  p_config->get("pool_size");
    td->collision_layer = (int)  p_config->get("collision_layer");
    td->collision_mask  = (int)  p_config->get("collision_mask");
    {
        Variant tv = p_config->get("texture");
        if (tv.get_type() == Variant::OBJECT)
            td->texture = Ref<Texture2D>(Object::cast_to<Texture2D>(tv.operator Object *()));
        Variant btv = p_config->get("base_texture");
        if (btv.get_type() == Variant::OBJECT)
            td->base_texture = Ref<Texture2D>(Object::cast_to<Texture2D>(btv.operator Object *()));
    }

    int n = td->pool_size;
    _types[key] = td;

    // Physics area with N pre-allocated rectangle shapes (all disabled initially)
    RID space = get_world_2d()->get_space();
    td->area = PhysicsServer2D::get_singleton()->area_create();
    PhysicsServer2D::get_singleton()->area_set_space(td->area, space);
    PhysicsServer2D::get_singleton()->area_set_collision_layer(td->area, td->collision_layer);
    PhysicsServer2D::get_singleton()->area_set_collision_mask(td->area, td->collision_mask);
    PhysicsServer2D::get_singleton()->area_set_monitorable(td->area, false);

    // Rectangle shape half-extents: width × length
    Vector2 half_extents(td->length / 2.0f, td->width / 2.0f);

    td->shapes.reserve(n);
    for (int i = 0; i < n; ++i) {
        RID shape = PhysicsServer2D::get_singleton()->rectangle_shape_create();
        PhysicsServer2D::get_singleton()->shape_set_data(shape, half_extents);
        PhysicsServer2D::get_singleton()->area_add_shape(td->area, shape);
        PhysicsServer2D::get_singleton()->area_set_shape_disabled(td->area, i, true);
        td->shapes.push_back(shape);
    }

    Callable cb = callable_mp_static(TamaServerLaserPool::_area_monitor_callback)
                      .bind(this, p_key);
    PhysicsServer2D::get_singleton()->area_set_monitor_callback(td->area, cb);

    // Pre-allocate laser states and wrapper objects
    td->lasers.resize(n);
    td->wrappers.resize(n);
    for (int i = 0; i < n; ++i) {
        td->lasers[i].global_slot = i;
        td->lasers[i].area_rid    = td->area;
        TamaServerLaser *w = memnew(TamaServerLaser);
        w->_init_slot(this, &td->lasers[i], key);
        td->wrappers[i] = w;
    }

    // FIFO ring: all slots free initially
    td->ring.resize(n);
    for (int i = 0; i < n; ++i) td->ring[i] = i;
    td->ring_r     = 0;
    td->ring_w     = 0;
    td->ring_count = n;
}

// ---------------------------------------------------------------------------
// Collision callback
// ---------------------------------------------------------------------------

void TamaServerLaserPool::_area_monitor_callback(
        int status, RID /*body_rid*/, int64_t body_iid,
        int /*body_shape*/, int local_shape,
        TamaServerLaserPool *self, String type_key)
{
    if (status != PhysicsServer2D::AREA_BODY_ADDED) return;
    std::string key = type_key.utf8().get_data();
    auto it = self->_types.find(key);
    if (it == self->_types.end()) return;
    TypeData *td = it->second;
    if (local_shape < 0 || local_shape >= (int)td->lasers.size()) return;
    LaserState &l = td->lasers[local_shape];
    if (!l.active) return;
    self->emit_signal("laser_hit", l.wrapper, body_iid);
}

// ---------------------------------------------------------------------------
// Spawn
// ---------------------------------------------------------------------------

Object *TamaServerLaserPool::spawn(
        const TamaBulletFireData &p_data, Object *p_config,
        float angle, Vector2 position,
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

    LaserState &l    = td.lasers[global_slot];
    l.active         = true;
    l.spawn_frame    = frame;
    l.active_idx     = (int32_t)_active.size();
    _active.push_back(&l);

    l.position     = position;
    l.angle        = angle;
    l.last_angle   = angle;
    l.phase        = 0;
    l.frame_counter = 0;
    l.current_width   = 1.0f;
    l.current_opacity = 1.0f;
    l.angle_tween.active = false;

    // If delay is zero, jump straight to expand and activate hitbox immediately
    if (td.delay_frames == 0) {
        l.phase = 1;
        PhysicsServer2D::get_singleton()->area_set_shape_disabled(td.area, global_slot, false);
        _update_shape_transform(td, l);
    }

    // bullet_act runner
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

        TamaServerLaser *wrapper_laser = Object::cast_to<TamaServerLaser>(l.wrapper);
        runner->set_event_handler(wrapper_laser);

        TamaManager *mgr = TamaManager::get_instance();
        _TamaSpawnManager *sm = mgr ? mgr->_get_spawn_manager() : nullptr;
        if (sm) sm->connect_interpreter(runner, l.wrapper);

        runner->start_act(p_data.source_program, p_data.bullet_act, std::move(act_scope));
    }

    queue_redraw();
    return l.wrapper;
}

// Helper — computes and applies the rectangle shape transform for expand/active phases
void TamaServerLaserPool::_update_shape_transform(TypeData &td, LaserState &l) {
    float hlen = td.length / 2.0f;
    Vector2 center = l.position + Vector2(std::cos(l.angle), std::sin(l.angle)) * hlen;
    PhysicsServer2D::get_singleton()->area_set_shape_transform(
        td.area, l.global_slot, Transform2D(l.angle, center));
}

// ---------------------------------------------------------------------------
// Recycle
// ---------------------------------------------------------------------------

void TamaServerLaserPool::recycle(Object *p_wrapper) {
    if (!p_wrapper) return;
    TamaServerLaser *w = Object::cast_to<TamaServerLaser>(p_wrapper);
    if (!w || !w->_state) return;
    _recycle_internal(w->_state);
}

void TamaServerLaserPool::recycle_all() {
    std::vector<LaserState *> copy = _active;
    for (auto *l : copy) _recycle_internal(l);
    queue_redraw();
}

void TamaServerLaserPool::_recycle_internal(LaserState *l) {
    if (!l || !l->active) return;
    l->active = false;

    // Disable physics shape
    PhysicsServer2D::get_singleton()->area_set_shape_disabled(l->area_rid, l->global_slot, true);

    // Remove from _active via swap-erase
    int idx = l->active_idx;
    int last = (int)_active.size() - 1;
    if (idx != last) {
        _active[idx] = _active[last];
        _active[idx]->active_idx = idx;
    }
    _active.pop_back();
    l->active_idx = -1;

    // Clean up runner
    if (l->runner) {
        delete l->runner;
        l->runner = nullptr;
    }

    // Return slot to FIFO ring — find TypeData by area_rid
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
// Physics process
// ---------------------------------------------------------------------------

void TamaServerLaserPool::_physics_process(double p_delta) {
    float delta = (float)p_delta;
    int64_t cur_frame = Engine::get_singleton()->get_physics_frames();

    _to_recycle.clear();

    for (auto &[key, td_iter] : _types) {
        TypeData *td = td_iter;
        for (int i = 0; i < (int)td->lasers.size(); ++i) {
            LaserState &l = td->lasers[i];
            if (!l.active) continue;
            if (l.spawn_frame == cur_frame) continue;

            // Step runner so scripts can reposition the laser before we update physics
            if (l.runner && l.runner->is_running()) l.runner->step(delta);
            if (!l.active) continue; // vanished via runner

            // Step angle tween
            if (l.angle_tween.active) l.angle = l.angle_tween.step(delta);

            switch (l.phase) {
                case 0: { // delay — thin line, no hitbox
                    l.current_width   = 1.0f;
                    l.current_opacity = 1.0f;
                    ++l.frame_counter;
                    if (l.frame_counter >= td->delay_frames) {
                        l.phase = 1;
                        l.frame_counter = 0;
                        PhysicsServer2D::get_singleton()->area_set_shape_disabled(td->area, l.global_slot, false);
                        _update_shape_transform(*td, l);
                    }
                    break;
                }
                case 1: { // expand — width grows, hitbox active
                    float t = (td->expand_frames > 0)
                        ? std::min((float)l.frame_counter / (float)td->expand_frames, 1.0f)
                        : 1.0f;
                    l.current_width   = 1.0f + (td->width - 1.0f) * t;
                    l.current_opacity = 1.0f;
                    _update_shape_transform(*td, l);
                    ++l.frame_counter;
                    if (l.frame_counter >= td->expand_frames) {
                        l.phase = 2;
                        l.frame_counter = 0;
                        l.current_width = td->width;
                    }
                    break;
                }
                case 2: { // active — full width, hitbox active
                    l.current_width   = td->width;
                    l.current_opacity = 1.0f;
                    _update_shape_transform(*td, l);
                    ++l.frame_counter;
                    if (l.frame_counter >= td->duration_frames) {
                        l.phase = 3;
                        l.frame_counter = 0;
                        PhysicsServer2D::get_singleton()->area_set_shape_disabled(td->area, l.global_slot, true);
                    }
                    break;
                }
                case 3: { // fade — opacity decreases, no hitbox
                    float t = (td->fade_frames > 0)
                        ? std::min((float)l.frame_counter / (float)td->fade_frames, 1.0f)
                        : 1.0f;
                    l.current_opacity = 1.0f - t;
                    ++l.frame_counter;
                    if (l.frame_counter >= td->fade_frames) {
                        _to_recycle.push_back(&l);
                    }
                    break;
                }
            }
        }
    }

    for (auto *l : _to_recycle) _recycle_internal(l);
    _to_recycle.clear();

    if (!_active.empty()) queue_redraw();
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void TamaServerLaserPool::_draw() {
    for (auto &[key, td_iter] : _types) {
        TypeData *td = td_iter;
        for (int i = 0; i < (int)td->lasers.size(); ++i) {
            LaserState &l = td->lasers[i];
            if (!l.active) continue;

            Color modulate(1.0f, 1.0f, 1.0f, l.current_opacity);
            float half_w = l.current_width * 0.5f;

            // All drawing in the laser's local frame: origin at base, +X along the beam
            draw_set_transform(l.position, l.angle, Vector2(1.0f, 1.0f));

            // Laser body
            if (td->texture.is_valid()) {
                draw_texture_rect(td->texture,
                    Rect2(0.0f, -half_w, td->length, l.current_width),
                    td->tile, modulate);
            } else {
                draw_rect(Rect2(0.0f, -half_w, td->length, l.current_width), modulate);
            }

            // Base texture centered at origin
            if (td->base_texture.is_valid()) {
                Vector2i bsz = td->base_texture->get_size();
                draw_texture_rect(td->base_texture,
                    Rect2(-bsz.x * 0.5f, -bsz.y * 0.5f, (float)bsz.x, (float)bsz.y),
                    false, modulate);
            }
        }
    }
    draw_set_transform(Vector2(), 0.0f, Vector2(1.0f, 1.0f));
}
