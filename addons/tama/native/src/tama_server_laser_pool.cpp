#include "tama_server_laser_pool.h"
#include "tama_interpreter.h"
#include "tama_manager.h"
#include "tama_server_straight_laser_config.h"

#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/world2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>
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
        PropertyInfo(Variant::OBJECT, "laser", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "TamaServerLaser")));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

TamaServerLaserPool::~TamaServerLaserPool() {
    for (auto &[key, td] : _types) {
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
    td->tile_x          = (bool) p_config->get("tile_x");
    td->tile_y          = (bool) p_config->get("tile_y");
    td->delay_frames    = (int)  p_config->get("delay_frames");
    td->expand_frames   = (int)  p_config->get("expand_frames");
    td->duration_frames = (int)  p_config->get("duration_frames");
    td->fade_frames     = (int)  p_config->get("fade_frames");
    td->pool_size       = (int)  p_config->get("pool_size");
    {
        auto load_anim = [](Object *cfg, const char *prop) -> std::vector<TamaAnimFrame> {
            Variant v = cfg->get(prop);
            if (v.get_type() != Variant::OBJECT) return {};
            TamaAnimatedTexture *anim = Object::cast_to<TamaAnimatedTexture>(v.operator Object *());
            return anim ? anim->build_anim_frames() : std::vector<TamaAnimFrame>{};
        };
        td->texture_frames      = load_anim(p_config, "texture");
        td->base_texture_frames = load_anim(p_config, "base_texture");
    }

    int n = td->pool_size;
    _types[key] = td;

    // Pre-allocate laser states and wrapper objects
    td->lasers.resize(n);
    td->wrappers.resize(n);
    for (int i = 0; i < n; ++i) {
        td->lasers[i].global_slot = i;
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
    l.tex_anim_frame  = 0;
    l.tex_anim_time   = 0.0f;
    l.base_anim_frame = 0;
    l.base_anim_time  = 0.0f;

    // If delay is zero, jump straight to expand phase immediately
    if (td.delay_frames == 0) {
        l.phase = 1;
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

    // Return slot to FIFO ring — find TypeData by pointer match
    for (auto &[key, td] : _types) {
        if (l->global_slot >= 0 && l->global_slot < (int)td->lasers.size() &&
            &td->lasers[l->global_slot] == l)
        {
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
    TamaManager *mgr = TamaManager::get_instance();
    Vector2 player   = mgr ? mgr->get_player_position()      : Vector2();
    float   hitbox_r = mgr ? mgr->get_player_hitbox_radius()  : 3.0f;

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
                case 0: { // delay — thin line, no collision
                    l.current_width   = 1.0f;
                    l.current_opacity = 1.0f;
                    ++l.frame_counter;
                    if (l.frame_counter >= td->delay_frames) {
                        l.phase = 1;
                        l.frame_counter = 0;
                    }
                    break;
                }
                case 1: { // expand — width grows, collision active
                    float t = (td->expand_frames > 0)
                        ? std::min((float)l.frame_counter / (float)td->expand_frames, 1.0f)
                        : 1.0f;
                    l.current_width   = 1.0f + (td->width - 1.0f) * t;
                    l.current_opacity = 1.0f;
                    ++l.frame_counter;
                    if (l.frame_counter >= td->expand_frames) {
                        l.phase = 2;
                        l.frame_counter = 0;
                        l.current_width = td->width;
                    }
                    break;
                }
                case 2: { // active — full width, collision active
                    l.current_width   = td->width;
                    l.current_opacity = 1.0f;
                    ++l.frame_counter;
                    if (l.frame_counter >= td->duration_frames) {
                        l.phase = 3;
                        l.frame_counter = 0;
                    }
                    break;
                }
                case 3: { // fade — opacity decreases, no collision
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

            // CPU collision — point-to-segment test with AABB early-out
            if (l.phase == 1 || l.phase == 2) {
                float threshold = l.current_width * 0.5f + hitbox_r;
                Vector2 dir(std::cos(l.angle), std::sin(l.angle));
                Vector2 end = l.position + dir * td->length;
                float ax = l.position.x < end.x ? l.position.x : end.x;
                float bx = l.position.x < end.x ? end.x : l.position.x;
                float ay = l.position.y < end.y ? l.position.y : end.y;
                float by = l.position.y < end.y ? end.y : l.position.y;
                if (player.x >= ax - threshold && player.x <= bx + threshold &&
                    player.y >= ay - threshold && player.y <= by + threshold)
                {
                    Vector2 AB = end - l.position;
                    float ab_sq = AB.length_squared();
                    if (ab_sq > 0.0001f) {
                        float t = (player - l.position).dot(AB) / ab_sq;
                        if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
                        if ((player - (l.position + AB * t)).length_squared() < threshold * threshold)
                            emit_signal("laser_hit", l.wrapper);
                    }
                }
            }

            // Advance per-laser texture animations
            if (td->texture_frames.size() > 1) {
                l.tex_anim_time += delta;
                while (l.tex_anim_time >= td->texture_frames[l.tex_anim_frame].duration_sec) {
                    l.tex_anim_time -= td->texture_frames[l.tex_anim_frame].duration_sec;
                    l.tex_anim_frame = (l.tex_anim_frame + 1) % (int)td->texture_frames.size();
                }
            }
            if (td->base_texture_frames.size() > 1) {
                l.base_anim_time += delta;
                while (l.base_anim_time >= td->base_texture_frames[l.base_anim_frame].duration_sec) {
                    l.base_anim_time -= td->base_texture_frames[l.base_anim_frame].duration_sec;
                    l.base_anim_frame = (l.base_anim_frame + 1) % (int)td->base_texture_frames.size();
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

            Ref<Texture2D> tex = td->texture_frames.empty()
                ? Ref<Texture2D>()
                : td->texture_frames[l.tex_anim_frame].texture;
            Ref<Texture2D> base_tex = td->base_texture_frames.empty()
                ? Ref<Texture2D>()
                : td->base_texture_frames[l.base_anim_frame].texture;

            // All drawing in the laser's local frame: origin at base, +X along the beam
            draw_set_transform(l.position, l.angle, Vector2(1.0f, 1.0f));

            // Laser body — 4 tiling modes
            const Rect2 body_rect(0.0f, -half_w, td->length, l.current_width);
            if (tex.is_valid()) {
                if (!td->tile_x && !td->tile_y) {
                    draw_texture_rect(tex, body_rect, false, modulate);
                } else if (td->tile_x && td->tile_y) {
                    draw_texture_rect(tex, body_rect, true, modulate);
                } else if (td->tile_x) {
                    // Tile along length, stretch across width
                    Vector2i tsz = tex->get_size();
                    if (tsz.x > 0) {
                        float x = 0.0f, rem = td->length;
                        while (rem > 0.0f) {
                            float w = std::min((float)tsz.x, rem);
                            draw_texture_rect_region(tex,
                                Rect2(x, -half_w, w, l.current_width),
                                Rect2(0, 0, w, (float)tsz.y), modulate);
                            x   += (float)tsz.x;
                            rem -= (float)tsz.x;
                        }
                    }
                } else {
                    // Stretch along length, tile across width
                    Vector2i tsz = tex->get_size();
                    if (tsz.y > 0) {
                        float y = -half_w, rem = l.current_width;
                        while (rem > 0.0f) {
                            float h = std::min((float)tsz.y, rem);
                            draw_texture_rect_region(tex,
                                Rect2(0.0f, y, td->length, h),
                                Rect2(0, 0, (float)tsz.x, h), modulate);
                            y   += (float)tsz.y;
                            rem -= (float)tsz.y;
                        }
                    }
                }
            } else {
                draw_rect(body_rect, modulate);
            }

            // Base texture centered at origin
            if (base_tex.is_valid()) {
                Vector2i bsz = base_tex->get_size();
                draw_texture_rect(base_tex,
                    Rect2(-bsz.x * 0.5f, -bsz.y * 0.5f, (float)bsz.x, (float)bsz.y),
                    false, modulate);
            }
        }
    }
    draw_set_transform(Vector2(), 0.0f, Vector2(1.0f, 1.0f));
}
