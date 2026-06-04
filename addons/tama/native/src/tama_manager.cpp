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

TamaManager *TamaManager::s_instance = nullptr;

void TamaManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_scripts","path"),              &TamaManager::load_scripts, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("load_script","filename","full_path"),&TamaManager::load_script);
    ClassDB::bind_method(D_METHOD("load_script_from_source","name","source"),&TamaManager::load_script_from_source);
    ClassDB::bind_method(D_METHOD("register_bullet","type","scene"),   &TamaManager::register_bullet);
    ClassDB::bind_method(D_METHOD("register_server_bullet","type","config"),&TamaManager::register_server_bullet);
    ClassDB::bind_method(D_METHOD("set_default_bullet","scene"),       &TamaManager::set_default_bullet);
    ClassDB::bind_method(D_METHOD("get_player_position"),              &TamaManager::get_player_position);
    ClassDB::bind_method(D_METHOD("set_player_position","v"),          &TamaManager::set_player_position);
    ClassDB::bind_method(D_METHOD("get_spawn_parent"),                 &TamaManager::get_spawn_parent);
    ClassDB::bind_method(D_METHOD("set_spawn_parent","v"),             &TamaManager::set_spawn_parent);
    ClassDB::bind_method(D_METHOD("get_context"),                      &TamaManager::get_context);
    ClassDB::bind_method(D_METHOD("set_context","v"),                  &TamaManager::set_context);
    ClassDB::bind_method(D_METHOD("get_registry"),                     &TamaManager::get_registry);
    ClassDB::bind_method(D_METHOD("set_registry","v"),                 &TamaManager::set_registry);
    ClassDB::bind_method(D_METHOD("get_server_bullet_pool"),                 &TamaManager::get_server_bullet_pool);
    ClassDB::bind_method(D_METHOD("destroy_server_bullet","bullet"),         &TamaManager::destroy_server_bullet);
    ClassDB::bind_method(D_METHOD("get_global_out_of_bounds_margin"),        &TamaManager::get_global_out_of_bounds_margin);
    ClassDB::bind_method(D_METHOD("set_global_out_of_bounds_margin","v"),    &TamaManager::set_global_out_of_bounds_margin);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "global_out_of_bounds_margin",
                              PROPERTY_HINT_RANGE, "-1,1000,1,or_greater"),
                 "set_global_out_of_bounds_margin", "get_global_out_of_bounds_margin");
    ClassDB::bind_method(D_METHOD("get_bullet_count"),                       &TamaManager::get_bullet_count);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "bullet_count",
                              PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
                 "", "get_bullet_count");
    ClassDB::bind_method(D_METHOD("_on_pool_bullet_hit", "bullet", "body_instance_id"), &TamaManager::_on_pool_bullet_hit);
    ADD_SIGNAL(MethodInfo("bullet_hit",
        PropertyInfo(Variant::OBJECT, "bullet", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "TamaServerBullet"),
        PropertyInfo(Variant::INT,    "body_instance_id")));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void TamaManager::_ensure_scene_nodes() {
    if (_nodes_injected) return;
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (!tree || !tree->get_root()) return;
    _spawn_manager = memnew(_TamaSpawnManager);
    _server_pool   = memnew(TamaServerBulletPool);
    _spawn_manager->_repository  = _repository;
    _spawn_manager->_server_pool = _server_pool;
    _spawn_manager->context = Ref<TamaContext>(memnew(TamaContext));
    _server_pool->connect("bullet_hit", callable_mp(this, &TamaManager::_on_pool_bullet_hit));
    tree->get_root()->call_deferred("add_child", _spawn_manager);
    tree->get_root()->call_deferred("add_child", _server_pool);
    _nodes_injected = true;
}

void TamaManager::_shutdown() {
    _spawn_manager  = nullptr;
    _server_pool    = nullptr;
    _nodes_injected = false;
    if (_repository) {
        memdelete(_repository);
        _repository = nullptr;
    }
    s_instance = nullptr;
}

void TamaManager::_on_scene_nodes_freed() {
    _spawn_manager  = nullptr;
    _server_pool    = nullptr;
    _nodes_injected = false;
}

// ---------------------------------------------------------------------------
// Script loading
// ---------------------------------------------------------------------------

void TamaManager::load_scripts(const String &path) {
    String p = path.is_empty()
        ? String(ProjectSettings::get_singleton()->get_setting(SETTING_SCRIPTS_PATH, DEFAULT_SCRIPTS_PATH))
        : path;
    if (_repository) _repository->load_scripts(p);
}

void TamaManager::load_script(const String &filename, const String &full_path) {
    if (_repository) _repository->load_script(filename, full_path);
}

