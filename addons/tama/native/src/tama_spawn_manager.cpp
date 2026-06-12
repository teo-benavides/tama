#include "tama_spawn_manager.h"
#include "tama_emitter.h"
#include "tama_manager.h"
#include "tama_server_curved_laser.h"
#include "tama_server_curved_laser_config.h"
#include "tama_server_straight_laser_config.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// DirType: AIM=0 ABS=1 REL=2 SEQ=3   ValueType: ABS=0 REL=1 SEQ=2
// OffsetMode: NONE=0 INLINE=1 BLOCK=2

void _TamaSpawnManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_registry"),     &_TamaSpawnManager::get_registry);
    ClassDB::bind_method(D_METHOD("set_registry","v"), &_TamaSpawnManager::set_registry);
    ClassDB::bind_method(D_METHOD("get_context"),      &_TamaSpawnManager::get_context);
    ClassDB::bind_method(D_METHOD("set_context","v"),  &_TamaSpawnManager::set_context);
    ClassDB::bind_method(D_METHOD("get_player_position"),    &_TamaSpawnManager::get_player_position);
    ClassDB::bind_method(D_METHOD("set_player_position","v"),&_TamaSpawnManager::set_player_position);
    ClassDB::bind_method(D_METHOD("get_spawn_parent"),       &_TamaSpawnManager::get_spawn_parent);
    ClassDB::bind_method(D_METHOD("set_spawn_parent","v"),   &_TamaSpawnManager::set_spawn_parent);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT,"registry",   PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),"set_registry","get_registry");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT,"context",    PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),"set_context","get_context");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"player_position",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),"set_player_position","get_player_position");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH,"spawn_parent",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),"set_spawn_parent","get_spawn_parent");
}

void _TamaSpawnManager::_exit_tree() {
    TamaManager *mgr = TamaManager::get_instance();
    if (mgr) mgr->_on_scene_nodes_freed();
}

void _TamaSpawnManager::connect_interpreter(_TamaInterpreter *interp, Object *spawner) {
    if (!interp) return;
    interp->_fire_cb = [this, spawner](const TamaBulletFireData &data) {
        _on_bullet_fired(data, spawner);
    };
    interp->_event_cb = [](const std::string &name, const godot::Array &args) {
        if (TamaManager *mgr = TamaManager::get_instance())
            mgr->emit_signal("event_fired", godot::String(name.c_str()), args);
    };
}

_TamaASTNode *_TamaSpawnManager::get_tama_script(const String &filename) const {
    if (_repository) return _repository->get_tama_script(filename);
    return nullptr;
}

int _TamaSpawnManager::get_scene_bullet_count() const {
    SceneTree *tree = get_tree();
    return tree ? tree->get_nodes_in_group("tama_bullets").size() : 0;
}

// ===========================================================================
// Spawner accessors — direct C++ member access, no Godot property-system overhead.
// All helpers accept Object* so they work for Node2D spawners (TamaEmitter /
// TamaBullet) and for TamaServerBullet wrappers equally.
// ===========================================================================

