#include "tama_spawn_manager.h"
#include "tama_emitter.h"
#include "tama_manager.h"

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
    ClassDB::bind_method(D_METHOD("connect_interpreter","interpreter","spawner"), &_TamaSpawnManager::connect_interpreter);
    ClassDB::bind_method(D_METHOD("get_tama_script","filename"), &_TamaSpawnManager::get_tama_script);
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

void _TamaSpawnManager::connect_interpreter(Object *interpreter, Node2D *spawner) {
    interpreter->connect("bullet_fired",
        callable_mp(this, &_TamaSpawnManager::_on_bullet_fired).bind(Variant(spawner)));
}

Object *_TamaSpawnManager::get_tama_script(const String &filename) const {
    if (_repository) return _repository->get_tama_script(filename);
    return nullptr;
}

int _TamaSpawnManager::get_scene_bullet_count() const {
    SceneTree *tree = get_tree();
    return tree ? tree->get_nodes_in_group("tama_bullets").size() : 0;
}

// ===========================================================================
// Spawner accessors — direct C++ member access, no Godot property-system overhead
// ===========================================================================

static float spawner_get_last_angle(godot::Node2D *s) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))  return e->_last_angle;
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))   return b->_last_angle;
    return 0.0f;
}
static void spawner_set_last_angle(godot::Node2D *s, float v) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))  { e->_last_angle = v; return; }
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))     b->_last_angle = v;
}
static float spawner_get_last_speed(godot::Node2D *s) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))  return e->_last_speed;
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))   return b->_last_speed;
    return 0.0f;
}
static void spawner_set_last_speed(godot::Node2D *s, float v) {
    if (auto *e = godot::Object::cast_to<TamaEmitter>(s))  { e->_last_speed = v; return; }
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))     b->_last_speed = v;
}
static float spawner_get_angle(godot::Node2D *s) {
    if (auto *b = godot::Object::cast_to<TamaBullet>(s))   return b->_angle;
    return s->get_rotation(); // emitter: use node rotation as reference for dir rel
}

// ===========================================================================
// Bullet firing
// ===========================================================================

