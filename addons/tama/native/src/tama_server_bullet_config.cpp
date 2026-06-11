#include "tama_server_bullet_config.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>

using namespace godot;

static void _auto_compute_rect(TamaServerBulletConfig *cfg) {
    if (!cfg->texture.is_valid()) return;
    Ref<Texture2D> first = cfg->texture->get_first_texture();
    if (!first.is_valid()) return;
    Vector2i sz = first->get_size();
    cfg->rect = Rect2(-sz.x * 0.5f, -sz.y * 0.5f, (float)sz.x, (float)sz.y);
}

void TamaServerBulletConfig::set_texture(Ref<TamaAnimatedTexture> v) {
    texture = v;
    if (auto_rect) _auto_compute_rect(this);
}

void TamaServerBulletConfig::set_auto_rect(bool v) {
    auto_rect = v;
    if (auto_rect) _auto_compute_rect(this);
    notify_property_list_changed();
}

bool TamaServerBulletConfig::_get(const StringName &p_name, Variant &r_ret) const {
    if (p_name == StringName("rect"))                   { r_ret = rect;                   return true; }
    if (p_name == StringName("face_velocity"))          { r_ret = face_velocity;          return true; }
    if (p_name == StringName("spawn_texture"))          { r_ret = spawn_texture;          return true; }
    if (p_name == StringName("starting_spawn_scale"))   { r_ret = starting_spawn_scale;   return true; }
    if (p_name == StringName("starting_spawn_opacity")) { r_ret = starting_spawn_opacity; return true; }
    return false;
}

bool TamaServerBulletConfig::_set(const StringName &p_name, const Variant &p_value) {
    if (p_name == StringName("rect"))                   { rect                   = p_value; return true; }
    if (p_name == StringName("face_velocity"))          { face_velocity          = p_value; return true; }
    if (p_name == StringName("spawn_texture"))          { spawn_texture          = p_value; return true; }
    if (p_name == StringName("starting_spawn_scale"))   { starting_spawn_scale   = p_value; return true; }
    if (p_name == StringName("starting_spawn_opacity")) { starting_spawn_opacity = p_value; return true; }
    return false;
}

void TamaServerBulletConfig::_get_property_list(List<PropertyInfo> *p_list) const {
    uint32_t rect_usage = auto_rect
        ? (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_READ_ONLY)
        : PROPERTY_USAGE_DEFAULT;
    p_list->push_back(PropertyInfo(Variant::RECT2, "rect", PROPERTY_HINT_NONE, "", rect_usage));

    uint32_t fv_usage = rotates
        ? PROPERTY_USAGE_DEFAULT
        : (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_READ_ONLY);
    p_list->push_back(PropertyInfo(Variant::BOOL, "face_velocity", PROPERTY_HINT_NONE, "", fv_usage));

    p_list->push_back(PropertyInfo(Variant::OBJECT, "spawn_texture",
        PROPERTY_HINT_RESOURCE_TYPE, "TamaAnimatedTexture", PROPERTY_USAGE_DEFAULT));
    p_list->push_back(PropertyInfo(Variant::FLOAT, "starting_spawn_scale",
        PROPERTY_HINT_RANGE, "0.01,10,0.01,or_greater", PROPERTY_USAGE_DEFAULT));
    p_list->push_back(PropertyInfo(Variant::FLOAT, "starting_spawn_opacity",
        PROPERTY_HINT_RANGE, "0,1,0.01", PROPERTY_USAGE_DEFAULT));
}

void TamaServerBulletConfig::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_texture"),          &TamaServerBulletConfig::get_texture);
    ClassDB::bind_method(D_METHOD("set_texture", "v"),     &TamaServerBulletConfig::set_texture);
    ClassDB::bind_method(D_METHOD("get_auto_rect"),          &TamaServerBulletConfig::get_auto_rect);
    ClassDB::bind_method(D_METHOD("set_auto_rect", "v"),     &TamaServerBulletConfig::set_auto_rect);
    ClassDB::bind_method(D_METHOD("get_rect"),               &TamaServerBulletConfig::get_rect);
    ClassDB::bind_method(D_METHOD("set_rect", "v"),          &TamaServerBulletConfig::set_rect);
    ClassDB::bind_method(D_METHOD("get_texture_scale"),    &TamaServerBulletConfig::get_texture_scale);
    ClassDB::bind_method(D_METHOD("set_texture_scale","v"),&TamaServerBulletConfig::set_texture_scale);
    ClassDB::bind_method(D_METHOD("get_shape_radius"),     &TamaServerBulletConfig::get_shape_radius);
    ClassDB::bind_method(D_METHOD("set_shape_radius","v"), &TamaServerBulletConfig::set_shape_radius);
    ClassDB::bind_method(D_METHOD("get_rotates"),          &TamaServerBulletConfig::get_rotates);
    ClassDB::bind_method(D_METHOD("set_rotates","v"),      &TamaServerBulletConfig::set_rotates);
    ClassDB::bind_method(D_METHOD("get_face_velocity"),    &TamaServerBulletConfig::get_face_velocity);
    ClassDB::bind_method(D_METHOD("set_face_velocity","v"),&TamaServerBulletConfig::set_face_velocity);
    ClassDB::bind_method(D_METHOD("get_spawn_delay"),                  &TamaServerBulletConfig::get_spawn_delay);
    ClassDB::bind_method(D_METHOD("set_spawn_delay","v"),              &TamaServerBulletConfig::set_spawn_delay);
    ClassDB::bind_method(D_METHOD("get_spawn_texture"),                &TamaServerBulletConfig::get_spawn_texture);
    ClassDB::bind_method(D_METHOD("set_spawn_texture","v"),            &TamaServerBulletConfig::set_spawn_texture);
    ClassDB::bind_method(D_METHOD("get_starting_spawn_scale"),         &TamaServerBulletConfig::get_starting_spawn_scale);
    ClassDB::bind_method(D_METHOD("set_starting_spawn_scale","v"),     &TamaServerBulletConfig::set_starting_spawn_scale);
    ClassDB::bind_method(D_METHOD("get_starting_spawn_opacity"),       &TamaServerBulletConfig::get_starting_spawn_opacity);
    ClassDB::bind_method(D_METHOD("set_starting_spawn_opacity","v"),   &TamaServerBulletConfig::set_starting_spawn_opacity);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture",
                 PROPERTY_HINT_RESOURCE_TYPE, "TamaAnimatedTexture"),
                 "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_rect"), "set_auto_rect", "get_auto_rect");
    // "rect" is dynamic via _get_property_list (read-only when auto_rect is true)
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2,"texture_scale"), "set_texture_scale", "get_texture_scale");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "shape_radius"),  "set_shape_radius",  "get_shape_radius");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rotates"),       "set_rotates",       "get_rotates");
    // "face_velocity" is dynamic via _get_property_list (read-only when rotates is false)
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "spawn_delay", PROPERTY_HINT_RANGE, "0,300,1,or_greater"),
                 "set_spawn_delay", "get_spawn_delay");
    // spawn_texture, starting_spawn_scale, starting_spawn_opacity are dynamic (via _get_property_list)
}
