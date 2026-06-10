#include "tama_server_laser.h"
#include "tama_manager.h"
#include "tama_server_laser_pool.h"

#include <cmath>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static constexpr float DEG2RAD = 3.14159265f / 180.0f;

void TamaServerLaser::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_position"), &TamaServerLaser::get_position);
    ClassDB::bind_method(D_METHOD("get_angle"),    &TamaServerLaser::get_angle);
    ClassDB::bind_method(D_METHOD("get_active"),   &TamaServerLaser::get_active);
    ClassDB::bind_method(D_METHOD("get_phase"),    &TamaServerLaser::get_phase);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "", "get_position");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "angle",   PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "", "get_angle");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,    "active",  PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "", "get_active");
    ADD_PROPERTY(PropertyInfo(Variant::INT,     "phase",   PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "", "get_phase");
}

void TamaServerLaser::_init_slot(TamaServerLaserPool *pool, LaserState *state, const std::string &key) {
    _pool     = pool;
    _state    = state;
    _type_key = key;
    state->wrapper = this;
}

Vector2 TamaServerLaser::get_position() const { return _state ? _state->position : Vector2(); }
float   TamaServerLaser::get_angle()    const { return _state ? _state->angle    : 0.0f; }
bool    TamaServerLaser::get_active()   const { return _state ? _state->active   : false; }
int     TamaServerLaser::get_phase()    const { return _state ? _state->phase    : 0; }

void TamaServerLaser::on_chdir(const TamaChdirEvent &e) {
    if (!_state || !_state->active) return;
    float val = e.dir_value * DEG2RAD;
    float target;
    switch (e.dir_type) {
        case 0: { // AIM — toward player
            TamaManager *mgr = TamaManager::get_instance();
            Vector2 player = mgr ? mgr->get_player_position() : Vector2();
            target = (player - _state->position).angle() + val;
            break;
        }
        case 1: target = val;                          break; // ABS
        case 2: target = _state->angle      + val;    break; // REL
        case 3: target = _state->last_angle + val;    break; // SEQ
        default: target = val;                        break;
    }
    _state->last_angle = target;
    if (e.over > 0.0f) {
        _state->angle_tween.active   = true;
        _state->angle_tween.from     = _state->angle;
        _state->angle_tween.to       = target;
        _state->angle_tween.elapsed  = 0.0f;
        _state->angle_tween.duration = e.over;
    } else {
        _state->angle_tween.active = false;
        _state->angle = target;
    }
}

void TamaServerLaser::on_chpos(const TamaChposEvent &e) {
    if (!_state || !_state->active) return;
    if (e.has_x) {
        _state->position.x = (e.x_type == 1) ? _state->position.x + e.x : e.x;
    }
    if (e.has_y) {
        _state->position.y = (e.y_type == 1) ? _state->position.y + e.y : e.y;
    }
}

void TamaServerLaser::on_vanished() {
    if (_pool && _state) _pool->_recycle_internal(_state);
}
