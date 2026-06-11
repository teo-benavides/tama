#include "tama_server_curved_laser.h"
#include "tama_manager.h"
#include "tama_server_curved_laser_pool.h"

#include <cmath>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

static constexpr float DEG2RAD = 3.14159265f / 180.0f;

void TamaServerCurvedLaser::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_position"), &TamaServerCurvedLaser::get_position);
    ClassDB::bind_method(D_METHOD("get_angle"),    &TamaServerCurvedLaser::get_angle);
    ClassDB::bind_method(D_METHOD("get_active"),   &TamaServerCurvedLaser::get_active);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "", "get_position");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "angle",    PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "", "get_angle");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,    "active",   PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "", "get_active");
}

void TamaServerCurvedLaser::_init_slot(TamaServerCurvedLaserPool *pool, CurvedLaserState *state, const std::string &key) {
    _pool     = pool;
    _state    = state;
    _type_key = key;
    state->wrapper = this;
}

Vector2 TamaServerCurvedLaser::get_position() const { return _state ? _state->position : Vector2(); }
float   TamaServerCurvedLaser::get_angle()    const { return _state ? _state->angle    : 0.0f; }
bool    TamaServerCurvedLaser::get_active()   const { return _state ? _state->active   : false; }

void TamaServerCurvedLaser::on_chdir(const TamaChdirEvent &e) {
    if (!_state || !_state->active) return;
    float val = e.dir_value * DEG2RAD;
    float target;
    switch (e.dir_type) {
        case 0: {
            TamaManager *mgr = TamaManager::get_instance();
            Vector2 player = mgr ? mgr->get_player_position() : Vector2();
            target = (player - _state->position).angle() + val;
            break;
        }
        case 1: target = val;                           break;  // ABS
        case 2: target = _state->angle      + val;     break;  // REL
        case 3: target = _state->last_angle + val;     break;  // SEQ
        default: target = val;                         break;
    }
    _state->last_angle = target;
    if (e.over > 0.0f) {
        _state->angle_tween = { true, _state->angle, target, 0.0f, e.over };
    } else {
        _state->angle_tween.active = false;
        _state->angle = target;
    }
}

void TamaServerCurvedLaser::on_chspd(const TamaChspdEvent &e) {
    if (!_state || !_state->active) return;
    float target;
    switch (e.speed_type) {
        case 1: target = _state->speed      + e.speed_value; break;  // REL
        case 2: target = _state->last_speed + e.speed_value; break;  // SEQ
        default: target = e.speed_value;                     break;  // ABS
    }
    _state->last_speed = _state->speed;
    if (e.over > 0.0f) {
        _state->speed_tween = { true, _state->speed, target, 0.0f, e.over };
    } else {
        _state->speed_tween.active = false;
        _state->speed = target;
    }
}

void TamaServerCurvedLaser::on_chrotspd(const TamaChrotspdEvent &e) {
    if (!_state || !_state->active) return;
    float target;
    switch (e.speed_type) {
        case 1: target = _state->rot_speed      + e.speed_value; break;  // REL
        case 2: target = _state->last_rot_speed + e.speed_value; break;  // SEQ
        default: target = e.speed_value;                         break;  // ABS
    }
    _state->last_rot_speed = _state->rot_speed;
    if (e.over > 0.0f) {
        _state->rot_speed_tween = { true, _state->rot_speed, target, 0.0f, e.over };
    } else {
        _state->rot_speed_tween.active = false;
        _state->rot_speed = target;
    }
}

void TamaServerCurvedLaser::on_chpos(const TamaChposEvent &e) {
    if (!_state || !_state->active) return;
    if (e.has_x) _state->position.x = (e.x_type == 1) ? _state->position.x + e.x : e.x;
    if (e.has_y) _state->position.y = (e.y_type == 1) ? _state->position.y + e.y : e.y;
}

void TamaServerCurvedLaser::on_vanished() {
    if (_pool && _state) _pool->_recycle_internal(_state);
}
