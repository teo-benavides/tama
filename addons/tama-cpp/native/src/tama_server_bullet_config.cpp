#include "tama_server_bullet_config.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>

using namespace godot;

void TamaServerBulletConfig::set_texture(Ref<Texture2D> v) {
    texture = v;
    if (auto_rect && texture.is_valid()) {
        Vector2i sz = texture->get_size();
        rect = Rect2(-sz.x * 0.5f, -sz.y * 0.5f, (float)sz.x, (float)sz.y);
    }
}

void TamaServerBulletConfig::set_auto_rect(bool v) {
    auto_rect = v;
    if (auto_rect && texture.is_valid()) {
        Vector2i sz = texture->get_size();
        rect = Rect2(-sz.x * 0.5f, -sz.y * 0.5f, (float)sz.x, (float)sz.y);
    }
    notify_property_list_changed();
}

bool TamaServerBulletConfig::_get(const StringName &p_name, Variant &r_ret) const {
    if (p_name == StringName("rect")) {
        r_ret = rect;
        return true;
    }
    return false;
}

bool TamaServerBulletConfig::_set(const StringName &p_name, const Variant &p_value) {
    if (p_name == StringName("rect")) {
        rect = p_value;
        return true;
    }
    return false;
}

void TamaServerBulletConfig::_get_property_list(List<PropertyInfo> *p_list) const {
    uint32_t usage = auto_rect
        ? (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_READ_ONLY)
        : PROPERTY_USAGE_DEFAULT;
    p_list->push_back(PropertyInfo(Variant::RECT2, "rect", PROPERTY_HINT_NONE, "", usage));
}

void TamaServerBulletConfig::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_texture"),            &TamaServerBulletConfig::get_texture);
    ClassDB::bind_method(D_METHOD("set_texture", "v"),       &TamaServerBulletConfig::set_texture);
    ClassDB::bind_method(D_METHOD("get_auto_rect"),          &TamaServerBulletConfig::get_auto_rect);
    ClassDB::bind_method(D_METHOD("set_auto_rect", "v"),     &TamaServerBulletConfig::set_auto_rect);
    ClassDB::bind_method(D_METHOD("get_rect"),               &TamaServerBulletConfig::get_rect);
    ClassDB::bind_method(D_METHOD("set_rect", "v"),          &TamaServerBulletConfig::set_rect);
    ClassDB::bind_method(D_METHOD("get_texture_scale"),    &TamaServerBulletConfig::get_texture_scale);
    ClassDB::bind_method(D_METHOD("set_texture_scale","v"),&TamaServerBulletConfig::set_texture_scale);
    ClassDB::bind_method(D_METHOD("get_shape_radius"),     &TamaServerBulletConfig::get_shape_radius);
    ClassDB::bind_method(D_METHOD("set_shape_radius","v"), &TamaServerBulletConfig::set_shape_radius);
    ClassDB::bind_method(D_METHOD("get_collision_layer"),  &TamaServerBulletConfig::get_collision_layer);
    ClassDB::bind_method(D_METHOD("set_collision_layer","v"),&TamaServerBulletConfig::set_collision_layer);
    ClassDB::bind_method(D_METHOD("get_collision_mask"),   &TamaServerBulletConfig::get_collision_mask);
    ClassDB::bind_method(D_METHOD("set_collision_mask","v"),&TamaServerBulletConfig::set_collision_mask);
    ClassDB::bind_method(D_METHOD("get_rotates"),          &TamaServerBulletConfig::get_rotates);
    ClassDB::bind_method(D_METHOD("set_rotates","v"),      &TamaServerBulletConfig::set_rotates);
    ClassDB::bind_method(D_METHOD("get_pool_size"),        &TamaServerBulletConfig::get_pool_size);
    ClassDB::bind_method(D_METHOD("set_pool_size","v"),    &TamaServerBulletConfig::set_pool_size);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"),
                 "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_rect"), "set_auto_rect", "get_auto_rect");
    // "rect" is added dynamically via _get_property_list so it can be read-only when auto_rect is true
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"texture_scale"), "set_texture_scale", "get_texture_scale");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "shape_radius"),  "set_shape_radius",  "get_shape_radius");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "collision_layer", PROPERTY_HINT_LAYERS_2D_PHYSICS),
                 "set_collision_layer", "get_collision_layer");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "collision_mask",  PROPERTY_HINT_LAYERS_2D_PHYSICS),
                 "set_collision_mask",  "get_collision_mask");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "rotates"),   "set_rotates",   "get_rotates");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "pool_size"), "set_pool_size", "get_pool_size");
}
