#pragma once
#include "tama_bullet_registry.h"
#include "tama_context.h"
#include "tama_script_repository.h"
#include "tama_server_bullet_pool.h"
#include "tama_spawn_manager.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

class TamaManager : public godot::Object {
    GDCLASS(TamaManager, godot::Object)
protected:
    static void _bind_methods();

public:
    static TamaManager *s_instance;
    static TamaManager *get_instance() { return s_instance; }

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------
    void load_scripts(const godot::String &path = "");
    void load_script(const godot::String &filename, const godot::String &full_path);
    void load_script_from_source(const godot::String &script_name, const godot::String &source);

    void register_bullet(const godot::String &type, godot::Ref<godot::PackedScene> scene);
    void register_server_bullet(const godot::String &type, TamaServerBulletConfig *config);
    void set_default_bullet(godot::Ref<godot::PackedScene> scene);

    // Properties
    godot::Vector2 get_player_position() const;
    void set_player_position(godot::Vector2 v);
    godot::NodePath get_spawn_parent() const;
    void set_spawn_parent(godot::NodePath v);
    godot::Ref<TamaContext> get_context() const;
    void set_context(godot::Ref<TamaContext> v);
    TamaBulletRegistry *get_registry() const;
    void set_registry(TamaBulletRegistry *v);
    godot::Object *get_server_bullet_pool() const;

    float get_global_out_of_bounds_margin() const { return _global_out_of_bounds_margin; }
    void  set_global_out_of_bounds_margin(float v) { _global_out_of_bounds_margin = v; }

    int get_bullet_count() const;

    godot::Object *_get_tama_script(const godot::String &filename) const;
    bool           _has_tama_script(const godot::String &filename) const;
    godot::Object *_get_script_from_repository(const godot::String &filename) const;
    void           _connect_interpreter(godot::Object *interpreter, godot::Node2D *spawner);
    godot::Object *_get_context() const { return _spawn_manager ? _spawn_manager->get_context().ptr() : nullptr; }
    godot::String  _get_scripts_path() const;

    void _shutdown();
    void _on_scene_nodes_freed();

    friend void initialize_tama_module(godot::ModuleInitializationLevel);

private:
    _TamaScriptRepository *_repository    = nullptr;
    _TamaSpawnManager     *_spawn_manager = nullptr;
    _TamaServerBulletPool *_server_pool   = nullptr;
    bool                  _nodes_injected = false;
    float _global_out_of_bounds_margin    = -1.0f;

    void _ensure_scene_nodes();
    void _ensure_registry();
};
