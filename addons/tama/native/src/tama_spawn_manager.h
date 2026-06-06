#pragma once
#include "tama_bullet.h"
#include "tama_bullet_registry.h"
#include "tama_context.h"
#include "tama_interpreter.h"
#include "tama_script_repository.h"
#include "tama_server_bullet_pool.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector2.hpp>

class _TamaSpawnManager : public godot::Node {
    GDCLASS(_TamaSpawnManager, godot::Node)
protected:
    static void _bind_methods();

public:
    godot::Ref<TamaBulletRegistry> registry;
    _TamaScriptRepository*_repository  = nullptr;
    TamaServerBulletPool*_server_pool = nullptr;
    godot::Ref<TamaContext> context;
    godot::Vector2 player_position;
    godot::NodePath spawn_parent;

    void _exit_tree();

    // spawner may be a Node2D (TamaEmitter / TamaBullet) or a TamaServerBullet (Object)
    void connect_interpreter(godot::Object *interpreter, godot::Object *spawner);
    godot::Object *get_tama_script(const godot::String &filename) const;

    TamaBulletRegistry *get_registry() const { return registry.ptr(); }
    void set_registry(TamaBulletRegistry *v)  { registry = godot::Ref<TamaBulletRegistry>(v); }
    godot::Ref<TamaContext> get_context() const { return context; }
    void set_context(godot::Ref<TamaContext> v) { context = v; }
    godot::Vector2 get_player_position() const { return player_position; }
    void set_player_position(godot::Vector2 v) { player_position = v; }
    godot::NodePath get_spawn_parent() const { return spawn_parent; }
    void set_spawn_parent(godot::NodePath v)  { spawn_parent = v; }

    int get_scene_bullet_count() const;

private:
    void _on_bullet_fired(const TamaBulletFireData &data, godot::Object *spawner);

    float _resolve_angle(const TamaBulletFireData &data, godot::Object *spawner) const;
    float _resolve_speed(const TamaBulletFireData &data, godot::Object *spawner) const;
    godot::Vector2 _resolve_position(const TamaBulletFireData &data, godot::Object *spawner, float angle) const;
    godot::Node *_get_spawn_parent() const;
};
