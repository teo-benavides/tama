#include "tama_bullet_registry.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void TamaBulletRegistry::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_default_scene_bullet"),           &TamaBulletRegistry::get_default_scene_bullet);
    ClassDB::bind_method(D_METHOD("set_default_scene_bullet", "v"),      &TamaBulletRegistry::set_default_scene_bullet);
    ClassDB::bind_method(D_METHOD("get_default_server_bullet"),          &TamaBulletRegistry::get_default_server_bullet);
    ClassDB::bind_method(D_METHOD("set_default_server_bullet", "v"),     &TamaBulletRegistry::set_default_server_bullet);
    ClassDB::bind_method(D_METHOD("get_scene_bullets"),                  &TamaBulletRegistry::get_scene_bullets);
    ClassDB::bind_method(D_METHOD("set_scene_bullets", "v"),             &TamaBulletRegistry::set_scene_bullets);
    ClassDB::bind_method(D_METHOD("get_server_bullets"),                 &TamaBulletRegistry::get_server_bullets);
    ClassDB::bind_method(D_METHOD("set_server_bullets", "v"),            &TamaBulletRegistry::set_server_bullets);
    ClassDB::bind_method(D_METHOD("get_default_to_server_bullets"),      &TamaBulletRegistry::get_default_to_server_bullets);
    ClassDB::bind_method(D_METHOD("set_default_to_server_bullets", "v"), &TamaBulletRegistry::set_default_to_server_bullets);
    ClassDB::bind_method(D_METHOD("get_server_config", "type"),          &TamaBulletRegistry::get_server_config);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_scene_bullet",
                              PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"),
                 "set_default_scene_bullet", "get_default_scene_bullet");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_server_bullet",
                              PROPERTY_HINT_RESOURCE_TYPE, "TamaServerBulletConfig"),
                 "set_default_server_bullet", "get_default_server_bullet");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "scene_bullets",
                              PROPERTY_HINT_DICTIONARY_TYPE, "String;PackedScene"),
                 "set_scene_bullets", "get_scene_bullets");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "server_bullets",
                              PROPERTY_HINT_DICTIONARY_TYPE, "String;TamaServerBulletConfig"),
                 "set_server_bullets", "get_server_bullets");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_to_server_bullets"),
                 "set_default_to_server_bullets", "get_default_to_server_bullets");
}

TamaServerBulletConfig *TamaBulletRegistry::get_server_config(const String &type) const {
    Variant v = server_bullets.get(type, Variant());
    return Object::cast_to<TamaServerBulletConfig>(v.operator Object *());
}
