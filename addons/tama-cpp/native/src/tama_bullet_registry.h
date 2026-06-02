#pragma once
#include "tama_server_bullet_config.h"

#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

class TamaBulletRegistry : public godot::Resource {
    GDCLASS(TamaBulletRegistry, godot::Resource)
protected:
    static void _bind_methods();
public:
    godot::Dictionary entries;         // String → PackedScene
    godot::Ref<godot::PackedScene> default_bullet;
    godot::Dictionary server_configs;  // String → TamaServerBulletConfig

    godot::Dictionary  get_entries()           const { return entries; }
    void set_entries(godot::Dictionary v)            { entries = v; }
    godot::Ref<godot::PackedScene> get_default_bullet() const { return default_bullet; }
    void set_default_bullet(godot::Ref<godot::PackedScene> v)  { default_bullet = v; }
    godot::Dictionary  get_server_configs()    const { return server_configs; }
    void set_server_configs(godot::Dictionary v)     { server_configs = v; }

    TamaServerBulletConfig *get_server_config(const godot::String &type) const;
};
