#pragma once
#include "tama_server_object_config.h"
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
    godot::Ref<godot::PackedScene>     default_scene_bullet;
    godot::Ref<TamaServerBulletConfig> default_server_bullet;
    godot::Dictionary                  scene_bullets;  // String → PackedScene
    godot::Dictionary                  objects;        // String → TamaServerObjectConfig (any subclass)
    bool                               default_to_server_bullets = false;

    godot::Ref<godot::PackedScene>     get_default_scene_bullet()  const { return default_scene_bullet; }
    void set_default_scene_bullet(godot::Ref<godot::PackedScene> v)      { default_scene_bullet = v; }
    godot::Ref<TamaServerBulletConfig> get_default_server_bullet() const { return default_server_bullet; }
    void set_default_server_bullet(godot::Ref<TamaServerBulletConfig> v) { default_server_bullet = v; }
    godot::Dictionary  get_scene_bullets()  const { return scene_bullets; }
    void set_scene_bullets(godot::Dictionary v)   { scene_bullets = v; }
    godot::Dictionary  get_objects()        const { return objects; }
    void set_objects(godot::Dictionary v)         { objects = v; }
    bool get_default_to_server_bullets()    const { return default_to_server_bullets; }
    void set_default_to_server_bullets(bool v)    { default_to_server_bullets = v; }

    // Returns the config for the given type key, or null if not found.
    TamaServerObjectConfig *get_object_config(const godot::String &type) const;
    // Convenience: returns TamaServerBulletConfig if the type maps to one, else null.
    TamaServerBulletConfig *get_server_config(const godot::String &type) const;
};
