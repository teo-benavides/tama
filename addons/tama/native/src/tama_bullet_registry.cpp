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
    ClassDB::bind_method(D_METHOD("get_objects"),                        &TamaBulletRegistry::get_objects);
    ClassDB::bind_method(D_METHOD("set_objects", "v"),                   &TamaBulletRegistry::set_objects);
    ClassDB::bind_method(D_METHOD("get_default_to_server_bullets"),      &TamaBulletRegistry::get_default_to_server_bullets);
    ClassDB::bind_method(D_METHOD("set_default_to_server_bullets", "v"), &TamaBulletRegistry::set_default_to_server_bullets);
    ClassDB::bind_method(D_METHOD("get_object_config", "type"),          &TamaBulletRegistry::get_object_config);
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
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "objects",
                              PROPERTY_HINT_DICTIONARY_TYPE, "String;TamaServerObjectConfig"),
                 "set_objects", "get_objects");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_to_server_bullets"),
                 "set_default_to_server_bullets", "get_default_to_server_bullets");
}

TamaServerObjectConfig *TamaBulletRegistry::get_object_config(const String &type) const {
    Variant v = objects.get(type, Variant());
    return Object::cast_to<TamaServerObjectConfig>(v.operator Object *());
}

TamaServerBulletConfig *TamaBulletRegistry::get_server_config(const String &type) const {
    return Object::cast_to<TamaServerBulletConfig>(get_object_config(type));
}