static godot::Vector2 spawner_get_global_position(godot::Object *s) {
    if (auto *sb = godot::Object::cast_to<TamaServerBullet>(s))
        return sb->_state ? sb->_state->position : godot::Vector2();
    if (auto *cl = godot::Object::cast_to<TamaServerCurvedLaser>(s))
        return cl->_state ? cl->_state->position : godot::Vector2();
    if (auto *sl = godot::Object::cast_to<TamaServerLaser>(s))
        return sl->_state ? sl->_state->position : godot::Vector2();
    if (auto *n = godot::Object::cast_to<godot::Node2D>(s)) return n->get_global_position();
    return godot::Vector2();
}
static float spawner_get_last_angle(godot::Object *s) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))   return e->_last_angle;
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))    return b->_last_angle;
    if (auto *sb = godot::Object::cast_to<TamaServerBullet>(s))
        return sb->_state ? sb->_state->last_angle : 0.0f;
    if (auto *cl = godot::Object::cast_to<TamaServerCurvedLaser>(s))
        return cl->_state ? cl->_state->last_angle : 0.0f;
    if (auto *sl = godot::Object::cast_to<TamaServerLaser>(s))
        return sl->_state ? sl->_state->last_angle : 0.0f;
    return 0.0f;
}
static void spawner_set_last_angle(godot::Object *s, float v) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))   { e->_last_angle = v; return; }
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))    { b->_last_angle = v; return; }
    if (auto *sb = godot::Object::cast_to<TamaServerBullet>(s))
        { if (sb->_state) sb->_state->last_angle = v; return; }
    if (auto *cl = godot::Object::cast_to<TamaServerCurvedLaser>(s))
        { if (cl->_state) cl->_state->last_angle = v; return; }
    if (auto *sl = godot::Object::cast_to<TamaServerLaser>(s))
        { if (sl->_state) sl->_state->last_angle = v; }
}
static float spawner_get_last_speed(godot::Object *s) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))   return e->_last_speed;
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))    return b->_last_speed;
    if (auto *sb = godot::Object::cast_to<TamaServerBullet>(s))
        return sb->_state ? sb->_state->last_speed : 0.0f;
    if (auto *cl = godot::Object::cast_to<TamaServerCurvedLaser>(s))
        return cl->_state ? cl->_state->last_speed : 0.0f;
    return 0.0f;
}
static void spawner_set_last_speed(godot::Object *s, float v) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))   { e->_last_speed = v; return; }
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))    { b->_last_speed = v; return; }
    if (auto *sb = godot::Object::cast_to<TamaServerBullet>(s))
        { if (sb->_state) sb->_state->last_speed = v; return; }
    if (auto *cl = godot::Object::cast_to<TamaServerCurvedLaser>(s))
        { if (cl->_state) cl->_state->last_speed = v; }
}
static float spawner_get_last_rot_speed(godot::Object *s) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))   return e->_last_rot_speed;
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))    return b->_last_rot_speed;
    if (auto *sb = godot::Object::cast_to<TamaServerBullet>(s))
        return sb->_state ? sb->_state->last_rot_speed : 0.0f;
    if (auto *cl = godot::Object::cast_to<TamaServerCurvedLaser>(s))
        return cl->_state ? cl->_state->last_rot_speed : 0.0f;
    return 0.0f;
}
static void spawner_set_last_rot_speed(godot::Object *s, float v) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))   { e->_last_rot_speed = v; return; }
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))    { b->_last_rot_speed = v; return; }
    if (auto *sb = godot::Object::cast_to<TamaServerBullet>(s))
        { if (sb->_state) sb->_state->last_rot_speed = v; return; }
    if (auto *cl = godot::Object::cast_to<TamaServerCurvedLaser>(s))
        { if (cl->_state) cl->_state->last_rot_speed = v; }
}
static float spawner_get_angle(godot::Object *s) {
    if (auto *b  = godot::Object::cast_to<TamaBullet>(s))       return b->_angle;
    if (auto *sb = godot::Object::cast_to<TamaServerBullet>(s))
        return sb->_state ? sb->_state->angle : 0.0f;
    if (auto *cl = godot::Object::cast_to<TamaServerCurvedLaser>(s))
        return cl->_state ? cl->_state->angle : 0.0f;
    if (auto *sl = godot::Object::cast_to<TamaServerLaser>(s))
        return sl->_state ? sl->_state->angle : 0.0f;
    if (auto *n  = godot::Object::cast_to<godot::Node2D>(s))    return n->get_rotation();
    return 0.0f;
}

// ===========================================================================
// Bullet firing
// ===========================================================================

