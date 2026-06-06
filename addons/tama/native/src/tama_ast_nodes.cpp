#include "tama_ast_nodes.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void _TamaASTNode::_bind_methods() {
    // GDCLASS required for Array storage — no properties exposed.
}

Ref<_TamaASTNode> tama_make_node(int type_id) {
    Ref<_TamaASTNode> n;
    n.instantiate();
    n->type_id = type_id;
    return n;
}
