#pragma once
#include "tama_ast_nodes.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>

#include <functional>
#include <string>
#include <unordered_map>

class TamaScriptRepository : public godot::Node {
    GDCLASS(TamaScriptRepository, godot::Node)

protected:
    static void _bind_methods();

public:
    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------
    void load_scripts(const godot::String &path);
    void load_script(const godot::String &filename, const godot::String &full_path);
    void load_script_from_source(const godot::String &name, const godot::String &source);

    godot::Object *get_tama_script(const godot::String &filename) const;
    bool           has_tama_script(const godot::String &filename) const;

    // Parse a raw source string and return a ProgramNode (may be null on failure).
    // `scripts_dir` is used to resolve 'include' directives.
    // The returned pointer is valid until this repository is destroyed.
    godot::Object *parse_source_for_exports(const godot::String &source,
                                            const godot::String &scripts_dir);

private:
    std::unordered_map<std::string, godot::Ref<TamaASTNode>> _scripts;
    std::string _scripts_dir;

    using Resolver = std::function<godot::Ref<TamaASTNode>(const std::string &)>;

    godot::Ref<TamaASTNode> _parse_source(const std::string &source,
                                           const std::string &label,
                                           Resolver resolver = nullptr) const;

    std::string _find_script_path(const std::string &name,
                                   const std::string &dir) const;

    Resolver _make_resolver(const std::string &scripts_dir,
                             std::vector<std::string> loading = {}) const;
};
