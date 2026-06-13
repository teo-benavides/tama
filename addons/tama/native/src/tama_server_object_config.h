#pragma once
#include <godot_cpp/classes/resource.hpp>

class TamaServerObjectConfig : public godot::Resource {
    GDCLASS(TamaServerObjectConfig, godot::Resource)
protected:
    static void _bind_methods();
public:
    int   pool_size            = 1000;
    float out_of_bounds_margin = 50.0f;
    int   z_index              = 0;

    int   get_pool_size()                    const { return pool_size; }
    void  set_pool_size(int v)                     { pool_size = v; }
    float get_out_of_bounds_margin()         const { return out_of_bounds_margin; }
    void  set_out_of_bounds_margin(float v)        { out_of_bounds_margin = v; }
    int   get_z_index()                      const { return z_index; }
    void  set_z_index(int v)                       { z_index = v; }
};
