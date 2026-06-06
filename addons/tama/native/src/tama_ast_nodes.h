#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

// ---------------------------------------------------------------------------
// _TamaASTNode — flat struct for all AST node types.
//
// GDCLASS is kept so nodes can live in godot::Array (ExecFrame::body).
// All fields are direct C++ members — no Dictionary lookup, no _get() overhead.
// ---------------------------------------------------------------------------

class _TamaASTNode : public godot::RefCounted {
    GDCLASS(_TamaASTNode, godot::RefCounted)
protected:
    static void _bind_methods();
public:
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

    // Array fields
    godot::Array body;
    godot::Array params;
    godot::Array args;
    godot::Array conditions;
    godot::Array bodies;
    godot::Array else_body;
    godot::Array fires;
    godot::Array acts;
    godot::Array bullets;
    godot::Array exports;

    // Child node fields
    godot::Ref<_TamaASTNode> dir;
    godot::Ref<_TamaASTNode> speed;
    godot::Ref<_TamaASTNode> over;
    godot::Ref<_TamaASTNode> offset;
    godot::Ref<_TamaASTNode> pos;
    godot::Ref<_TamaASTNode> bullet;
    godot::Ref<_TamaASTNode> x;
    godot::Ref<_TamaASTNode> y;
    godot::Ref<_TamaASTNode> emitter_act;
    godot::Ref<_TamaASTNode> act;
    godot::Ref<_TamaASTNode> mvmt;
    godot::Ref<_TamaASTNode> main;

    int get_type_id() const { return type_id; }
};

// Convenience factory used by the parser.
godot::Ref<_TamaASTNode> tama_make_node(int type_id);
