#include "tama_script_repository.h"
#include "tama_lexer.h"
#include "tama_parser.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ===========================================================================
// Binding
// ===========================================================================

void _TamaScriptRepository::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_scripts", "path"),          &_TamaScriptRepository::load_scripts);
    ClassDB::bind_method(D_METHOD("load_script", "filename", "full_path"), &_TamaScriptRepository::load_script);
    ClassDB::bind_method(D_METHOD("load_script_from_source", "name", "source"), &_TamaScriptRepository::load_script_from_source);
    ClassDB::bind_method(D_METHOD("get_tama_script", "filename"),   &_TamaScriptRepository::get_tama_script);
    ClassDB::bind_method(D_METHOD("has_tama_script", "filename"),   &_TamaScriptRepository::has_tama_script);
    ClassDB::bind_method(D_METHOD("parse_source_for_exports", "source", "scripts_dir"), &_TamaScriptRepository::parse_source_for_exports);
}

// ===========================================================================
// Helpers
// ===========================================================================

std::string _TamaScriptRepository::_find_script_path(const std::string &name,
                                                      const std::string &dir) const {
    if (dir.empty()) return "";
    String sdir(dir.c_str());
    for (const String &suffix : {String(""), String(".tama"), String(".tam")}) {
        String full = sdir.path_join(String(name.c_str()) + suffix);
        if (FileAccess::file_exists(full)) return full.utf8().get_data();
    }
    return "";
}

_TamaScriptRepository::Resolver
_TamaScriptRepository::_make_resolver(const std::string &scripts_dir,
                                      std::vector<std::string> loading) const {
    return [this, scripts_dir, loading](const std::string &name) mutable -> Ref<_TamaASTNode> {
        for (const auto &n : loading) {
            if (n == name) {
                UtilityFunctions::push_warning(
                    String("_TamaScriptRepository: circular include '") + String(name.c_str()) + "'");
                return Ref<_TamaASTNode>();
            }
        }
        // Check cache first
        auto it = _scripts.find(name);
        if (it != _scripts.end()) return it->second;
        // Try to load from disk
        std::string path = _find_script_path(name, scripts_dir);
        if (path.empty()) {
            UtilityFunctions::push_warning(
                String("_TamaScriptRepository: included script '") + String(name.c_str()) + "' not found");
            return Ref<_TamaASTNode>();
        }
        String fsrc = FileAccess::get_file_as_string(String(path.c_str()));
        if (fsrc.is_empty()) return Ref<_TamaASTNode>();
        loading.push_back(name);
        std::string src = fsrc.utf8().get_data();
        Ref<_TamaASTNode> prog = _parse_source(src, path, _make_resolver(scripts_dir, loading));
        loading.erase(std::remove(loading.begin(), loading.end(), name), loading.end());
        if (prog.is_valid()) {
            const_cast<_TamaScriptRepository *>(this)->_scripts[name] = prog;
        }
        return prog;
    };
}

Ref<_TamaASTNode> _TamaScriptRepository::_parse_source(const std::string &source,
                                                       const std::string &label,
                                                       Resolver resolver) const {
    TamaLexerCpp  lexer;
    TamaParserCpp parser;
    auto tokens = lexer.tokenize(source);
    auto result = parser.parse(tokens, resolver);
    for (const auto &err : result.errors) {
        UtilityFunctions::push_warning(
            String("TamaScript [") + String(label.c_str()) + "] L" +
            String::num_int64(err.line) + " C" + String::num_int64(err.col) +
            ": " + String(err.message.c_str()));
    }
    return result.program;
}

// ===========================================================================
// Public API
// ===========================================================================

void _TamaScriptRepository::load_scripts(const String &path) {
    _scripts_dir = path.utf8().get_data();
    Ref<DirAccess> dir = DirAccess::open(path);
    if (!dir.is_valid()) {
        return;
    }
    dir->list_dir_begin();
    String file = dir->get_next();
    while (!file.is_empty()) {
        if (file.ends_with(".tam") || file.ends_with(".tama")) {
            String base = file.get_basename();
            String full = path.path_join(file);
            String src  = FileAccess::get_file_as_string(full);
            if (!src.is_empty()) {
                Resolver res = _make_resolver(_scripts_dir);
                Ref<_TamaASTNode> prog = _parse_source(src.utf8().get_data(), full.utf8().get_data(), res);
                if (prog.is_valid())
                    _scripts[base.utf8().get_data()] = prog;
            }
        }
        file = dir->get_next();
    }
    dir->list_dir_end();
}

void _TamaScriptRepository::load_script(const String &filename, const String &full_path) {
    String src = FileAccess::get_file_as_string(full_path);
    if (src.is_empty()) {
        UtilityFunctions::push_error(
            String("_TamaScriptRepository: could not read '") + full_path + "'");
        return;
    }
    Resolver res = _make_resolver(_scripts_dir);
    Ref<_TamaASTNode> prog = _parse_source(src.utf8().get_data(), full_path.utf8().get_data(), res);
    if (prog.is_valid())
        _scripts[filename.utf8().get_data()] = prog;
}

void _TamaScriptRepository::load_script_from_source(const String &name, const String &source) {
    Resolver res = _make_resolver(_scripts_dir);
    Ref<_TamaASTNode> prog = _parse_source(source.utf8().get_data(), name.utf8().get_data(), res);
    if (prog.is_valid())
        _scripts[name.utf8().get_data()] = prog;
}

Object *_TamaScriptRepository::get_tama_script(const String &filename) const {
    auto it = _scripts.find(filename.utf8().get_data());
    if (it != _scripts.end()) return it->second.ptr();
    return nullptr;
}

bool _TamaScriptRepository::has_tama_script(const String &filename) const {
    return _scripts.count(filename.utf8().get_data()) > 0;
}

Object *_TamaScriptRepository::parse_source_for_exports(const String &source,
                                                         const String &scripts_dir) {
    Resolver res = _make_resolver(scripts_dir.utf8().get_data());
    Ref<_TamaASTNode> prog = _parse_source(source.utf8().get_data(), "<editor>", res);
    if (!prog.is_valid()) return nullptr;
    _scripts["<editor>"] = prog;
    return prog.ptr();
}
