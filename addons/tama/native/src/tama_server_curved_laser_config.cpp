#include "tama_server_curved_laser_config.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void TamaServerCurvedLaserConfig::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_width"),       &TamaServerCurvedLaserConfig::get_width);
    ClassDB::bind_method(D_METHOD("set_width","v"),   &TamaServerCurvedLaserConfig::set_width);
    ClassDB::bind_method(D_METHOD("get_length"),      &TamaServerCurvedLaserConfig::get_length);
    ClassDB::bind_method(D_METHOD("set_length","v"),  &TamaServerCurvedLaserConfig::set_length);
    ClassDB::bind_method(D_METHOD("get_texture"),     &TamaServerCurvedLaserConfig::get_texture);
    ClassDB::bind_method(D_METHOD("set_texture","v"), &TamaServerCurvedLaserConfig::set_texture);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width",  PROPERTY_HINT_RANGE, "1,500,1,or_greater"),
                 "set_width",  "get_width");
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "length", PROPERTY_HINT_RANGE, "2,200,1,or_greater"),
                 "set_length", "get_length");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture",
                 PROPERTY_HINT_RESOURCE_TYPE, "TamaAnimatedTexture"),
                 "set_texture", "get_texture");
}
