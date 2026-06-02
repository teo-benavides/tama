#include "tama_bullet_registry.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void TamaBulletRegistry::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_entries"),             &TamaBulletRegistry::get_entries);
    ClassDB::bind_method(D_METHOD("set_entries", "v"),        &TamaBulletRegistry::set_entries);
    ClassDB::bind_method(D_METHOD("get_default_bullet"),      &TamaBulletRegistry::get_default_bullet);
    ClassDB::bind_method(D_METHOD("set_default_bullet", "v"), &TamaBulletRegistry::set_default_bullet);
    ClassDB::bind_method(D_METHOD("get_server_configs"),      &TamaBulletRegistry::get_server_configs);
    ClassDB::bind_method(D_METHOD("set_server_configs", "v"), &TamaBulletRegistry::set_server_configs);
    ClassDB::bind_method(D_METHOD("get_server_config","type"),&TamaBulletRegistry::get_server_config);

    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "entries"),
                 "set_entries", "get_entries");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_bullet",
                              PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"),
                 "set_default_bullet", "get_default_bullet");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "server_configs"),
                 "set_server_configs", "get_server_configs");
}

TamaServerBulletConfig *TamaBulletRegistry::get_server_config(const String &type) const {
    Variant v = server_configs.get(type, Variant());
    return Object::cast_to<TamaServerBulletConfig>(v.operator Object *());
}
