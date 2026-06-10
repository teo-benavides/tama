#include "tama_server_object_config.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void TamaServerObjectConfig::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_pool_size"),               &TamaServerObjectConfig::get_pool_size);
    ClassDB::bind_method(D_METHOD("set_pool_size","v"),           &TamaServerObjectConfig::set_pool_size);
    ClassDB::bind_method(D_METHOD("get_collision_layer"),         &TamaServerObjectConfig::get_collision_layer);
    ClassDB::bind_method(D_METHOD("set_collision_layer","v"),     &TamaServerObjectConfig::set_collision_layer);
    ClassDB::bind_method(D_METHOD("get_collision_mask"),          &TamaServerObjectConfig::get_collision_mask);
    ClassDB::bind_method(D_METHOD("set_collision_mask","v"),      &TamaServerObjectConfig::set_collision_mask);
    ClassDB::bind_method(D_METHOD("get_out_of_bounds_margin"),    &TamaServerObjectConfig::get_out_of_bounds_margin);
    ClassDB::bind_method(D_METHOD("set_out_of_bounds_margin","v"),&TamaServerObjectConfig::set_out_of_bounds_margin);

    ADD_PROPERTY(PropertyInfo(Variant::INT,   "pool_size"),
                 "set_pool_size",   "get_pool_size");
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "collision_layer", PROPERTY_HINT_LAYERS_2D_PHYSICS),
                 "set_collision_layer", "get_collision_layer");
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "collision_mask",  PROPERTY_HINT_LAYERS_2D_PHYSICS),
                 "set_collision_mask",  "get_collision_mask");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "out_of_bounds_margin"),
                 "set_out_of_bounds_margin", "get_out_of_bounds_margin");
}
