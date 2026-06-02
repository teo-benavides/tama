#include "tama_emitter.h"
#include "tama_manager.h"
#include "tama_script_repository.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void TamaEmitter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_script_filename"),        &TamaEmitter::get_script_filename);
    ClassDB::bind_method(D_METHOD("set_script_filename","v"),    &TamaEmitter::set_script_filename);
    ClassDB::bind_method(D_METHOD("get_excluded_from_group"),    &TamaEmitter::get_excluded_from_group);
    ClassDB::bind_method(D_METHOD("set_excluded_from_group","v"),&TamaEmitter::set_excluded_from_group);
    ClassDB::bind_method(D_METHOD("start"),                      &TamaEmitter::start);
    ClassDB::bind_method(D_METHOD("stop"),                       &TamaEmitter::stop);
    ClassDB::bind_method(D_METHOD("set_export","name","value"),  &TamaEmitter::set_export);
    ClassDB::bind_method(D_METHOD("set_export_num","name","value"),  &TamaEmitter::set_export_num);
    ClassDB::bind_method(D_METHOD("set_export_str","name","value"),  &TamaEmitter::set_export_str);
    ClassDB::bind_method(D_METHOD("set_export_bool","name","value"), &TamaEmitter::set_export_bool);
    ClassDB::bind_method(D_METHOD("get_export","name"),          &TamaEmitter::get_export);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "script_filename"), "set_script_filename","get_script_filename");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "excluded_from_group"), "set_excluded_from_group","get_excluded_from_group");
}

// ---------------------------------------------------------------------------
// Property overrides for dynamic tama_export_* fields
// ---------------------------------------------------------------------------

bool TamaEmitter::_get(const StringName &p_name, Variant &r_ret) const {
    String name(p_name);
    if (name.begins_with("tama_export_")) {
        r_ret = _export_values.get(name.substr(12), Variant());
        return true;
    }
    return false;
}

bool TamaEmitter::_set(const StringName &p_name, const Variant &p_value) {
    String name(p_name);
    if (name.begins_with("tama_export_")) {
        _export_values[name.substr(12)] = p_value;
        return true;
    }
    return false;
}

bool TamaEmitter::_property_can_revert(const StringName &p_name) const {
    String name(p_name);
    if (!name.begins_with("tama_export_")) return false;
    String key = name.substr(12);
    for (const ExportDef &def : _cached_export_defs) {
        if (def.name == key) return def.default_value.get_type() != Variant::NIL;
    }
    return false;
}

bool TamaEmitter::_property_get_revert(const StringName &p_name, Variant &r_ret) const {
    String name(p_name);
    if (!name.begins_with("tama_export_")) return false;
    String key = name.substr(12);
    for (const ExportDef &def : _cached_export_defs) {
        if (def.name == key) { r_ret = def.default_value; return true; }
    }
    return false;
}

void TamaEmitter::_get_property_list(List<PropertyInfo> *p_list) const {
    if (_cached_export_defs.empty()) return;

    p_list->push_back(PropertyInfo(Variant::NIL, "TamaScript Exports",
        PROPERTY_HINT_NONE, "tama_export_", PROPERTY_USAGE_GROUP));

    for (const ExportDef &def : _cached_export_defs) {
        Variant::Type vtype;
        if      (def.type == "num")  vtype = Variant::FLOAT;
        else if (def.type == "bool") vtype = Variant::BOOL;
        else                         vtype = Variant::STRING;
        p_list->push_back(PropertyInfo(vtype, String("tama_export_") + def.name,
            PROPERTY_HINT_NONE, "",
            PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE));
    }
}

// ---------------------------------------------------------------------------
// Godot virtuals
// ---------------------------------------------------------------------------

void TamaEmitter::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        _refresh_exports();
        _last_modified_time = _get_script_mtime();
        notify_property_list_changed();
        return;
    }
    if (!_excluded_from_group) add_to_group("tama_emitters");
}

void TamaEmitter::_process(double delta) {
    if (!Engine::get_singleton()->is_editor_hint()) return;
    _poll_timer += (float)delta;
    if (_poll_timer < POLL_INTERVAL) return;
    _poll_timer = 0.0f;
    _check_file_changed();
}

// ---------------------------------------------------------------------------
// script_filename setter
// ---------------------------------------------------------------------------

