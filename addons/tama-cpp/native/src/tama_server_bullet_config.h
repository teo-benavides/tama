#pragma once
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

class TamaServerBulletConfig : public godot::Resource {
    GDCLASS(TamaServerBulletConfig, godot::Resource)
protected:
    static void _bind_methods();
public:
    godot::Ref<godot::Texture2D> texture;
    godot::Rect2   rect           = {-8.0f, -8.0f, 16.0f, 16.0f};
    godot::Vector2 texture_scale  = {1.0f, 1.0f};
    float  shape_radius    = 6.0f;
    int    collision_layer = 1;
    int    collision_mask  = 2;
    bool   rotates         = true;
    int    pool_size       = 1000;

    godot::Ref<godot::Texture2D> get_texture()        const { return texture; }
    void set_texture(godot::Ref<godot::Texture2D> v)        { texture = v; }
    godot::Rect2   get_rect()           const { return rect; }
    void set_rect(godot::Rect2 v)             { rect = v; }
    godot::Vector2 get_texture_scale()  const { return texture_scale; }
    void set_texture_scale(godot::Vector2 v)  { texture_scale = v; }
    float get_shape_radius()            const { return shape_radius; }
    void set_shape_radius(float v)            { shape_radius = v; }
    int  get_collision_layer()          const { return collision_layer; }
    void set_collision_layer(int v)           { collision_layer = v; }
    int  get_collision_mask()           const { return collision_mask; }
    void set_collision_mask(int v)            { collision_mask = v; }
    bool get_rotates()                  const { return rotates; }
    void set_rotates(bool v)                  { rotates = v; }
    int  get_pool_size()                const { return pool_size; }
    void set_pool_size(int v)                 { pool_size = v; }
};
