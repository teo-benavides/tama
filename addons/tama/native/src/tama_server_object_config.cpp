#include "tama_server_object_config.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void TamaServerObjectConfig::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_pool_size"),               &TamaServerObjectConfig::get_pool_size);
    ClassDB::bind_method(D_METHOD("set_pool_size","v"),           &TamaServerObjectConfig::set_pool_size);
    ClassDB::bind_method(D_METHOD("get_out_of_bounds_margin"),    &TamaServerObjectConfig::get_out_of_bounds_margin);
    ClassDB::bind_method(D_METHOD("set_out_of_bounds_margin","v"),&TamaServerObjectConfig::set_out_of_bounds_margin);

    ADD_PROPERTY(PropertyInfo(Variant::INT,   "pool_size"),
                 "set_pool_size",   "get_pool_size");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "out_of_bounds_margin"),
                 "set_out_of_bounds_margin", "get_out_of_bounds_margin");
}