void TamaEmitter::set_script_filename(const String &v) {
    _script_filename = v;
    String old_sig = _exports_signature();
    _refresh_exports();
    _last_modified_time = _get_script_mtime();
    if (_exports_signature() != old_sig) notify_property_list_changed();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TamaEmitter::start() {
    UtilityFunctions::print("[TAMA] TamaEmitter::start script='", _script_filename, "'");
    if (_script_filename.is_empty()) {
        UtilityFunctions::push_error("TamaEmitter: script_filename is not set");
        return;
    }
    TamaManagerBase *mgr = TamaManagerBase::get_instance();
    if (!mgr) { UtilityFunctions::print("[TAMA] TamaEmitter::start: no mgr"); return; }
    Object *program = mgr->_get_tama_script(_script_filename);
    UtilityFunctions::print("[TAMA] TamaEmitter::start program=", (int64_t)(uintptr_t)program);
    if (!program) {
        UtilityFunctions::push_error(String("TamaEmitter: script '") + _script_filename + "' not found in repository (check tama/scripts_path project setting)");
        return;
    }

    _interpreter = memnew(TamaInterpreter);
    UtilityFunctions::print("[TAMA] TamaEmitter::start: interpreter created");
    _interpreter->set_context(mgr->_get_context());
    UtilityFunctions::print("[TAMA] TamaEmitter::start: context set");
    _interpreter->connect("finished", callable_mp(this, &TamaEmitter::stop));
    mgr->_connect_interpreter(_interpreter, this);
    UtilityFunctions::print("[TAMA] TamaEmitter::start: interpreter connected");
    add_child(_interpreter);
    UtilityFunctions::print("[TAMA] TamaEmitter::start: interpreter added as child");
    _running = true;
    Dictionary scope = _export_values.duplicate();
    _interpreter->start(program, scope);
    UtilityFunctions::print("[TAMA] TamaEmitter::start: interpreter started");
}

void TamaEmitter::stop() {
    if (_interpreter && _running) {
        _interpreter->stop();
    }
    _running = false;
}

// ---------------------------------------------------------------------------
// Editor helpers
// ---------------------------------------------------------------------------

void TamaEmitter::_read_exports_from(Object *program) {
    Array exports = (Array)program->get("exports");
    for (int i = 0; i < exports.size(); ++i) {
        Object *exp = Object::cast_to<Object>(exports[i].operator Object *());
        if (!exp) continue;
        ExportDef def;
        def.name          = (String)exp->get("name");
        def.type          = (String)exp->get("export_type");
        def.default_value = exp->get("default_value");
        _cached_export_defs.push_back(def);
        if (def.default_value.get_type() != Variant::NIL && !_export_values.has(def.name))
            _export_values[def.name] = def.default_value;
    }
}

void TamaEmitter::_refresh_exports() {
    _cached_export_defs.clear();
    if (_script_filename.is_empty()) return;

    if (Engine::get_singleton()->is_editor_hint()) {
        String path = _find_script_path();
        if (path.is_empty()) return;
        String source = FileAccess::get_file_as_string(path);
        if (source.is_empty()) return;
        String scripts_dir(String(ProjectSettings::get_singleton()->get_setting("tama/scripts_path", "res://tamascripts")));
        // Must use memnew — Godot Objects cannot be stack-allocated.
        // parse_source_for_exports stores the AST in the repo so the raw pointer stays valid.
        TamaScriptRepository *temp = memnew(TamaScriptRepository);
        Object *program = temp->parse_source_for_exports(source, scripts_dir);
        if (program) _read_exports_from(program);
        memdelete(temp);
    } else {
        TamaManagerBase *mgr = TamaManagerBase::get_instance();
        if (mgr && mgr->_has_tama_script(_script_filename)) {
            Object *program = mgr->_get_script_from_repository(_script_filename);
            if (program) _read_exports_from(program);
        }
    }
}

String TamaEmitter::_exports_signature() const {
    String sig;
    for (const ExportDef &def : _cached_export_defs) {
        if (!sig.is_empty()) sig += ",";
        sig += def.name + String(":") + def.type;
    }
    return sig;
}

String TamaEmitter::_find_script_path() const {
    String dir(String(ProjectSettings::get_singleton()->get_setting("tama/scripts_path", "res://addons/tama-cpp/example/tamascripts")));
    for (const String &suffix : {String(""), String(".tama"), String(".tam")}) {
        String full = dir.path_join(_script_filename + suffix);
        if (FileAccess::file_exists(full)) return full;
    }
    return "";
}

uint64_t TamaEmitter::_get_script_mtime() const {
    String path = _find_script_path();
    return path.is_empty() ? 0 : FileAccess::get_modified_time(path);
}

void TamaEmitter::_check_file_changed() {
    if (_script_filename.is_empty()) return;
    uint64_t mtime = _get_script_mtime();
    if (mtime == 0 || mtime == _last_modified_time) return;
    _last_modified_time = mtime;
    String old_sig = _exports_signature();
    _refresh_exports();
    if (_exports_signature() != old_sig) notify_property_list_changed();
}
