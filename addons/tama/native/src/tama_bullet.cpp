#include "tama_bullet.h"
#include "tama_manager.h"

#include <cmath>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// DirType: AIM=0, ABS=1, REL=2, SEQ=3
// ValueType: ABS=0, REL=1, SEQ=2

void TamaBullet::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_angle"),            &TamaBullet::get_angle);
    ClassDB::bind_method(D_METHOD("set_angle","v"),        &TamaBullet::set_angle);
    ClassDB::bind_method(D_METHOD("get_speed"),            &TamaBullet::get_speed);
    ClassDB::bind_method(D_METHOD("set_speed","v"),        &TamaBullet::set_speed);
    ClassDB::bind_method(D_METHOD("get_speed_x"),          &TamaBullet::get_speed_x);
    ClassDB::bind_method(D_METHOD("set_speed_x","v"),      &TamaBullet::set_speed_x);
    ClassDB::bind_method(D_METHOD("get_speed_y"),          &TamaBullet::get_speed_y);
    ClassDB::bind_method(D_METHOD("set_speed_y","v"),      &TamaBullet::set_speed_y);
    ClassDB::bind_method(D_METHOD("get_rotates"),            &TamaBullet::get_rotates);
    ClassDB::bind_method(D_METHOD("set_rotates","v"),        &TamaBullet::set_rotates);
    ClassDB::bind_method(D_METHOD("get_face_velocity"),      &TamaBullet::get_face_velocity);
    ClassDB::bind_method(D_METHOD("set_face_velocity","v"),  &TamaBullet::set_face_velocity);
    ClassDB::bind_method(D_METHOD("get_initial_position"), &TamaBullet::get_initial_position);
    ClassDB::bind_method(D_METHOD("set_initial_position","v"),&TamaBullet::set_initial_position);
    ClassDB::bind_method(D_METHOD("destroy"), &TamaBullet::destroy);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_angle", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_angle", "get_angle");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_speed", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_speed", "get_speed");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_speed_x", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_speed_x", "get_speed_x");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_speed_y", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_speed_y", "get_speed_y");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rotates"), "set_rotates", "get_rotates");
    // "face_velocity" is dynamic via _get_property_list so it can be read-only when rotates is false
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "_initial_position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_initial_position", "get_initial_position");
}

bool TamaBullet::_get(const StringName &p_name, Variant &r_ret) const {
    if (p_name == StringName("face_velocity")) { r_ret = face_velocity; return true; }
    return false;
}

bool TamaBullet::_set(const StringName &p_name, const Variant &p_value) {
    if (p_name == StringName("face_velocity")) { face_velocity = p_value; return true; }
    return false;
}

void TamaBullet::_get_property_list(List<PropertyInfo> *p_list) const {
    uint32_t fv_usage = rotates
        ? PROPERTY_USAGE_DEFAULT
        : (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_READ_ONLY);
    p_list->push_back(PropertyInfo(Variant::BOOL, "face_velocity", PROPERTY_HINT_NONE, "", fv_usage));
}

void TamaBullet::_ready() {
    if (_on_ready_cb) { auto cb = std::move(_on_ready_cb); cb(); }

    add_to_group("tama_bullets");
    set_global_position(_initial_position);

    // Despawn if spawned off screen
    float sw = (float)(int)ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_width");
    float sh = (float)(int)ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_height");
    Vector2 gp = get_global_position();
    if ((gp.x < 0.0f || gp.x > sw) && (gp.y < 0.0f || gp.y > sh)) {
        set_physics_process(false);
        call("destroy");
        return;
    }

    if (rotates) set_rotation(_angle);

    if (!_runner) return;
    _TamaInterpreter *runner = Object::cast_to<_TamaInterpreter>(_runner);
    if (!runner) return;

    runner->set_event_handler(this);
}

void TamaBullet::_physics_process(double /*delta*/) {
    if (_mvmt_x_set || _mvmt_y_set) {
        Vector2 pos_before = get_global_position();
        Vector2 pos = pos_before;
        _TamaInterpreter *runner = Object::cast_to<_TamaInterpreter>(_runner);
        if (runner) {
            if (_mvmt_x_set) {
                float vx = runner->eval_expr(_mvmt_x_expr, _mvmt_scope);
                pos.x = (_mvmt_x_type == 0) ? vx : _initial_position.x + vx; // 0=ABS
            }
            if (_mvmt_y_set) {
                float vy = runner->eval_expr(_mvmt_y_expr, _mvmt_scope);
                pos.y = (_mvmt_y_type == 0) ? vy : _initial_position.y + vy;
            }
        }
        set_global_position(pos);
        if (rotates) {
            if (face_velocity) {
                Vector2 dp = pos - pos_before;
                if (dp.length_squared() > 1e-8f)
                    set_rotation(std::atan2(dp.y, dp.x));
                else
                    set_rotation(_angle);
            } else {
                set_rotation(_angle);
            }
        }
        return;
    }

    Vector2 vel = Vector2(
        std::cos(_angle) * _speed + _speed_x,
        std::sin(_angle) * _speed + _speed_y
    );
    set_velocity(vel);
    if (rotates) {
        if (face_velocity && (vel.x != 0.0f || vel.y != 0.0f))
            set_rotation(std::atan2(vel.y, vel.x));
        else
            set_rotation(_angle);
    }
    move_and_slide();
}

