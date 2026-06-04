#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

// ---------------------------------------------------------------------------
// _TamaASTNode — single registered C++ class for all AST nodes.
//
// Each node stores its type-specific fields in `_data` (a Dictionary with
// String keys). The `_get` override routes `object.get("field")` calls to
// the dictionary, so the existing C++ interpreter works without modification.
//
// `type_id` is a registered property for fast O(1) access (the interpreter
// reads it on every node). Everything else goes through _get / _data.
// ---------------------------------------------------------------------------

class _TamaASTNode : public godot::RefCounted {
    GDCLASS(_TamaASTNode, godot::RefCounted)

protected:
    static void _bind_methods();

public:
    godot::Dictionary _data; // type-specific fields; public for parser access
    int type_id = 0;

    int  get_type_id() const { return type_id; }

    // Routes get("field") to _data for any field not registered as a property.
    bool _get(const godot::StringName &p_name, godot::Variant &r_ret) const;
};

// Convenience factory used by the parser.
godot::Ref<_TamaASTNode> tama_make_node(int type_id);
