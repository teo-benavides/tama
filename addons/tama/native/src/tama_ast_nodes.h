#pragma once
#include <cctype>
#include <memory>
#include <string>
#include <vector>
#include <godot_cpp/variant/variant.hpp>

// Forward declarations — TamaArgVal and TamaRef are mutually recursive.
struct TamaRef;
struct _TamaASTNode;

// ---------------------------------------------------------------------------
// TamaArgVal — evaluated argument value
// ---------------------------------------------------------------------------
struct TamaArgVal {
    bool is_node = false;
    godot::Variant var;
    _TamaASTNode *node = nullptr;
    std::shared_ptr<_TamaASTNode> _owner; // keeps inline parser-created nodes alive
    std::shared_ptr<TamaRef> ref;         // non-null when this is a first-class ref

    TamaArgVal() = default;
    TamaArgVal(const godot::Variant &v) : var(v) {}
    // Raw-ptr constructor: caller guarantees lifetime (body nodes owned by AST).
    TamaArgVal(_TamaASTNode *n) : is_node(true), node(n) {}
    // Owning constructor: takes ownership (inline parser-created nodes).
    TamaArgVal(std::shared_ptr<_TamaASTNode> n) : is_node(true), node(n.get()), _owner(std::move(n)) {}
    // First-class ref constructor.
    TamaArgVal(std::shared_ptr<TamaRef> r) : ref(std::move(r)) {}
};

// ---------------------------------------------------------------------------
// _TamaASTNode — plain C++ struct, no Godot machinery.
// ---------------------------------------------------------------------------
struct _TamaASTNode {
    int type_id = 0;

    std::string name, expr, var_name, count, index_var, condition;
    std::string bullet_type, export_type, axis;
    std::string dir_type_var, speed_type_var, axis_type_var;
    int  dir_type   = 0;
    int  speed_type = 0;
    int  axis_type  = 1;
    bool is_async   = false;
    godot::Variant default_value; // export var defaults — keep as Variant

    std::vector<std::shared_ptr<_TamaASTNode>> body, else_body;
    std::vector<std::shared_ptr<_TamaASTNode>> fires, acts, bullets, exports;

    std::vector<std::string>                               conditions;
    std::vector<std::vector<std::shared_ptr<_TamaASTNode>>> bodies;

    std::vector<std::string> params;
    std::vector<TamaArgVal>  args;

    std::shared_ptr<_TamaASTNode> dir, speed, rotspd, over, offset, pos, bullet, delay;
    std::shared_ptr<_TamaASTNode> x, y, emitter_act, act, mvmt, main;
};

// ---------------------------------------------------------------------------
// TamaRef — first-class act/fire/bullet reference with pre-bound args.
// Plain C++ struct, no Godot machinery.
// ---------------------------------------------------------------------------
struct TamaRef {
    std::string name;
    std::vector<TamaArgVal> bound_args;
};

std::shared_ptr<_TamaASTNode> tama_make_node(int type_id);
