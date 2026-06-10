#include "tama_server_straight_laser_config.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void TamaServerStraightLaserConfig::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_width"),           &TamaServerStraightLaserConfig::get_width);
    ClassDB::bind_method(D_METHOD("set_width","v"),       &TamaServerStraightLaserConfig::set_width);
    ClassDB::bind_method(D_METHOD("get_length"),          &TamaServerStraightLaserConfig::get_length);
    ClassDB::bind_method(D_METHOD("set_length","v"),      &TamaServerStraightLaserConfig::set_length);
    ClassDB::bind_method(D_METHOD("get_texture"),         &TamaServerStraightLaserConfig::get_texture);
    ClassDB::bind_method(D_METHOD("set_texture","v"),     &TamaServerStraightLaserConfig::set_texture);
    ClassDB::bind_method(D_METHOD("get_tile"),            &TamaServerStraightLaserConfig::get_tile);
    ClassDB::bind_method(D_METHOD("set_tile","v"),        &TamaServerStraightLaserConfig::set_tile);
    ClassDB::bind_method(D_METHOD("get_base_texture"),    &TamaServerStraightLaserConfig::get_base_texture);
    ClassDB::bind_method(D_METHOD("set_base_texture","v"),&TamaServerStraightLaserConfig::set_base_texture);
    ClassDB::bind_method(D_METHOD("get_delay_frames"),    &TamaServerStraightLaserConfig::get_delay_frames);
    ClassDB::bind_method(D_METHOD("set_delay_frames","v"),&TamaServerStraightLaserConfig::set_delay_frames);
    ClassDB::bind_method(D_METHOD("get_expand_frames"),   &TamaServerStraightLaserConfig::get_expand_frames);
    ClassDB::bind_method(D_METHOD("set_expand_frames","v"),&TamaServerStraightLaserConfig::set_expand_frames);
    ClassDB::bind_method(D_METHOD("get_duration_frames"), &TamaServerStraightLaserConfig::get_duration_frames);
    ClassDB::bind_method(D_METHOD("set_duration_frames","v"),&TamaServerStraightLaserConfig::set_duration_frames);
    ClassDB::bind_method(D_METHOD("get_fade_frames"),     &TamaServerStraightLaserConfig::get_fade_frames);
    ClassDB::bind_method(D_METHOD("set_fade_frames","v"), &TamaServerStraightLaserConfig::set_fade_frames);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width",  PROPERTY_HINT_RANGE, "1,500,1,or_greater"),
                 "set_width",  "get_width");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "length", PROPERTY_HINT_RANGE, "1,5000,1,or_greater"),
                 "set_length", "get_length");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture",      PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"),
                 "set_texture",      "get_texture");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tile"),
                 "set_tile", "get_tile");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "base_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"),
                 "set_base_texture", "get_base_texture");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "delay_frames",    PROPERTY_HINT_RANGE, "0,600,1,or_greater"),
                 "set_delay_frames",    "get_delay_frames");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "expand_frames",   PROPERTY_HINT_RANGE, "0,120,1,or_greater"),
                 "set_expand_frames",   "get_expand_frames");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "duration_frames", PROPERTY_HINT_RANGE, "1,600,1,or_greater"),
                 "set_duration_frames", "get_duration_frames");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "fade_frames",     PROPERTY_HINT_RANGE, "0,120,1,or_greater"),
                 "set_fade_frames",     "get_fade_frames");
}