void TamaManager::load_script_from_source(const String &name, const String &source) {
    if (_repository) _repository->load_script_from_source(name, source);
}

// ---------------------------------------------------------------------------
// Bullet registration
// ---------------------------------------------------------------------------

void TamaManager::_ensure_registry() {
    if (!_spawn_manager->registry.is_valid()) {
        _spawn_manager->registry.instantiate();
    }
}

void TamaManager::register_bullet(const String &type, Ref<PackedScene> scene) {
    _ensure_scene_nodes();
    if (!_spawn_manager) return;
    _ensure_registry();
    _spawn_manager->registry->scene_bullets[type] = Variant(scene.ptr());
}

void TamaManager::register_server_bullet(const String &type, TamaServerBulletConfig *config) {
    _ensure_scene_nodes();
    if (!_spawn_manager || !_server_pool) return;
    _ensure_registry();
    _spawn_manager->registry->server_bullets[type] = Variant(config);
    _server_pool->register_type(type, config);
}

void TamaManager::set_default_bullet(Ref<PackedScene> scene) {
    _ensure_scene_nodes();
    if (!_spawn_manager) return;
    _ensure_registry();
    _spawn_manager->registry->default_scene_bullet = scene;
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

Vector2 TamaManager::get_player_position() const {
    return _spawn_manager ? _spawn_manager->player_position : Vector2();
}
void TamaManager::set_player_position(Vector2 v) {
    if (_spawn_manager) _spawn_manager->player_position = v;
}
NodePath TamaManager::get_spawn_parent() const {
    return _spawn_manager ? _spawn_manager->spawn_parent : NodePath();
}
void TamaManager::set_spawn_parent(NodePath v) {
    if (_spawn_manager) _spawn_manager->spawn_parent = v;
}
Ref<TamaContext> TamaManager::get_context() const {
    return _spawn_manager ? _spawn_manager->context : Ref<TamaContext>();
}
void TamaManager::set_context(Ref<TamaContext> v) {
    _ensure_scene_nodes();
    if (_spawn_manager) _spawn_manager->context = v;
}
TamaBulletRegistry *TamaManager::get_registry() const {
    return _spawn_manager ? _spawn_manager->get_registry() : nullptr;
}
void TamaManager::set_registry(TamaBulletRegistry *v) {
    _ensure_scene_nodes();
    if (!_spawn_manager) return;
    _spawn_manager->set_registry(v);
    if (!v || !_server_pool) return;
    // Register any server bullet types already in the registry with the pool.
    Dictionary cfgs = v->server_bullets;
    Array keys = cfgs.keys();
    for (int i = 0; i < keys.size(); ++i) {
        String type = keys[i];
        Object *cfg = Object::cast_to<Object>(cfgs[type].operator Object *());
        if (cfg) _server_pool->register_type(type, cfg);
    }
    // Register default_server_bullet under "" so typeless bullets can find its TypeData.
    if (v->default_server_bullet.is_valid())
        _server_pool->register_type(String(""), v->default_server_bullet.ptr());
}
Object *TamaManager::get_server_bullet_pool() const { return _server_pool; }

void TamaManager::destroy_server_bullet(Object *p_bullet) {
    // call_deferred so this is safe to call from inside the bullet_hit signal handler,
    // which fires during a physics query flush where area_set_shape_disabled is forbidden.
    if (_server_pool) _server_pool->call_deferred("recycle", p_bullet);
}

void TamaManager::_on_pool_bullet_hit(Object *bullet, int64_t body_instance_id) {
    emit_signal("bullet_hit", bullet, body_instance_id);
}

int TamaManager::get_bullet_count() const {
    int server_count = _server_pool  ? _server_pool->get_active_count()      : 0;
    int scene_count  = _spawn_manager ? _spawn_manager->get_scene_bullet_count() : 0;
    return server_count + scene_count;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

Object *TamaManager::_get_tama_script(const String &filename) const {
    return _spawn_manager ? _spawn_manager->get_tama_script(filename) : nullptr;
}
bool TamaManager::_has_tama_script(const String &filename) const {
    return _repository ? _repository->has_tama_script(filename) : false;
}
Object *TamaManager::_get_script_from_repository(const String &filename) const {
    return _repository ? _repository->get_tama_script(filename) : nullptr;
}
void TamaManager::_connect_interpreter(Object *interpreter, Object *spawner) {
    if (_spawn_manager) _spawn_manager->connect_interpreter(interpreter, spawner);
}
String TamaManager::_get_scripts_path() const {
    return String(ProjectSettings::get_singleton()->get_setting(SETTING_SCRIPTS_PATH, DEFAULT_SCRIPTS_PATH));
}
