#pragma once
#include "tama_animated_texture.h"
#include "tama_server_object_config.h"
#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

class TamaServerBulletConfig : public TamaServerObjectConfig {
    GDCLASS(TamaServerBulletConfig, TamaServerObjectConfig)
protected:
    static void _bind_methods();
public:
    godot::Ref<TamaAnimatedTexture> texture;
    godot::Rect2   rect           = {-8.0f, -8.0f, 16.0f, 16.0f};
    godot::Vector2 texture_scale  = {1.0f, 1.0f};
    bool           auto_rect      = true;
    float  shape_radius    = 6.0f;
    bool   rotates              = true;
    bool   face_velocity        = true;

    // Spawn animation
    int    spawn_delay             = 0;
    godot::Ref<TamaAnimatedTexture> spawn_texture;
    float  starting_spawn_scale    = 2.0f;
    float  starting_spawn_opacity  = 0.0f;

    // Virtuals for conditional property visibility
    bool _get(const godot::StringName &p_name, godot::Variant &r_ret) const;
    bool _set(const godot::StringName &p_name, const godot::Variant &p_value);
    void _get_property_list(godot::List<godot::PropertyInfo> *p_list) const;

    godot::Ref<TamaAnimatedTexture> get_texture() const { return texture; }
    void set_texture(godot::Ref<TamaAnimatedTexture> v);
    godot::Rect2   get_rect()           const { return rect; }
    void set_rect(godot::Rect2 v)             { rect = v; }
    bool get_auto_rect()                const { return auto_rect; }
    void set_auto_rect(bool v);
    godot::Vector2 get_texture_scale()  const { return texture_scale; }
    void set_texture_scale(godot::Vector2 v)  { texture_scale = v; }
    float get_shape_radius()            const { return shape_radius; }
    void set_shape_radius(float v)            { shape_radius = v; }
    bool get_rotates()                  const { return rotates; }
    void set_rotates(bool v)                  { rotates = v; notify_property_list_changed(); }
    bool get_face_velocity()            const { return face_velocity; }
    void set_face_velocity(bool v)            { face_velocity = v; }

    int   get_spawn_delay()                  const { return spawn_delay; }
    void  set_spawn_delay(int v)                   { spawn_delay = v; notify_property_list_changed(); }
    godot::Ref<TamaAnimatedTexture> get_spawn_texture()  const { return spawn_texture; }
    void  set_spawn_texture(godot::Ref<TamaAnimatedTexture> v) { spawn_texture = v; }
    float get_starting_spawn_scale()         const { return starting_spawn_scale; }
    void  set_starting_spawn_scale(float v)        { starting_spawn_scale = v; }
    float get_starting_spawn_opacity()       const { return starting_spawn_opacity; }
    void  set_starting_spawn_opacity(float v)      { starting_spawn_opacity = v; }
};
