#pragma once
#include "tama_interpreter.h"

#include <functional>
#include <godot_cpp/classes/character_body2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

class TamaBullet : public godot::CharacterBody2D, public TamaBulletEventHandler {
    GDCLASS(TamaBullet, godot::CharacterBody2D)
protected:
    static void _bind_methods();

public:
    // Owned interpreter(s) — freed when bullet is freed
    std::unique_ptr<_TamaInterpreter> _runner;   // act runner (also used for eval context)
    std::unique_ptr<_TamaInterpreter> _runner2;  // emitter-act runner (may be null)

    float          _angle         = 0.0f;
    float          _speed         = 0.0f;
    float          _speed_x       = 0.0f;
    float          _speed_y       = 0.0f;
    float          _last_angle    = 0.0f;
    float          _last_speed    = 0.0f;
    bool           _mvmt_x_set    = false;
    int            _mvmt_x_type   = 0;
    std::string    _mvmt_x_expr;
    bool           _mvmt_y_set    = false;
    int            _mvmt_y_type   = 0;
    std::string    _mvmt_y_expr;
    std::vector<std::string> _mvmt_var_names;
    std::vector<double>      _mvmt_var_values;
    godot::Vector2 _initial_position;
    float          _rot_speed      = 0.0f;  // degrees/sec; applied each frame as angle += rot_speed * DEG2RAD * delta
    float          _last_rot_speed = 0.0f;
    int            _bounces_left   = 0;     // 0=none, -1=infinite, N>0=remaining
    int            _bounces_axis   = 0;     // 0=both, 1=x (left/right), 2=y (top/bottom)

    bool rotates       = true;
    bool face_velocity = true;

    void _ready()                       override;
    void _physics_process(double delta) override;

    bool _get(const godot::StringName &p_name, godot::Variant &r_ret) const;
    bool _set(const godot::StringName &p_name, const godot::Variant &p_value);
    void _get_property_list(godot::List<godot::PropertyInfo> *p_list) const;

    virtual void destroy();

    float  get_angle()    const { return _angle; }
    void   set_angle(float v)   { _angle = v; }
    float  get_speed()    const { return _speed; }
    void   set_speed(float v)   { _speed = v; }
    float  get_speed_x()   const { return _speed_x; }
    void   set_speed_x(float v)  { _speed_x = v; }
    float  get_speed_y()   const { return _speed_y; }
    void   set_speed_y(float v)  { _speed_y = v; }
    float  get_rot_speed() const { return _rot_speed; }
    void   set_rot_speed(float v){ _rot_speed = v; }

    bool  get_rotates()         const { return rotates; }
    void  set_rotates(bool v)         { rotates = v; notify_property_list_changed(); }
    bool  get_face_velocity()   const { return face_velocity; }
    void  set_face_velocity(bool v)   { face_velocity = v; }

    godot::Vector2 get_initial_position() const { return _initial_position; }
    void set_initial_position(godot::Vector2 v) { _initial_position = v; }

private:
    godot::Ref<godot::Tween> _dir_tween;
    godot::Ref<godot::Tween> _spd_tween;
    godot::Ref<godot::Tween> _rotspd_tween;
    godot::Ref<godot::Tween> _pos_tween;
    godot::Ref<godot::Tween> _accel_tween;

    void on_chdir    (const TamaChdirEvent    &e) override;
    void on_chspd    (const TamaChspdEvent    &e) override;
    void on_chrotspd (const TamaChrotspdEvent &e) override;
    void on_chpos    (const TamaChposEvent    &e) override;
    void on_accel    (const TamaAccelEvent    &e) override;
    void on_vanished ()                            override;

    float _dir_to_angle(int dir_type, float value) const;
    float _spd_to_value(int speed_type, float value) const;
    float _accel_axis_end(int axis_type, float value, float current, float over) const;
    godot::Rect2 _scene_bullet_world_bounds() const;
};