void _TamaSpawnManager::_on_bullet_fired(const TamaBulletFireData &data, Object *spawner) {
    // Resolve bullet_type (substitute from params if it matches a param name)
    String bullet_type = String(data.bullet_type.c_str());
    const std::vector<std::string> &bullet_params = data.bullet_params;
    const std::vector<TamaArgVal>  &bullet_args   = data.bullet_args;
    if (!bullet_type.is_empty() && bullet_params.size() > 0) {
        for (int i = 0; i < (int)std::min(bullet_params.size(), bullet_args.size()); ++i) {
            const TamaArgVal &av = bullet_args[i];
            if (String(bullet_params[i].c_str()) == bullet_type) {
                if (!av.is_node && av.var.get_type() == Variant::STRING)
                    bullet_type = (String)av.var;
                break;
            }
        }
    }

    // Fast path: server object (bullet or laser)
    if (registry.is_valid()) {
        TamaServerObjectConfig *obj_cfg = registry->get_object_config(bullet_type);

        // Fall back to default server bullet when type is unknown and not in scene_bullets
        TamaServerBulletConfig *default_bullet = nullptr;
        if (!obj_cfg && registry->default_to_server_bullets && registry->default_server_bullet.is_valid()) {
            bool in_scene = !bullet_type.is_empty() && registry->scene_bullets.has(bullet_type);
            if (!in_scene)
                default_bullet = registry->default_server_bullet.ptr();
        }

        // Straight laser path
        if (_laser_pool) {
            TamaServerStraightLaserConfig *laser_cfg =
                Object::cast_to<TamaServerStraightLaserConfig>(obj_cfg);
            if (laser_cfg) {
                float angle = _resolve_angle(data, spawner);
                Vector2 pos = _resolve_position(data, spawner, angle);
                spawner_set_last_angle(spawner, angle);
                _laser_pool->spawn(data, laser_cfg, angle, pos, context.ptr());
                return;
            }
        }

        // Curved laser path
        if (_curved_laser_pool) {
            TamaServerCurvedLaserConfig *curved_cfg =
                Object::cast_to<TamaServerCurvedLaserConfig>(obj_cfg);
            if (curved_cfg) {
                float angle = _resolve_angle(data, spawner);
                float speed = _resolve_speed(data, spawner);
                Vector2 pos = _resolve_position(data, spawner, angle);
                spawner_set_last_angle(spawner, angle);
                spawner_set_last_speed(spawner, speed);
                auto *wrapper = godot::Object::cast_to<TamaServerCurvedLaser>(
                    _curved_laser_pool->spawn(data, curved_cfg, angle, speed, pos, context.ptr()));
                if (wrapper && wrapper->_state && data.has_rot_speed) {
                    float rot_spd = _resolve_rot_speed(data, spawner);
                    spawner_set_last_rot_speed(spawner, rot_spd);
                    wrapper->_state->rot_speed = rot_spd;
                }
                return;
            }
        }

        // Bullet path
        if (_server_pool) {
            TamaServerBulletConfig *srv_cfg = Object::cast_to<TamaServerBulletConfig>(obj_cfg);
            if (!srv_cfg) srv_cfg = default_bullet;
            if (srv_cfg) {
                float angle = _resolve_angle(data, spawner);
                float speed = _resolve_speed(data, spawner);
                Vector2 pos = _resolve_position(data, spawner, angle);
                spawner_set_last_angle(spawner, angle);
                spawner_set_last_speed(spawner, speed);
                auto *wrapper = godot::Object::cast_to<TamaServerBullet>(
                    _server_pool->spawn(data, srv_cfg, angle, speed, pos, context.ptr()));
                if (wrapper && wrapper->_state && data.has_rot_speed) {
                    float rot_spd = _resolve_rot_speed(data, spawner);
                    spawner_set_last_rot_speed(spawner, rot_spd);
                    wrapper->_state->rot_speed = rot_spd;
                }
                return;
            }
        }
    }

    // Scene bullet path
    Ref<PackedScene> scene;
    if (registry.is_valid()) {
        if (!bullet_type or bullet_type.is_empty()) {
            scene = registry->default_scene_bullet;
        } else {
            Variant sv = registry->scene_bullets.get(bullet_type, Variant());
            scene = Ref<PackedScene>(Object::cast_to<PackedScene>(sv.operator Object *()));
            if (!scene.is_valid()) scene = registry->default_scene_bullet;
        }
    }
    if (!scene.is_valid()) {
        UtilityFunctions::push_error("_TamaSpawnManager: no scene for bullet type '" + bullet_type + "' (no default set)");
        return;
    }

    // Bail out early if we're not in the scene tree yet (e.g. first physics tick
    // fires before the deferred add_child for this node has executed).
    Node *spawn_parent = _get_spawn_parent();
    if (!spawn_parent) return;

    TamaBullet *bullet = Object::cast_to<TamaBullet>(scene->instantiate());
    if (!bullet) {
        UtilityFunctions::push_error("_TamaSpawnManager: scene for type '" + bullet_type + "' is not a TamaBullet");
        return;
    }

    bullet->_runner = std::make_unique<_TamaInterpreter>();
    bullet->_runner->set_context(context.ptr());
    _TamaInterpreter *bullet_runner = bullet->_runner.get();

    float angle = _resolve_angle(data, spawner);
    float speed = _resolve_speed(data, spawner);
    spawner_set_last_angle(spawner, angle);
    spawner_set_last_speed(spawner, speed);

    bullet->_angle            = angle;
    bullet->_speed            = speed;
    bullet->_rot_speed        = data.has_rot_speed ? _resolve_rot_speed(data, spawner) : 0.0f;
    if (data.has_rot_speed) spawner_set_last_rot_speed(spawner, bullet->_rot_speed);
    bullet->_initial_position = _resolve_position(data, spawner, angle);
    bullet->_bounces_left     = data.bounces_max;
    bullet->_bounces_axis     = data.bounces_axis;

    if (data.mvmt_x_set || data.mvmt_y_set) {
        std::vector<std::string> mvmt_var_names;
        std::vector<double> mvmt_var_values;
        for (int i = 0; i < (int)std::min(bullet_params.size(), bullet_args.size()); ++i) {
            const TamaArgVal &av = bullet_args[i];
            if (!av.is_node) {
                mvmt_var_names.push_back(bullet_params[i]);
                mvmt_var_values.push_back((double)(float)av.var);
            }
        }
        mvmt_var_names.push_back("spawn_x");
        mvmt_var_values.push_back((double)bullet->_initial_position.x);
        mvmt_var_names.push_back("spawn_y");
        mvmt_var_values.push_back((double)bullet->_initial_position.y);
        bullet->_mvmt_x_set      = data.mvmt_x_set;
        bullet->_mvmt_x_type     = data.mvmt_x_type;
        bullet->_mvmt_x_expr     = data.mvmt_x_expr;
        bullet->_mvmt_y_set      = data.mvmt_y_set;
        bullet->_mvmt_y_type     = data.mvmt_y_type;
        bullet->_mvmt_y_expr     = data.mvmt_y_expr;
        bullet->_mvmt_var_names  = std::move(mvmt_var_names);
        bullet->_mvmt_var_values = std::move(mvmt_var_values);
    }

    spawn_parent->call_deferred("add_child", Variant(bullet));

    _TamaASTNode *bullet_act         = data.bullet_act;
    _TamaASTNode *bullet_emitter_act = data.bullet_emitter_act;
    _TamaASTNode *source_program     = data.source_program;

    if (bullet_act) {
        TamaScope act_scope;
        for (int i = 0; i < (int)std::min(bullet_params.size(), bullet_args.size()); ++i)
            act_scope[bullet_params[i]] = TamaScopeVal(bullet_args[i]);
        act_scope["spawn_x"] = TamaScopeVal(bullet->_initial_position.x);
        act_scope["spawn_y"] = TamaScopeVal(bullet->_initial_position.y);
        connect_interpreter(bullet_runner, bullet);
        bullet_runner->set_event_handler(bullet);
        bullet_runner->start_act(source_program, bullet_act, std::move(act_scope));
    }

    if (bullet_emitter_act) {
        _TamaInterpreter *spawner_runner;
        if (bullet_act) {
            bullet->_runner2 = std::make_unique<_TamaInterpreter>();
            bullet->_runner2->set_context(context.ptr());
            spawner_runner = bullet->_runner2.get();
        } else {
            spawner_runner = bullet_runner;
        }
        connect_interpreter(spawner_runner, bullet);
        spawner_runner->set_event_handler(bullet);
        TamaScope emt_scope;
        for (int i = 0; i < (int)std::min(bullet_params.size(), bullet_args.size()); ++i)
            emt_scope[bullet_params[i]] = TamaScopeVal(bullet_args[i]);
        emt_scope["spawn_x"] = TamaScopeVal(bullet->_initial_position.x);
        emt_scope["spawn_y"] = TamaScopeVal(bullet->_initial_position.y);
        spawner_runner->start_act(source_program, bullet_emitter_act, std::move(emt_scope));
    }
}

