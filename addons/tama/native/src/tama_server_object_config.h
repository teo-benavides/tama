#pragma once
#include <godot_cpp/classes/resource.hpp>

class TamaServerObjectConfig : public godot::Resource {
    GDCLASS(TamaServerObjectConfig, godot::Resource)
protected:
    static void _bind_methods();
public:
    int   pool_size            = 1000;
    int   collision_layer      = 1;
    int   collision_mask       = 2;
    float out_of_bounds_margin = 50.0f;

    int   get_pool_size()                    const { return pool_size; }
    void  set_pool_size(int v)                     { pool_size = v; }
    int   get_collision_layer()              const { return collision_layer; }
    void  set_collision_layer(int v)               { collision_layer = v; }
    int   get_collision_mask()               const { return collision_mask; }
    void  set_collision_mask(int v)                { collision_mask = v; }
    float get_out_of_bounds_margin()         const { return out_of_bounds_margin; }
    void  set_out_of_bounds_margin(float v)        { out_of_bounds_margin = v; }
};