void _TamaSpawnManager::_on_bullet_fired(Variant data_v, Node2D *spawner) {
    Object *data = data_v.operator Object *();
    if (!data) return;

    // Resolve bullet_type (substitute from params if it matches a param name)
    String bullet_type = (String)data->get("bullet_type");
    Array bullet_params = (Array)data->get("bullet_params");
    Array bullet_args   = (Array)data->get("bullet_args");
    if (!bullet_type.is_empty() && bullet_params.size() > 0) {
        for (int i = 0; i < (int)UtilityFunctions::mini(bullet_params.size(), bullet_args.size()); ++i) {
            if (String(bullet_params[i]) == bullet_type) {
                bullet_type = String(Variant(bullet_args[i]));
                break;
            }
        }
    }

    // Fast path: server bullet
    if (registry.is_valid() && _server_pool) {
        TamaServerBulletConfig *srv_cfg = registry->get_server_config(bullet_type);
        if (srv_cfg) {
            float angle    = _resolve_angle(data, spawner);
            float speed    = _resolve_speed(data, spawner);
            Vector2 pos    = _resolve_position(data, spawner, angle);
            spawner_set_last_angle(spawner, angle);
            spawner_set_last_speed(spawner, speed);
            _server_pool->spawn(data, srv_cfg, angle, speed, pos, context.ptr());
            return;
        }
    }

    // Scene bullet path
    Ref<PackedScene> scene;
    if (registry.is_valid()) {
        if (!bullet_type or bullet_type.is_empty()) {
            scene = registry->default_bullet;
        } else {
            Variant sv = registry->entries.get(bullet_type, Variant());
            scene = Ref<PackedScene>(Object::cast_to<PackedScene>(sv.operator Object *()));
            if (!scene.is_valid()) scene = registry->default_bullet;
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

    _TamaInterpreter *bullet_runner = memnew(_TamaInterpreter);
    bullet_runner->set_context(context.ptr());
    bullet->add_child(bullet_runner);
    bullet->_runner = bullet_runner;

    float angle = _resolve_angle(data, spawner);
    float speed = _resolve_speed(data, spawner);
    spawner_set_last_angle(spawner, angle);
    spawner_set_last_speed(spawner, speed);

    bullet->_angle            = angle;
    bullet->_speed            = speed;
    bullet->_initial_position = _resolve_position(data, spawner, angle);

    bool mvmt_x_set = (bool)data->get("mvmt_x_set");
    bool mvmt_y_set = (bool)data->get("mvmt_y_set");
    if (mvmt_x_set || mvmt_y_set) {
        Dictionary mvmt_scope;
        for (int i = 0; i < (int)UtilityFunctions::mini(bullet_params.size(), bullet_args.size()); ++i)
            mvmt_scope[bullet_params[i]] = bullet_args[i];
        mvmt_scope["spawn_x"] = bullet->_initial_position.x;
        mvmt_scope["spawn_y"] = bullet->_initial_position.y;
        bullet->_mvmt_x_set  = mvmt_x_set;
        bullet->_mvmt_x_type = (int)data->get("mvmt_x_type");
        bullet->_mvmt_x_expr = (String)data->get("mvmt_x_expr");
        bullet->_mvmt_y_set  = mvmt_y_set;
        bullet->_mvmt_y_type = (int)data->get("mvmt_y_type");
        bullet->_mvmt_y_expr = (String)data->get("mvmt_y_expr");
        bullet->_mvmt_scope  = mvmt_scope;
    }

    spawn_parent->call_deferred("add_child", Variant(bullet));

    Object *bullet_act         = Object::cast_to<Object>(data->get("bullet_act"));
    Object *bullet_emitter_act = Object::cast_to<Object>(data->get("bullet_emitter_act"));
    Object *source_program     = Object::cast_to<Object>(data->get("source_program"));

    if (bullet_act) {
        Dictionary act_scope;
        for (int i = 0; i < (int)UtilityFunctions::mini(bullet_params.size(), bullet_args.size()); ++i)
            act_scope[bullet_params[i]] = bullet_args[i];
        act_scope["spawn_x"] = bullet->_initial_position.x;
        act_scope["spawn_y"] = bullet->_initial_position.y;
        connect_interpreter(bullet_runner, bullet);
        bullet->connect("ready",
            callable_mp(bullet_runner, &_TamaInterpreter::start_act)
                .bind(Variant(source_program), Variant(bullet_act), Variant(act_scope)),
            Object::CONNECT_ONE_SHOT);
    }

    if (bullet_emitter_act) {
        _TamaInterpreter *spawner_runner;
        if (bullet_act) {
            spawner_runner = memnew(_TamaInterpreter);
            spawner_runner->set_context(context.ptr());
            bullet->add_child(spawner_runner);
        } else {
            spawner_runner = bullet_runner;
        }
        connect_interpreter(spawner_runner, bullet);
        Dictionary emt_scope;
        for (int i = 0; i < (int)UtilityFunctions::mini(bullet_params.size(), bullet_args.size()); ++i)
            emt_scope[bullet_params[i]] = bullet_args[i];
        emt_scope["spawn_x"] = bullet->_initial_position.x;
        emt_scope["spawn_y"] = bullet->_initial_position.y;
        bullet->connect("ready",
            callable_mp(spawner_runner, &_TamaInterpreter::start_act)
                .bind(Variant(source_program), Variant(bullet_emitter_act), Variant(emt_scope)),
            Object::CONNECT_ONE_SHOT);
    }
}

// ===========================================================================
// Resolution helpers
// ===========================================================================

float _TamaSpawnManager::_resolve_angle(Object *data, Node2D *spawner) const {
    static const float DEG2RAD = 3.14159265f / 180.0f;
    int   dir_type  = (int)  data->get("dir_type");
    float dir_value = (float)data->get("dir_value");
    switch (dir_type) {
        case 0: return (player_position - spawner->get_global_position()).angle() + dir_value * DEG2RAD;
        case 1: return dir_value * DEG2RAD;
        case 2: return spawner_get_angle(spawner) + dir_value * DEG2RAD;
        case 3: return spawner_get_last_angle(spawner) + dir_value * DEG2RAD;
        default: break;
    }
    return spawner->get_angle_to(player_position) + dir_value * DEG2RAD;
}

float _TamaSpawnManager::_resolve_speed(Object *data, Node2D *spawner) const {
    int   speed_type  = (int)  data->get("speed_type");
    float speed_value = (float)data->get("speed_value");
    switch (speed_type) {
        case 0: return speed_value;
        case 1: case 2: return spawner_get_last_speed(spawner) + speed_value;
        default: return speed_value;
    }
}

Vector2 _TamaSpawnManager::_resolve_position(Object *data, Node2D *spawner, float angle) const {
    bool has_pos = (bool)data->get("has_pos");
    if (has_pos) {
        Vector2 pos = spawner->get_global_position();
        if ((bool)data->get("pos_x_set")) {
            int xt = (int)data->get("pos_x_type"); float xv = (float)data->get("pos_x");
            if (xt == 0 || xt == 2) pos.x  = xv; else pos.x += xv;
        }
        if ((bool)data->get("pos_y_set")) {
            int yt = (int)data->get("pos_y_type"); float yv = (float)data->get("pos_y");
            if (yt == 0 || yt == 2) pos.y  = yv; else pos.y += yv;
        }
        return pos;
    }
    int offset_mode = (int)data->get("offset_mode");
    if (offset_mode == 1) { // INLINE
        float ov = (float)data->get("offset_value");
        return spawner->get_global_position() + Vector2(ov, 0.0f).rotated(angle);
    }
    if (offset_mode == 2) { // BLOCK
        Vector2 world_offset, local_offset;
        int oxt = (int)data->get("offset_x_type"); float oxv = (float)data->get("offset_x");
        if (oxt == 1) local_offset.x = oxv; else world_offset.x = oxv; // REL=1
        int oyt = (int)data->get("offset_y_type"); float oyv = (float)data->get("offset_y");
        if (oyt == 1) local_offset.y = oyv; else world_offset.y = oyv;
        return spawner->get_global_position() + world_offset + local_offset.rotated(angle);
    }
    return spawner->get_global_position();
}

Node *_TamaSpawnManager::_get_spawn_parent() const {
    if (!is_inside_tree()) return nullptr;
    if (!spawn_parent.is_empty()) {
        Node *n = get_node_or_null(spawn_parent);
        if (n) return n;
    }
    return get_tree() ? get_tree()->get_current_scene() : nullptr;
}
