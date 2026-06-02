#include "tama_manager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static const char *SETTING_SCRIPTS_PATH = "tama/scripts_path";
static const char *DEFAULT_SCRIPTS_PATH = "res://tamascripts";

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
    ClassDB::bind_method(D_METHOD("_get_tama_script","filename"),      &TamaManagerBase::_get_tama_script);
    ClassDB::bind_method(D_METHOD("_has_tama_script","filename"),      &TamaManagerBase::_has_tama_script);
    ClassDB::bind_method(D_METHOD("_get_script_from_repository","filename"),&TamaManagerBase::_get_script_from_repository);
    ClassDB::bind_method(D_METHOD("_connect_interpreter","interpreter","spawner"),&TamaManagerBase::_connect_interpreter);
    ClassDB::bind_method(D_METHOD("_get_context"),                     &TamaManagerBase::_get_context);
    ClassDB::bind_method(D_METHOD("_get_scripts_path"),                &TamaManagerBase::_get_scripts_path);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "player_position",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "set_player_position","get_player_position");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH,"spawn_parent",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "set_spawn_parent","get_spawn_parent");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT,"context",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "set_context","get_context");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT,"registry",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "set_registry","get_registry");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT,"server_bullet_pool",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NONE),
                 "","get_server_bullet_pool");
}

void TamaManagerBase::_enter_tree() {
    Engine::get_singleton()->register_singleton("TamaManager", this);
    _repository    = memnew(TamaScriptRepository);
    _spawn_manager = memnew(TamaSpawnManager);
    _server_pool   = memnew(TamaServerBulletPool);
    _spawn_manager->_repository  = _repository;
    _spawn_manager->_server_pool = _server_pool;
    _spawn_manager->context = Ref<TamaContext>(memnew(TamaContext));

    add_child(_repository);
    add_child(_spawn_manager);
    add_child(_server_pool);
}

void TamaManagerBase::_exit_tree() {
    Engine::get_singleton()->unregister_singleton("TamaManager");
}

void TamaManagerBase::_ready() {
    // Intentionally empty — load_scripts() is called by the GDScript subclass _ready()
    // so it can use GDScript's ProjectSettings access for the correct path fallback.
}

// ---------------------------------------------------------------------------
// Script loading
// ---------------------------------------------------------------------------

void TamaManagerBase::load_scripts(const String &path) {
    String p = path.is_empty()
        ? String(ProjectSettings::get_singleton()->get_setting(SETTING_SCRIPTS_PATH, DEFAULT_SCRIPTS_PATH))
        : path;
    if (_repository) _repository->load_scripts(p);
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
    if (!_spawn_manager->registry) {
        _spawn_manager->registry = memnew(TamaBulletRegistry);
    }
}

void TamaManagerBase::register_bullet(const String &type, Ref<PackedScene> scene) {
    _ensure_registry();
    _spawn_manager->registry->entries[type] = Variant(scene.ptr());
}

void TamaManagerBase::register_server_bullet(const String &type, TamaServerBulletConfig *config) {
    _ensure_registry();
    _spawn_manager->registry->server_configs[type] = Variant(config);
    _server_pool->register_type(type, config);
}

void TamaManagerBase::set_default_bullet(Ref<PackedScene> scene) {
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
    if (_spawn_manager) _spawn_manager->player_position = v;
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
    if (_spawn_manager) _spawn_manager->context = v;
}
TamaBulletRegistry *TamaManagerBase::get_registry() const {
    return _spawn_manager ? _spawn_manager->registry : nullptr;
}
void TamaManagerBase::set_registry(TamaBulletRegistry *v) {
    if (_spawn_manager) _spawn_manager->registry = v;
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