void TamaBullet::destroy() {
    queue_free();
}

// ---------------------------------------------------------------------------
// Signal handlers
// ---------------------------------------------------------------------------

void TamaBullet::on_chdir(const TamaChdirEvent &e) {
    float target = _dir_to_angle(e.dir_type, e.dir_value);
    _last_angle = _angle;
    if (_dir_tween.is_valid()) _dir_tween->kill();
    if (e.over <= 0.0f) {
        _angle = target;
    } else {
        _dir_tween = create_tween();
        _dir_tween->set_process_mode(Tween::TWEEN_PROCESS_PHYSICS);
        _dir_tween->tween_property(this, NodePath("_angle"), Variant(target), (double)e.over)
                  ->set_trans(Tween::TRANS_LINEAR);
    }
}

void TamaBullet::on_chspd(const TamaChspdEvent &e) {
    float target = _spd_to_value(e.speed_type, e.speed_value);
    _last_speed = _speed;
    if (_spd_tween.is_valid()) _spd_tween->kill();
    if (e.over <= 0.0f) {
        _speed = target;
    } else {
        _spd_tween = create_tween();
        _spd_tween->set_process_mode(Tween::TWEEN_PROCESS_PHYSICS);
        _spd_tween->tween_property(this, NodePath("_speed"), Variant(target), (double)e.over)
                  ->set_trans(Tween::TRANS_LINEAR);
    }
}

void TamaBullet::on_chpos(const TamaChposEvent &e) {
    Vector2 target = get_global_position();
    if (e.has_x) {
        if (e.x_type == 0 || e.x_type == 2) target.x  = e.x;  // ABS or SEQ
        else                                  target.x += e.x;  // REL
    }
    if (e.has_y) {
        if (e.y_type == 0 || e.y_type == 2) target.y  = e.y;
        else                                  target.y += e.y;
    }
    if (_pos_tween.is_valid()) _pos_tween->kill();
    if (e.over <= 0.0f) {
        set_global_position(target);
    } else {
        _pos_tween = create_tween();
        _pos_tween->set_process_mode(Tween::TWEEN_PROCESS_PHYSICS);
        _pos_tween->tween_property(this, NodePath("global_position"), Variant(target), (double)e.over)
                  ->set_trans(Tween::TRANS_LINEAR);
    }
}

void TamaBullet::on_accel(const TamaAccelEvent &e) {
    if (_accel_tween.is_valid()) _accel_tween->kill();
    if (e.over <= 0.0f) {
        if (e.has_x) _speed_x = (e.x_type == 1) ? _speed_x + e.x : e.x;  // REL=1
        if (e.has_y) _speed_y = (e.y_type == 1) ? _speed_y + e.y : e.y;
    } else {
        _accel_tween = create_tween();
        _accel_tween->set_process_mode(Tween::TWEEN_PROCESS_PHYSICS);
        _accel_tween->set_parallel(true);
        if (e.has_x) {
            float ex = _accel_axis_end(e.x_type, e.x, _speed_x, e.over);
            _accel_tween->tween_property(this, NodePath("_speed_x"), Variant(ex), (double)e.over)
                        ->set_trans(Tween::TRANS_LINEAR);
        }
        if (e.has_y) {
            float ey = _accel_axis_end(e.y_type, e.y, _speed_y, e.over);
            _accel_tween->tween_property(this, NodePath("_speed_y"), Variant(ey), (double)e.over)
                        ->set_trans(Tween::TRANS_LINEAR);
        }
    }
}

void TamaBullet::on_vanished() { call("destroy"); }

// ---------------------------------------------------------------------------
// Conversion helpers
// ---------------------------------------------------------------------------

float TamaBullet::_dir_to_angle(int dir_type, float value) const {
    // Requires player position from TamaManager singleton
    static const float DEG2RAD = 3.14159265f / 180.0f;
    switch (dir_type) {
        case 0: { // AIM
            Vector2 player_pos;
            TamaManager *mgr = TamaManager::get_instance();
            if (mgr) player_pos = mgr->get_player_position();
            return (player_pos - get_global_position()).angle() + value * DEG2RAD;
        }
        case 1: return value * DEG2RAD;       // ABS
        case 2: return _angle + value * DEG2RAD;  // REL
        case 3: return _last_angle + value * DEG2RAD; // SEQ
        default: break;
    }
    return get_angle_to(Vector2()) + value * DEG2RAD;
}

float TamaBullet::_spd_to_value(int speed_type, float value) const {
    switch (speed_type) {
        case 0: return value;               // ABS
        case 1: return _speed + value;      // REL
        case 2: return _last_speed + value; // SEQ
        default: return value;
    }
}

float TamaBullet::_accel_axis_end(int axis_type, float value, float current, float over) const {
    (void)over;
    switch (axis_type) {
        case 0: case 2: return value;              // ABS / SEQ
        case 1: return (value - current) * over;   // REL
        default: return value;
    }
}
