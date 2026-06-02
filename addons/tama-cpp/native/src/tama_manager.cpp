#include "tama_manager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static const char *SETTING_SCRIPTS_PATH = "tama/scripts_path";
static const char *DEFAULT_SCRIPTS_PATH = "res://tamascripts";

TamaManagerBase *TamaManagerBase::s_instance = nullptr;

void TamaManagerBase::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_scripts","path"),              &TamaManagerBase::load_scripts, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("load_script","filename","full_path"),&TamaManagerBase::load_script);
    ClassDB::bind_method(D_METHOD("load_script_from_source","name","source"),&TamaManagerBase::load_script_from_source);
    ClassDB::bind_method(D_METHOD("register_bullet","type","scene"),   &TamaManagerBase::register_bullet);
    ClassDB::bind_method(D_METHOD("register_server_bullet","type","config"),&TamaManagerBase::register_server_bullet);
    ClassDB::bind_method(D_METHOD("set_default_bullet","scene"),       &TamaManagerBase::set_default_bullet);
    ClassDB::bind_method(D_METHOD("get_player_position"),              &TamaManagerBase::get_player_position);
    ClassDB::bind_method(D_METHOD("set_player_position","v"),          &TamaManagerBase::set_player_position);
    ClassDB::bind_method(D_METHOD("get_spawn_parent"),                 &TamaManagerBase::get_spawn_parent);
    ClassDB::bind_method(D_METHOD("set_spawn_parent","v"),             &TamaManagerBase::set_spawn_parent);
    ClassDB::bind_method(D_METHOD("get_context"),                      &TamaManagerBase::get_context);
    ClassDB::bind_method(D_METHOD("set_context","v"),                  &TamaManagerBase::set_context);
    ClassDB::bind_method(D_METHOD("get_registry"),                     &TamaManagerBase::get_registry);
    ClassDB::bind_method(D_METHOD("set_registry","v"),                 &TamaManagerBase::set_registry);
    ClassDB::bind_method(D_METHOD("get_server_bullet_pool"),           &TamaManagerBase::get_server_bullet_pool);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void TamaManagerBase::_ensure_scene_nodes() {
    UtilityFunctions::print("[TAMA] _ensure_scene_nodes injected=", (int)_nodes_injected,
        " spawn_mgr=", (int64_t)(uintptr_t)_spawn_manager);
    if (_nodes_injected) return;
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (!tree || !tree->get_root()) {
        UtilityFunctions::print("[TAMA] _ensure_scene_nodes: no tree root!");
        return;
    }
    _spawn_manager = memnew(TamaSpawnManager);
    _server_pool   = memnew(TamaServerBulletPool);
    _spawn_manager->_repository  = _repository;
    _spawn_manager->_server_pool = _server_pool;
    _spawn_manager->context = Ref<TamaContext>(memnew(TamaContext));
    tree->get_root()->call_deferred("add_child", _spawn_manager);
    tree->get_root()->call_deferred("add_child", _server_pool);
    _nodes_injected = true;
}

void TamaManagerBase::_shutdown() {
    // _spawn_manager and _server_pool are owned by the scene tree and freed by it.
    // Just null the pointers to avoid any double-free.
    _spawn_manager  = nullptr;
    _server_pool    = nullptr;
    _nodes_injected = false;
    if (_repository) {
        memdelete(_repository);
        _repository = nullptr;
    }
    s_instance = nullptr;
}

void TamaManagerBase::_on_scene_nodes_freed() {
    UtilityFunctions::print("[TAMA] _on_scene_nodes_freed called — resetting manager state");
    _spawn_manager  = nullptr;
    _server_pool    = nullptr;
    _nodes_injected = false;
}

// ---------------------------------------------------------------------------
// Script loading
// ---------------------------------------------------------------------------

void TamaManagerBase::load_scripts(const String &path) {
    String p = path.is_empty()
        ? String(ProjectSettings::get_singleton()->get_setting(SETTING_SCRIPTS_PATH, DEFAULT_SCRIPTS_PATH))
        : path;
    UtilityFunctions::print("[TAMA] load_scripts path='", p, "'");
    if (_repository) _repository->load_scripts(p);
    UtilityFunctions::print("[TAMA] load_scripts done");
}