// ===========================================================================
// Resolution helpers
// ===========================================================================

float _TamaSpawnManager::_resolve_angle(const TamaBulletFireData &data, Object *spawner) const {
    static const float DEG2RAD = 3.14159265f / 180.0f;
    switch (data.dir_type) {
        case 0: return (player_position - spawner_get_global_position(spawner)).angle() + data.dir_value * DEG2RAD;
        case 1: return data.dir_value * DEG2RAD;
        case 2: return spawner_get_angle(spawner) + data.dir_value * DEG2RAD;
        case 3: return spawner_get_last_angle(spawner) + data.dir_value * DEG2RAD;
        default: break;
    }
    return (player_position - spawner_get_global_position(spawner)).angle() + data.dir_value * DEG2RAD;
}

float _TamaSpawnManager::_resolve_speed(const TamaBulletFireData &data, Object *spawner) const {
    switch (data.speed_type) {
        case 0: return data.speed_value;
        case 1: case 2: return spawner_get_last_speed(spawner) + data.speed_value;
        default: return data.speed_value;
    }
}

float _TamaSpawnManager::_resolve_rot_speed(const TamaBulletFireData &data, Object *spawner) const {
    switch (data.rot_speed_type) {
        case 0: return data.rot_speed_value;
        case 1: case 2: return spawner_get_last_rot_speed(spawner) + data.rot_speed_value;
        default: return data.rot_speed_value;
    }
}

