#include "register_types.h"
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
#include "tama_spawn_manager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static TamaExprRuntime *s_expr_runtime = nullptr;

void initialize_tama_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<TamaASTNode>();
    ClassDB::register_class<TamaScriptRepository>();
    ClassDB::register_class<TamaContext>();
    ClassDB::register_class<TamaServerBulletConfig>();
    ClassDB::register_class<TamaBulletRegistry>();
    ClassDB::register_class<TamaBullet>();
    ClassDB::register_class<TamaEmitter>();
    ClassDB::register_class<TamaSpawnManager>();
    ClassDB::register_class<TamaManagerBase>();
    ClassDB::register_class<TamaExprRuntime>();
    ClassDB::register_class<TamaBulletFireData>();
    ClassDB::register_class<TamaChdirData>();
    ClassDB::register_class<TamaChspdData>();
    ClassDB::register_class<TamaChposData>();
    ClassDB::register_class<TamaAccelData>();
    ClassDB::register_class<TamaRef>();
    ClassDB::register_class<TamaInterpreter>();
    ClassDB::register_class<TamaServerBullet>();
    ClassDB::register_class<TamaServerBulletPool>();

    s_expr_runtime = memnew(TamaExprRuntime);
    Engine::get_singleton()->register_singleton("TamaExprRuntime", s_expr_runtime);
}

void uninitialize_tama_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    if (s_expr_runtime) {
        Engine::get_singleton()->unregister_singleton("TamaExprRuntime");
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
