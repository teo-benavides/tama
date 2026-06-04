#include "tama_context.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void TamaContext::_bind_methods() {
    ClassDB::bind_method(D_METHOD("time"),          &TamaContext::time);
    ClassDB::bind_method(D_METHOD("get_1_in", "n"), &TamaContext::get_1_in);
}

float TamaContext::time() const {
    Engine *e = Engine::get_singleton();
    return float(e->get_physics_frames()) / float(e->get_physics_ticks_per_second());
}

int TamaContext::get_1_in(int n) const {
    if (n <= 0) return 0;
    return (rand() % n == 0) ? 1 : 0;
}