Vector2 _TamaSpawnManager::_resolve_position(const TamaBulletFireData &data, Object *spawner, float angle) const {
    if (data.has_pos) {
        Vector2 pos = spawner_get_global_position(spawner);
        if (data.pos_x_set) {
            if (data.pos_x_type == 0 || data.pos_x_type == 2) pos.x  = data.pos_x; else pos.x += data.pos_x;
        }
        if (data.pos_y_set) {
            if (data.pos_y_type == 0 || data.pos_y_type == 2) pos.y  = data.pos_y; else pos.y += data.pos_y;
        }
        return pos;
    }
    if (data.offset_mode == 1) { // INLINE
        return spawner_get_global_position(spawner) + Vector2(data.offset_value, 0.0f).rotated(angle);
    }
    if (data.offset_mode == 2) { // BLOCK
        Vector2 world_offset, local_offset;
        if (data.offset_x_type == 1) local_offset.x = data.offset_x; else world_offset.x = data.offset_x; // REL=1
        if (data.offset_y_type == 1) local_offset.y = data.offset_y; else world_offset.y = data.offset_y;
        return spawner_get_global_position(spawner) + world_offset + local_offset.rotated(angle);
    }
    return spawner_get_global_position(spawner);
}

Node *_TamaSpawnManager::_get_spawn_parent() const {
    if (!is_inside_tree()) return nullptr;
    if (!spawn_parent.is_empty()) {
        Node *n = get_node_or_null(spawn_parent);
        if (n) return n;
    }
    return get_tree() ? get_tree()->get_current_scene() : nullptr;
}
