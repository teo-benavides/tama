#include "tama_ast_nodes.h"

std::shared_ptr<_TamaASTNode> tama_make_node(int type_id) {
    auto n = std::make_shared<_TamaASTNode>();
    n->type_id = type_id;
    return n;
}
