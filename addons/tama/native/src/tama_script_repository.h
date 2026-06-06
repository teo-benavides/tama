#pragma once
#include "tama_ast_nodes.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class _TamaScriptRepository : public godot::Node {
    GDCLASS(_TamaScriptRepository, godot::Node)

protected:
    static void _bind_methods();

public:
    void load_scripts(const godot::String &path);
    void load_script(const godot::String &filename, const godot::String &full_path);
    void load_script_from_source(const godot::String &name, const godot::String &source);

    _TamaASTNode *get_tama_script(const godot::String &filename) const;
    bool          has_tama_script(const godot::String &filename) const;

    _TamaASTNode *parse_source_for_exports(const godot::String &source,
                                           const godot::String &scripts_dir);

private:
    std::unordered_map<std::string, std::shared_ptr<_TamaASTNode>> _scripts;
    std::string _scripts_dir;

    using Resolver = std::function<std::shared_ptr<_TamaASTNode>(const std::string &)>;

    std::shared_ptr<_TamaASTNode> _parse_source(const std::string &source,
                                                 const std::string &label,
                                                 Resolver resolver = nullptr) const;

    std::string _find_script_path(const std::string &name,
                                   const std::string &dir) const;

    Resolver _make_resolver(const std::string &scripts_dir,
                             std::vector<std::string> loading = {}) const;
};
