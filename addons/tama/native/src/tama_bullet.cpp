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
#include <godot_cpp/variant/variant.hpp>

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
    ClassDB::bind_method(D_METHOD("get_rot_speed"),        &TamaBullet::get_rot_speed);
    ClassDB::bind_method(D_METHOD("set_rot_speed","v"),    &TamaBullet::set_rot_speed);
    ClassDB::bind_method(D_METHOD("get_rotates"),            &TamaBullet::get_rotates);
    ClassDB::bind_method(D_METHOD("set_rotates","v"),        &TamaBullet::set_rotates);
    ClassDB::bind_method(D_METHOD("get_face_velocity"),      &TamaBullet::get_face_velocity);
    ClassDB::bind_method(D_METHOD("set_face_velocity","v"),  &TamaBullet::set_face_velocity);
    ClassDB::bind_method(D_METHOD("get_initial_position"), &TamaBullet::get_initial_position);
    ClassDB::bind_method(D_METHOD("set_initial_position","v"),&TamaBullet::set_initial_position);
    ClassDB::bind_method(D_METHOD("destroy"), &TamaBullet::destroy);

    GDVIRTUAL_BIND(_bullet_ready);
    GDVIRTUAL_BIND(_bullet_process, "delta");

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_angle", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_angle", "get_angle");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_speed", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_speed", "get_speed");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_speed_x", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_speed_x", "get_speed_x");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_speed_y", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_speed_y", "get_speed_y");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "_rot_speed", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "set_rot_speed", "get_rot_speed");
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
    add_to_group("tama_bullets");
    set_global_position(_initial_position);

    // Despawn if spawned off screen
    Rect2 world = _scene_bullet_world_bounds();
    Vector2 gp = get_global_position();
    if ((gp.x < world.position.x || gp.x > world.get_end().x) && (gp.y < world.position.y || gp.y > world.get_end().y)) {
        set_physics_process(false);
        call("destroy");
        return;
    }

    if (rotates) set_rotation(_angle);
    GDVIRTUAL_CALL(_bullet_ready);
}

void TamaBullet::_physics_process(double delta) {
    if (_just_spawned)
    {
        _just_spawned = false;
        return;
    }
    // Step interpreters so act commands (chdir/chspd/etc.) apply this frame
    if (_runner || _runner2) {
        Rect2 world = _scene_bullet_world_bounds();
        Vector2 gpos = get_global_position();
        float cx = gpos.x - world.position.x;
        float cy = gpos.y - world.position.y;
        if (_runner) {
            _runner->set_scope_float("current_x",     cx);
            _runner->set_scope_float("current_y",     cy);
            _runner->set_scope_float("current_angle", _angle);
            _runner->set_scope_float("current_speed", _speed);
            _runner->step(delta);
        }
        if (_runner2) {
            _runner2->set_scope_float("current_x",     cx);
            _runner2->set_scope_float("current_y",     cy);
            _runner2->set_scope_float("current_angle", _angle);
            _runner2->set_scope_float("current_speed", _speed);
            _runner2->step(delta);
        }
    }

    // Apply rotation speed (degrees/sec → accumulates into angle each frame)
    if (_rot_speed != 0.0f) {
        static const float DEG2RAD = 3.14159265f / 180.0f;
        _angle += _rot_speed * DEG2RAD * (float)delta;
    }

    if (_mvmt_x_set || _mvmt_y_set) {
        Vector2 pos_before = get_global_position();
        Vector2 pos = pos_before;
        if (_mvmt_x_set && _runner) {
            float vx = _runner->eval_expr_native(_mvmt_x_expr, _mvmt_var_names, _mvmt_var_values);
            pos.x = (_mvmt_x_type == 0) ? vx : _initial_position.x + vx; // 0=ABS
        }
        if (_mvmt_y_set && _runner) {
            float vy = _runner->eval_expr_native(_mvmt_y_expr, _mvmt_var_names, _mvmt_var_values);
            pos.y = (_mvmt_y_type == 0) ? vy : _initial_position.y + vy;
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
        GDVIRTUAL_CALL(_bullet_process, delta);
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
    if (_bounces_left != 0) {
        static const float PI = 3.14159265f;
        Rect2 world = _scene_bullet_world_bounds();
        float wx0 = world.position.x, wx1 = world.get_end().x;
        float wy0 = world.position.y, wy1 = world.get_end().y;
        Vector2 pos = get_global_position();
        bool hit_x = false, hit_y = false;
        if (_bounces_axis != 2) { // not y-only
            if (pos.x < wx0) {
                pos.x = 2.0f * wx0 - pos.x; hit_x = true;
            } else if (pos.x > wx1) {
                pos.x = 2.0f * wx1 - pos.x; hit_x = true;
            }
        }
        if (_bounces_axis != 1) { // not x-only
            if (pos.y < wy0) {
                pos.y = 2.0f * wy0 - pos.y; hit_y = true;
            } else if (pos.y > wy1) {
                pos.y = 2.0f * wy1 - pos.y; hit_y = true;
            }
        }
        if (hit_x || hit_y) {
            if (hit_x) { _angle = PI - _angle; _speed_x = -_speed_x; }
            if (hit_y) { _angle = -_angle;     _speed_y = -_speed_y; }
            set_global_position(pos);
            if (_bounces_left > 0)
                --_bounces_left;
        }
    }

    GDVIRTUAL_CALL(_bullet_process, delta);
}

void TamaBullet::destroy() {
    queue_free();
}

godot::Rect2 TamaBullet::_scene_bullet_world_bounds() const {
    if (TamaManager *mgr = TamaManager::get_instance()) {
        Rect2 wr = mgr->get_world_rect();
        if (wr.size.x > 0.0f || wr.size.y > 0.0f) return wr;
    }
    float sw = (float)(int)ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_width");
    float sh = (float)(int)ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_height");
    return Rect2(0.0f, 0.0f, sw, sh);
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

void TamaBullet::on_chrotspd(const TamaChrotspdEvent &e) {
    float target;
    switch (e.speed_type) {
        case 1: target = _rot_speed + e.speed_value;       break; // REL
        case 2: target = _last_rot_speed + e.speed_value;  break; // SEQ
        default: target = e.speed_value;                   break; // ABS
    }
    _last_rot_speed = _rot_speed;
    if (_rotspd_tween.is_valid()) _rotspd_tween->kill();
    if (e.over <= 0.0f) {
        _rot_speed = target;
    } else {
        _rotspd_tween = create_tween();
        _rotspd_tween->set_process_mode(Tween::TWEEN_PROCESS_PHYSICS);
        _rotspd_tween->tween_property(this, NodePath("_rot_speed"), Variant(target), (double)e.over)
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