void TamaManagerBase::load_script(const String &filename, const String &full_path) {
    if (_repository) _repository->load_script(filename, full_path);
}

void TamaManagerBase::load_script_from_source(const String &name, const String &source) {
    if (_repository) _repository->load_script_from_source(name, source);
}

// ---------------------------------------------------------------------------
// Bullet registration
// ---------------------------------------------------------------------------

void TamaManagerBase::_ensure_registry() {
    if (!_spawn_manager->registry.is_valid()) {
        _spawn_manager->registry.instantiate();
    }
}

void TamaManagerBase::register_bullet(const String &type, Ref<PackedScene> scene) {
    _ensure_scene_nodes();
    if (!_spawn_manager) return;
    _ensure_registry();
    _spawn_manager->registry->entries[type] = Variant(scene.ptr());
}

void TamaManagerBase::register_server_bullet(const String &type, TamaServerBulletConfig *config) {
    _ensure_scene_nodes();
    if (!_spawn_manager || !_server_pool) return;
    _ensure_registry();
    _spawn_manager->registry->server_configs[type] = Variant(config);
    _server_pool->register_type(type, config);
}

void TamaManagerBase::set_default_bullet(Ref<PackedScene> scene) {
    _ensure_scene_nodes();
    if (!_spawn_manager) return;
    _ensure_registry();
    _spawn_manager->registry->default_bullet = scene;
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

Vector2 TamaManagerBase::get_player_position() const {
    return _spawn_manager ? _spawn_manager->player_position : Vector2();
}
void TamaManagerBase::set_player_position(Vector2 v) {
    static int spp_frame = 0;
    ++spp_frame;
    UtilityFunctions::print("[TAMA] set_player_position frame=", spp_frame,
        " _spawn_manager=", (int64_t)(uintptr_t)_spawn_manager,
        " _nodes_injected=", (int)_nodes_injected);
    if (_spawn_manager) {
        _spawn_manager->player_position = v;
        if (spp_frame <= 5 || spp_frame % 60 == 0)
            UtilityFunctions::print("[TAMA] set_player_position OK frame=", spp_frame);
    }
}
NodePath TamaManagerBase::get_spawn_parent() const {
    return _spawn_manager ? _spawn_manager->spawn_parent : NodePath();
}
void TamaManagerBase::set_spawn_parent(NodePath v) {
    if (_spawn_manager) _spawn_manager->spawn_parent = v;
}
Ref<TamaContext> TamaManagerBase::get_context() const {
    return _spawn_manager ? _spawn_manager->context : Ref<TamaContext>();
}
void TamaManagerBase::set_context(Ref<TamaContext> v) {
    UtilityFunctions::print("[TAMA] set_context v=", (int64_t)(uintptr_t)v.ptr());
    if (_spawn_manager) _spawn_manager->context = v;
    UtilityFunctions::print("[TAMA] set_context done");
}
TamaBulletRegistry *TamaManagerBase::get_registry() const {
    return _spawn_manager ? _spawn_manager->get_registry() : nullptr;
}
void TamaManagerBase::set_registry(TamaBulletRegistry *v) {
    if (_spawn_manager) _spawn_manager->set_registry(v);
}
Object *TamaManagerBase::get_server_bullet_pool() const { return _server_pool; }

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

Object *TamaManagerBase::_get_tama_script(const String &filename) const {
    return _spawn_manager ? _spawn_manager->get_tama_script(filename) : nullptr;
}
bool TamaManagerBase::_has_tama_script(const String &filename) const {
    return _repository ? _repository->has_tama_script(filename) : false;
}
Object *TamaManagerBase::_get_script_from_repository(const String &filename) const {
    return _repository ? _repository->get_tama_script(filename) : nullptr;
}
void TamaManagerBase::_connect_interpreter(Object *interpreter, Node2D *spawner) {
    if (_spawn_manager) _spawn_manager->connect_interpreter(interpreter, spawner);
}
String TamaManagerBase::_get_scripts_path() const {
    return String(ProjectSettings::get_singleton()->get_setting(SETTING_SCRIPTS_PATH, DEFAULT_SCRIPTS_PATH));
}
