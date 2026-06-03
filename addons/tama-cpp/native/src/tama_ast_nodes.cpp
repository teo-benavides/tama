#include "tama_ast_nodes.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void _TamaASTNode::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_type_id"), &_TamaASTNode::get_type_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "type_id"), "", "get_type_id");
}

bool _TamaASTNode::_get(const StringName &p_name, Variant &r_ret) const {
    String key(p_name);
    if (_data.has(key)) {
        r_ret = _data[key];
        return true;
    }
    return false;
}

Ref<_TamaASTNode> tama_make_node(int type_id) {
    Ref<_TamaASTNode> n;
    n.instantiate();
    n->type_id = type_id;
    return n;
}
