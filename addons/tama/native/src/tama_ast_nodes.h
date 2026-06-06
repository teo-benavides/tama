#pragma once
#include <memory>
#include <string>
#include <vector>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

// ---------------------------------------------------------------------------
// _TamaASTNode — plain C++ struct, no Godot machinery.
// ---------------------------------------------------------------------------

struct _TamaASTNode;

// Evaluated argument value — either a Godot Variant or an AST node pointer.
struct TamaArgVal {
    bool is_node = false;
    godot::Variant var;
    _TamaASTNode *node = nullptr;
    // Owns the node when created from a shared_ptr (parser inline nodes).
    // Null when node is a raw ptr into a body already owned by the AST.
    std::shared_ptr<_TamaASTNode> _owner;

    TamaArgVal() = default;
    TamaArgVal(const godot::Variant &v) : var(v) {}
    // Raw-ptr constructor: caller guarantees lifetime (body nodes owned by AST).
    TamaArgVal(_TamaASTNode *n) : is_node(true), node(n) {}
    // Owning constructor: takes ownership (inline parser-created nodes).
    TamaArgVal(std::shared_ptr<_TamaASTNode> n) : is_node(true), node(n.get()), _owner(std::move(n)) {}
};

struct _TamaASTNode {
    int type_id = 0;

    // Shared scalar fields
    godot::String name;
    godot::String expr;
    godot::String var_name;
    godot::String count;
    godot::String index_var;
    godot::String condition;
    godot::String bullet_type;
    godot::String export_type;
    godot::String axis;          // "x" or "y" — OFFSET_AXIS nodes
    godot::String dir_type_var;
    godot::String speed_type_var;
    godot::String axis_type_var;
    int  dir_type   = 0;
    int  speed_type = 0;
    int  axis_type  = 1;   // REL default
    bool is_async   = false;
    godot::Variant default_value;

    // Body/block fields (now vectors of shared_ptr)
    std::vector<std::shared_ptr<_TamaASTNode>> body, else_body;
    std::vector<std::shared_ptr<_TamaASTNode>> fires, acts, bullets, exports;

    // IF-chain fields
    std::vector<godot::String>                               conditions;
    std::vector<std::vector<std::shared_ptr<_TamaASTNode>>> bodies;

    // Params/args
    std::vector<godot::String> params;
    std::vector<TamaArgVal>    args;

    // Child node fields
    std::shared_ptr<_TamaASTNode> dir, speed, over, offset, pos, bullet;
    std::shared_ptr<_TamaASTNode> x, y, emitter_act, act, mvmt, main;
};

// Convenience factory used by the parser.
std::shared_ptr<_TamaASTNode> tama_make_node(int type_id);
