#include "register_types.h"
#include "tama_animated_texture.h"
#include "tama_ast_nodes.h"
#include "tama_bullet.h"
#include "tama_bullet_registry.h"
#include "tama_context.h"
#include "tama_emitter.h"
#include "tama_expr.h"
#include "tama_interpreter.h"
#include "tama_manager.h"
#include "tama_script_repository.h"
#include "tama_server_bullet.h"
#include "tama_server_bullet_config.h"
#include "tama_server_bullet_pool.h"
#include "tama_server_curved_laser.h"
#include "tama_server_curved_laser_config.h"
#include "tama_server_curved_laser_pool.h"
#include "tama_server_laser.h"
#include "tama_server_laser_pool.h"
#include "tama_server_object_config.h"
#include "tama_server_straight_laser_config.h"
#include "tama_spawn_manager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static _TamaExprRuntime *s_expr_runtime  = nullptr;
static TamaManager *s_tama_manager  = nullptr;

void initialize_tama_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Internal classes — not visible in the editor or docs
    ClassDB::register_internal_class<_TamaScriptRepository>();
    ClassDB::register_internal_class<_TamaSpawnManager>();
    ClassDB::register_internal_class<_TamaExprRuntime>();
    ClassDB::register_internal_class<TamaServerBulletPool>();
    ClassDB::register_internal_class<TamaServerLaserPool>();
    ClassDB::register_internal_class<TamaServerCurvedLaserPool>();

    // User-facing classes — base before derived
    ClassDB::register_class<TamaAnimatedTexture>();
    ClassDB::register_class<TamaServerObjectConfig>();
    ClassDB::register_class<TamaServerBulletConfig>();
    ClassDB::register_class<TamaServerStraightLaserConfig>();
    ClassDB::register_class<TamaServerCurvedLaserConfig>();
    ClassDB::register_class<TamaServerBullet>();
    ClassDB::register_class<TamaServerLaser>();
    ClassDB::register_class<TamaServerCurvedLaser>();
    ClassDB::register_class<TamaContext>();
    ClassDB::register_class<TamaBulletRegistry>();
    ClassDB::register_class<TamaBullet>();
    ClassDB::register_class<TamaEmitter>();
    ClassDB::register_class<TamaManager>();

    s_expr_runtime = memnew(_TamaExprRuntime);
    Engine::get_singleton()->register_singleton("_TamaExprRuntime", s_expr_runtime);

    s_tama_manager = memnew(TamaManager);
    s_tama_manager->_repository = memnew(_TamaScriptRepository);
    Engine::get_singleton()->register_singleton("TamaManager", s_tama_manager);
    TamaManager::s_instance = s_tama_manager;
}

void uninitialize_tama_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    if (s_tama_manager) {
        Engine::get_singleton()->unregister_singleton("TamaManager");
        s_tama_manager->_shutdown();
        memdelete(s_tama_manager);
        s_tama_manager = nullptr;
    }
    if (s_expr_runtime) {
        Engine::get_singleton()->unregister_singleton("_TamaExprRuntime");
        memdelete(s_expr_runtime);
        s_expr_runtime = nullptr;
    }
}

extern "C" {
GDExtensionBool GDE_EXPORT tama_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        const GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization) {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_tama_module);
    init_obj.register_terminator(uninitialize_tama_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
