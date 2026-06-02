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

class TamaSpawnManager : public godot::Node {
    GDCLASS(TamaSpawnManager, godot::Node)
protected:
    static void _bind_methods();

public:
    TamaBulletRegistry  *registry     = nullptr;
    TamaScriptRepository*_repository  = nullptr;
    TamaServerBulletPool*_server_pool = nullptr;
    godot::Ref<TamaContext> context;
    godot::Vector2 player_position;
    godot::NodePath spawn_parent;

    void connect_interpreter(godot::Object *interpreter, godot::Node2D *spawner);
    godot::Object *get_tama_script(const godot::String &filename) const;

    TamaBulletRegistry *get_registry() const { return registry; }
    void set_registry(TamaBulletRegistry *v)  { registry = v; }
    godot::Ref<TamaContext> get_context() const { return context; }
    void set_context(godot::Ref<TamaContext> v) { context = v; }
    godot::Vector2 get_player_position() const { return player_position; }
    void set_player_position(godot::Vector2 v) { player_position = v; }
    godot::NodePath get_spawn_parent() const { return spawn_parent; }
    void set_spawn_parent(godot::NodePath v)  { spawn_parent = v; }

private:
    void _on_bullet_fired(godot::Variant data_v, godot::Node2D *spawner);

    float _resolve_angle(godot::Object *data, godot::Node2D *spawner) const;
    float _resolve_speed(godot::Object *data, godot::Node2D *spawner) const;
    godot::Vector2 _resolve_position(godot::Object *data, godot::Node2D *spawner, float angle) const;
    godot::Node *_get_spawn_parent() const;
};
