#pragma once
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

class TamaServerBulletConfig : public godot::Resource {
    GDCLASS(TamaServerBulletConfig, godot::Resource)
protected:
    static void _bind_methods();
public:
    godot::TypedArray<godot::Texture2D> frames;
    float  fps             = 0.0f;
    godot::Rect2   rect           = {-8.0f, -8.0f, 16.0f, 16.0f};
    godot::Vector2 texture_scale  = {1.0f, 1.0f};
    bool           auto_rect      = true;
    float  shape_radius    = 6.0f;
    int    collision_layer = 1;
    int    collision_mask  = 2;
    bool   rotates              = true;
    bool   face_velocity        = true;
    int    pool_size            = 1000;
    float  out_of_bounds_margin = 50.0f;

    // Virtuals for conditional rect visibility
    bool _get(const godot::StringName &p_name, godot::Variant &r_ret) const;
    bool _set(const godot::StringName &p_name, const godot::Variant &p_value);
    void _get_property_list(godot::List<godot::PropertyInfo> *p_list) const;

    godot::TypedArray<godot::Texture2D> get_frames()  const { return frames; }
    void set_frames(godot::TypedArray<godot::Texture2D> v);
    float get_fps()                      const { return fps; }
    void  set_fps(float v)                     { fps = v; }
    godot::Rect2   get_rect()           const { return rect; }
    void set_rect(godot::Rect2 v)             { rect = v; }
    bool get_auto_rect()                const { return auto_rect; }
    void set_auto_rect(bool v);
    godot::Vector2 get_texture_scale()  const { return texture_scale; }
    void set_texture_scale(godot::Vector2 v)  { texture_scale = v; }
    float get_shape_radius()            const { return shape_radius; }
    void set_shape_radius(float v)            { shape_radius = v; }
    int  get_collision_layer()          const { return collision_layer; }
    void set_collision_layer(int v)           { collision_layer = v; }
    int  get_collision_mask()           const { return collision_mask; }
    void set_collision_mask(int v)            { collision_mask = v; }
    bool get_rotates()                  const { return rotates; }
    void set_rotates(bool v)                  { rotates = v; notify_property_list_changed(); }
    bool get_face_velocity()            const { return face_velocity; }
    void set_face_velocity(bool v)            { face_velocity = v; }
    int   get_pool_size()                    const { return pool_size; }
    void  set_pool_size(int v)                     { pool_size = v; }
    float get_out_of_bounds_margin()         const { return out_of_bounds_margin; }
    void  set_out_of_bounds_margin(float v)        { out_of_bounds_margin = v; }
};
