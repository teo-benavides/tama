#include "tama_interpreter.h"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ===========================================================================
// Payload class bindings
// ===========================================================================

#define BIND_PROP(cls, field, vtype) \
    ClassDB::bind_method(D_METHOD("get_" #field), [](cls *s) { return s->field; }); \
    ClassDB::bind_method(D_METHOD("set_" #field, "v"), [](cls *s, auto v) { s->field = v; }); \
    ADD_PROPERTY(PropertyInfo(Variant::vtype, #field), "set_" #field, "get_" #field)

void TamaBulletFireData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_dir_type"),        &TamaBulletFireData::get_dir_type);
    ClassDB::bind_method(D_METHOD("get_dir_value"),       &TamaBulletFireData::get_dir_value);
    ClassDB::bind_method(D_METHOD("get_speed_type"),      &TamaBulletFireData::get_speed_type);
    ClassDB::bind_method(D_METHOD("get_speed_value"),     &TamaBulletFireData::get_speed_value);
    ClassDB::bind_method(D_METHOD("get_offset_mode"),     &TamaBulletFireData::get_offset_mode);
    ClassDB::bind_method(D_METHOD("get_offset_value"),    &TamaBulletFireData::get_offset_value);
    ClassDB::bind_method(D_METHOD("get_offset_x_type"),   &TamaBulletFireData::get_offset_x_type);
    ClassDB::bind_method(D_METHOD("get_offset_x"),        &TamaBulletFireData::get_offset_x);
    ClassDB::bind_method(D_METHOD("get_offset_y_type"),   &TamaBulletFireData::get_offset_y_type);
    ClassDB::bind_method(D_METHOD("get_offset_y"),        &TamaBulletFireData::get_offset_y);
    ClassDB::bind_method(D_METHOD("get_has_pos"),         &TamaBulletFireData::get_has_pos);
    ClassDB::bind_method(D_METHOD("get_pos_x_set"),       &TamaBulletFireData::get_pos_x_set);
    ClassDB::bind_method(D_METHOD("get_pos_x_type"),      &TamaBulletFireData::get_pos_x_type);
    ClassDB::bind_method(D_METHOD("get_pos_x"),           &TamaBulletFireData::get_pos_x);
    ClassDB::bind_method(D_METHOD("get_pos_y_set"),       &TamaBulletFireData::get_pos_y_set);
    ClassDB::bind_method(D_METHOD("get_pos_y_type"),      &TamaBulletFireData::get_pos_y_type);
    ClassDB::bind_method(D_METHOD("get_pos_y"),           &TamaBulletFireData::get_pos_y);
    ClassDB::bind_method(D_METHOD("get_bullet_type"),     &TamaBulletFireData::get_bullet_type);
    ClassDB::bind_method(D_METHOD("get_bullet_emitter_act"), &TamaBulletFireData::get_bullet_emitter_act);
    ClassDB::bind_method(D_METHOD("get_bullet_act"),      &TamaBulletFireData::get_bullet_act);
    ClassDB::bind_method(D_METHOD("get_bullet_params"),   &TamaBulletFireData::get_bullet_params);
    ClassDB::bind_method(D_METHOD("get_bullet_args"),     &TamaBulletFireData::get_bullet_args);
    ClassDB::bind_method(D_METHOD("get_mvmt_x_set"),      &TamaBulletFireData::get_mvmt_x_set);
    ClassDB::bind_method(D_METHOD("get_mvmt_x_type"),     &TamaBulletFireData::get_mvmt_x_type);
    ClassDB::bind_method(D_METHOD("get_mvmt_x_expr"),     &TamaBulletFireData::get_mvmt_x_expr);
    ClassDB::bind_method(D_METHOD("get_mvmt_y_set"),      &TamaBulletFireData::get_mvmt_y_set);
    ClassDB::bind_method(D_METHOD("get_mvmt_y_type"),     &TamaBulletFireData::get_mvmt_y_type);
    ClassDB::bind_method(D_METHOD("get_mvmt_y_expr"),     &TamaBulletFireData::get_mvmt_y_expr);
    ClassDB::bind_method(D_METHOD("get_source_program"),  &TamaBulletFireData::get_source_program);

    ADD_PROPERTY(PropertyInfo(Variant::INT,    "dir_type"),        "", "get_dir_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "dir_value"),       "", "get_dir_value");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "speed_type"),      "", "get_speed_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "speed_value"),     "", "get_speed_value");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "offset_mode"),     "", "get_offset_mode");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "offset_value"),    "", "get_offset_value");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "offset_x_type"),   "", "get_offset_x_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "offset_x"),        "", "get_offset_x");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "offset_y_type"),   "", "get_offset_y_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "offset_y"),        "", "get_offset_y");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "has_pos"),         "", "get_has_pos");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "pos_x_set"),       "", "get_pos_x_set");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "pos_x_type"),      "", "get_pos_x_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "pos_x"),           "", "get_pos_x");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "pos_y_set"),       "", "get_pos_y_set");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "pos_y_type"),      "", "get_pos_y_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,  "pos_y"),           "", "get_pos_y");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "bullet_type"),     "", "get_bullet_type");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "bullet_emitter_act"), "", "get_bullet_emitter_act");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "bullet_act"),     "", "get_bullet_act");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY,  "bullet_params"),  "", "get_bullet_params");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY,  "bullet_args"),    "", "get_bullet_args");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "mvmt_x_set"),     "", "get_mvmt_x_set");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "mvmt_x_type"),    "", "get_mvmt_x_type");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "mvmt_x_expr"),    "", "get_mvmt_x_expr");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,   "mvmt_y_set"),     "", "get_mvmt_y_set");
    ADD_PROPERTY(PropertyInfo(Variant::INT,    "mvmt_y_type"),    "", "get_mvmt_y_type");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "mvmt_y_expr"),    "", "get_mvmt_y_expr");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "source_program"), "", "get_source_program");
}

// Accessor definitions for TamaBulletFireData
int    TamaBulletFireData::get_dir_type()        const { return dir_type; }
float  TamaBulletFireData::get_dir_value()       const { return dir_value; }
int    TamaBulletFireData::get_speed_type()      const { return speed_type; }
float  TamaBulletFireData::get_speed_value()     const { return speed_value; }
int    TamaBulletFireData::get_offset_mode()     const { return offset_mode; }
float  TamaBulletFireData::get_offset_value()    const { return offset_value; }
int    TamaBulletFireData::get_offset_x_type()   const { return offset_x_type; }
float  TamaBulletFireData::get_offset_x()        const { return offset_x; }
int    TamaBulletFireData::get_offset_y_type()   const { return offset_y_type; }
float  TamaBulletFireData::get_offset_y()        const { return offset_y; }
bool   TamaBulletFireData::get_has_pos()         const { return has_pos; }
bool   TamaBulletFireData::get_pos_x_set()       const { return pos_x_set; }
int    TamaBulletFireData::get_pos_x_type()      const { return pos_x_type; }
float  TamaBulletFireData::get_pos_x()           const { return pos_x; }
bool   TamaBulletFireData::get_pos_y_set()       const { return pos_y_set; }
int    TamaBulletFireData::get_pos_y_type()      const { return pos_y_type; }
float  TamaBulletFireData::get_pos_y()           const { return pos_y; }
String TamaBulletFireData::get_bullet_type()     const { return bullet_type; }
Object*TamaBulletFireData::get_bullet_emitter_act()const{ return bullet_emitter_act; }
Object*TamaBulletFireData::get_bullet_act()      const { return bullet_act; }
Array  TamaBulletFireData::get_bullet_params()   const { return bullet_params; }
Array  TamaBulletFireData::get_bullet_args()     const { return bullet_args; }
bool   TamaBulletFireData::get_mvmt_x_set()      const { return mvmt_x_set; }
int    TamaBulletFireData::get_mvmt_x_type()     const { return mvmt_x_type; }
String TamaBulletFireData::get_mvmt_x_expr()     const { return mvmt_x_expr; }
bool   TamaBulletFireData::get_mvmt_y_set()      const { return mvmt_y_set; }
int    TamaBulletFireData::get_mvmt_y_type()     const { return mvmt_y_type; }
String TamaBulletFireData::get_mvmt_y_expr()     const { return mvmt_y_expr; }
Object*TamaBulletFireData::get_source_program()  const { return source_program; }

void TamaChdirData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_dir_type"),  &TamaChdirData::get_dir_type);
    ClassDB::bind_method(D_METHOD("get_dir_value"), &TamaChdirData::get_dir_value);
    ClassDB::bind_method(D_METHOD("get_over"),      &TamaChdirData::get_over);
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "dir_type"),  "", "get_dir_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dir_value"), "", "get_dir_value");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "over"),      "", "get_over");
}
int   TamaChdirData::get_dir_type()  const { return dir_type; }
float TamaChdirData::get_dir_value() const { return dir_value; }
float TamaChdirData::get_over()      const { return over; }

void TamaChspdData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_speed_type"),  &TamaChspdData::get_speed_type);
    ClassDB::bind_method(D_METHOD("get_speed_value"), &TamaChspdData::get_speed_value);
    ClassDB::bind_method(D_METHOD("get_over"),        &TamaChspdData::get_over);
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "speed_type"),  "", "get_speed_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_value"), "", "get_speed_value");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "over"),        "", "get_over");
}
int   TamaChspdData::get_speed_type()  const { return speed_type; }
float TamaChspdData::get_speed_value() const { return speed_value; }
float TamaChspdData::get_over()        const { return over; }

void TamaChposData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_has_x"),  &TamaChposData::get_has_x);
    ClassDB::bind_method(D_METHOD("get_x_type"), &TamaChposData::get_x_type);
    ClassDB::bind_method(D_METHOD("get_x"),      &TamaChposData::get_x);
    ClassDB::bind_method(D_METHOD("get_has_y"),  &TamaChposData::get_has_y);
    ClassDB::bind_method(D_METHOD("get_y_type"), &TamaChposData::get_y_type);
    ClassDB::bind_method(D_METHOD("get_y"),      &TamaChposData::get_y);
    ClassDB::bind_method(D_METHOD("get_over"),   &TamaChposData::get_over);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "has_x"),  "", "get_has_x");
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "x_type"), "", "get_x_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "x"),      "", "get_x");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "has_y"),  "", "get_has_y");
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "y_type"), "", "get_y_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "y"),      "", "get_y");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "over"),   "", "get_over");
}
bool  TamaChposData::get_has_x()  const { return has_x; }
int   TamaChposData::get_x_type() const { return x_type; }
float TamaChposData::get_x()      const { return x; }
bool  TamaChposData::get_has_y()  const { return has_y; }
int   TamaChposData::get_y_type() const { return y_type; }
float TamaChposData::get_y()      const { return y; }
float TamaChposData::get_over()   const { return over; }

void TamaAccelData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_has_x"),  &TamaAccelData::get_has_x);
    ClassDB::bind_method(D_METHOD("get_x_type"), &TamaAccelData::get_x_type);
    ClassDB::bind_method(D_METHOD("get_x"),      &TamaAccelData::get_x);
    ClassDB::bind_method(D_METHOD("get_has_y"),  &TamaAccelData::get_has_y);
    ClassDB::bind_method(D_METHOD("get_y_type"), &TamaAccelData::get_y_type);
    ClassDB::bind_method(D_METHOD("get_y"),      &TamaAccelData::get_y);
    ClassDB::bind_method(D_METHOD("get_over"),   &TamaAccelData::get_over);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "has_x"),  "", "get_has_x");
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "x_type"), "", "get_x_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "x"),      "", "get_x");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,  "has_y"),  "", "get_has_y");
    ADD_PROPERTY(PropertyInfo(Variant::INT,   "y_type"), "", "get_y_type");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "y"),      "", "get_y");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "over"),   "", "get_over");
}
bool  TamaAccelData::get_has_x()  const { return has_x; }
int   TamaAccelData::get_x_type() const { return x_type; }
float TamaAccelData::get_x()      const { return x; }
bool  TamaAccelData::get_has_y()  const { return has_y; }
int   TamaAccelData::get_y_type() const { return y_type; }
float TamaAccelData::get_y()      const { return y; }
float TamaAccelData::get_over()   const { return over; }

void TamaRef::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_name"),       &TamaRef::get_ref_name);
    ClassDB::bind_method(D_METHOD("get_bound_args"), &TamaRef::get_bound_args);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"),       "", "get_name");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY,  "bound_args"), "", "get_bound_args");
}
String TamaRef::get_ref_name()   const { return name; }
Array  TamaRef::get_bound_args() const { return bound_args; }

// ===========================================================================
// TamaInterpreter
// ===========================================================================

void TamaInterpreter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("step", "delta"),                 &TamaInterpreter::step);
    ClassDB::bind_method(D_METHOD("start", "program", "scope"),    &TamaInterpreter::start);
    ClassDB::bind_method(D_METHOD("start_act", "program", "act", "scope"), &TamaInterpreter::start_act);
    ClassDB::bind_method(D_METHOD("stop"),                          &TamaInterpreter::stop);
    ClassDB::bind_method(D_METHOD("is_running"),                    &TamaInterpreter::is_running);
    ClassDB::bind_method(D_METHOD("set_context", "ctx"),              &TamaInterpreter::set_context);
    ClassDB::bind_method(D_METHOD("get_context"),                     &TamaInterpreter::get_context);
    ClassDB::bind_method(D_METHOD("eval_expr", "expr", "scope"),      &TamaInterpreter::eval_expr);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "context"), "set_context", "get_context");

    ADD_SIGNAL(MethodInfo("bullet_fired",        PropertyInfo(Variant::OBJECT, "data")));
    ADD_SIGNAL(MethodInfo("vanished"));
    ADD_SIGNAL(MethodInfo("changed_direction",   PropertyInfo(Variant::OBJECT, "data")));
    ADD_SIGNAL(MethodInfo("changed_speed",       PropertyInfo(Variant::OBJECT, "data")));
    ADD_SIGNAL(MethodInfo("changed_position",    PropertyInfo(Variant::OBJECT, "data")));
    ADD_SIGNAL(MethodInfo("accelerated",         PropertyInfo(Variant::OBJECT, "data")));
    ADD_SIGNAL(MethodInfo("finished"));
}

void TamaInterpreter::_ready() {
    set_physics_process(false);
}

void TamaInterpreter::_physics_process(double delta) {
    step(delta);
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

void TamaInterpreter::start(Object *program, Dictionary scope) {
    _program = program;
    _scope   = scope;
    _exec_stack.clear();
    _scope_saves.clear();
    _async_children.clear();
    _running  = true;
    _breaking = false;

    if (!_program) { _running = false; return; }
    _build_lookup_tables();

    Object *main_node = Object::cast_to<Object>(_program->get("main"));
    if (!main_node) {
        UtilityFunctions::push_error("TamaInterpreter: program has no main block");
        _running = false;
        return;
    }
    Array main_body = (Array)main_node->get("body");
    _push_body(main_body);

    if (is_inside_tree()) set_physics_process(true);
}

void TamaInterpreter::start_act(Object *program, Object *act, Dictionary scope) {
    _program = program;
    _scope   = scope;
    _exec_stack.clear();
    _scope_saves.clear();
    _async_children.clear();
    _running  = true;
    _breaking = false;

    if (!_program || !act) { _running = false; return; }
    _build_lookup_tables();

    int type_id = (int)act->get("type_id");
    if (type_id == (int)TamaNodeType::INLINE_ACT) {
        Array body = (Array)act->get("body");
        _push_body(body);
    } else if (type_id == (int)TamaNodeType::ACT_CALL) {
        String name_str = (String)act->get("name");
        std::string name = name_str.utf8().get_data();

        // Check if name resolves through scope (first-class acts / TamaRef)
        Variant scope_val = _scope.get(name_str, Variant());
        if (scope_val.get_type() == Variant::OBJECT) {
            Object *sv = scope_val.operator Object *();
            if (sv && (int)sv->get("type_id") == (int)TamaNodeType::INLINE_ACT) {
                _push_body((Array)sv->get("body"));
                goto start_act_done;
            }
        }
        {
            Ref<TamaRef> tref = Object::cast_to<TamaRef>(scope_val.operator Object *());
            std::string ref_name = name;
            Array pre_bound;
            if (tref.is_valid()) {
                ref_name  = tref->name.utf8().get_data();
                pre_bound = tref->bound_args;
            }
            Object *act_def = _find_act(ref_name);
            if (!act_def) {
                UtilityFunctions::push_error(String("TamaInterpreter: unknown act '") + String(ref_name.c_str()) + "'");
                _running = false;
                return;
            }
            Array params = (Array)act_def->get("params");
            Array body   = (Array)act_def->get("body");
            if (!pre_bound.is_empty()) {
                _scope_saves.push_back(_scope);
                _scope = _scope_snapshot_plus_params(params, pre_bound);
                _push_body(body, false, true);
            } else {
                _push_body(body);
            }
        }
        start_act_done:;
    } else {
        _running = false;
    }

    if (is_inside_tree()) set_physics_process(true);
}

void TamaInterpreter::stop() {
    _running = false;
    _exec_stack.clear();
    for (auto *child : _async_children) {
        child->stop();
        child->queue_free();
    }
    _async_children.clear();
    if (is_inside_tree()) set_physics_process(false);
}

// ---------------------------------------------------------------------------
// Step — called every physics frame
// ---------------------------------------------------------------------------

void TamaInterpreter::step(double p_delta) {
    float delta = (float)p_delta;

    // Step async children first
    for (auto it = _async_children.begin(); it != _async_children.end(); ) {
        TamaInterpreter *child = *it;
        if (child->is_running()) {
            child->step(delta);
            ++it;
        } else {
            it = _async_children.erase(it);
        }
    }

    if (!_running) {
        // If all async children are done and we were waiting, emit finished
        if (_async_children.empty() && _exec_stack.empty()) {
            if (is_inside_tree()) set_physics_process(false);
            emit_signal("finished");
        }
        return;
    }

    bool yielded = false;

    // Execute frames until we yield or run out
    while (!yielded && _running && !_exec_stack.empty()) {
        ExecFrame &f = _exec_stack.back();

        // Process suspension on BODY frames
        if (f.kind == ExecFrame::Kind::BODY) {
            if (f.suspend == ExecFrame::Suspend::WAIT_TIME) {
                f.wait_secs -= delta;
                if (f.wait_secs > 0.0f) { yielded = true; break; }
                f.suspend = ExecFrame::Suspend::NONE;
            } else if (f.suspend == ExecFrame::Suspend::WAIT_FRAMES) {
                f.wait_frames -= 1;
                if (f.wait_frames > 0) { yielded = true; break; }
                f.suspend = ExecFrame::Suspend::NONE;
            }
            _step_body(f, yielded);
        } else {
            _step_loop_ctrl(f, yielded);
        }
    }

    // Check if fully done
    if (_exec_stack.empty() && _running) {
        _running = false;
        if (_async_children.empty()) {
            if (is_inside_tree()) set_physics_process(false);
            emit_signal("finished");
        }
        // else wait for async children to finish (checked next step)
    }
}

// ---------------------------------------------------------------------------
// Frame management
// ---------------------------------------------------------------------------

void TamaInterpreter::_build_lookup_tables() {
    _fires_map.clear();
    _acts_map.clear();
    _bullets_map.clear();
    if (!_program) return;

    Array fires   = (Array)_program->get("fires");
    Array acts    = (Array)_program->get("acts");
    Array bullets = (Array)_program->get("bullets");

    for (int i = 0; i < fires.size(); ++i) {
        Object *f = fires[i].operator Object *();
        if (f) _fires_map[((String)f->get("name")).utf8().get_data()] = f;
    }
    for (int i = 0; i < acts.size(); ++i) {
        Object *a = acts[i].operator Object *();
        if (a) _acts_map[((String)a->get("name")).utf8().get_data()] = a;
    }
    for (int i = 0; i < bullets.size(); ++i) {
        Object *b = bullets[i].operator Object *();
        if (b) _bullets_map[((String)b->get("name")).utf8().get_data()] = b;
    }
}

std::vector<std::string> TamaInterpreter::_snapshot_scope_keys() const {
    std::vector<std::string> keys;
    Array ks = _scope.keys();
    for (int i = 0; i < ks.size(); ++i)
        keys.push_back(((String)ks[i]).utf8().get_data());
    return keys;
}

void TamaInterpreter::_push_body(const Array &body, bool sync_only, bool pops_scope) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::BODY;
    f.body       = body;
    f.pc         = 0;
    f.sync_only  = sync_only;
    f.pops_scope = pops_scope;
    f.pre_keys   = _snapshot_scope_keys();
    _exec_stack.push_back(std::move(f));
}

void TamaInterpreter::_push_repeat_ctrl(const Array &body, int n, const String &idx_var) {
    ExecFrame f;
    f.kind             = ExecFrame::Kind::REPEAT_CTRL;
    f.loop_body        = body;
    f.loop_n           = n;
    f.loop_i           = 0;
    f.loop_index_var   = idx_var;
    f.between_iters    = false;
    _exec_stack.push_back(std::move(f));
}

void TamaInterpreter::_push_while_ctrl(const Array &body, const String &cond) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::WHILE_CTRL;
    f.loop_body  = body;
    f.while_cond = cond;
    f.loop_n     = -1;
    f.loop_i     = 0;
    _exec_stack.push_back(std::move(f));
}

void TamaInterpreter::_push_repeatf_ctrl(const Array &body, int n, const String &idx_var) {
    ExecFrame f;
    f.kind           = ExecFrame::Kind::REPEATF_CTRL;
    f.loop_body      = body;
    f.loop_n         = n;
    f.loop_i         = 0;
    f.loop_index_var = idx_var;
    f.between_iters  = false;
    _exec_stack.push_back(std::move(f));
}

void TamaInterpreter::_pop_body_frame() {
    if (_exec_stack.empty()) return;
    ExecFrame &f = _exec_stack.back();

    // Restore scope: remove any keys added by this frame
    const auto &pre = f.pre_keys;
    Array cur_keys = _scope.keys();
    for (int i = 0; i < cur_keys.size(); ++i) {
        std::string k = ((String)cur_keys[i]).utf8().get_data();
        if (std::find(pre.begin(), pre.end(), k) == pre.end())
            _scope.erase(cur_keys[i]);
    }

    // Restore isolated scope if this frame pushed one
    if (f.pops_scope && !_scope_saves.empty()) {
        _scope = _scope_saves.back();
        _scope_saves.pop_back();
    }

    _exec_stack.pop_back();
}

float TamaInterpreter::eval_expr(const String &expr, const Dictionary &scope) {
    Dictionary saved = _scope;
    _scope = scope;
    float result = _eval_float(expr);
    _scope = saved;
    return result;
}

// ---------------------------------------------------------------------------
// Frame stepping
// ---------------------------------------------------------------------------

void TamaInterpreter::_step_body(ExecFrame &f, bool &yielded) {
    while (f.pc < f.body.size()) {
        if (!_running) return;

        Object *node = f.body[f.pc].operator Object *();
        f.pc++;
        if (!node) continue;

        int type_id = (int)node->get("type_id");
        bool pushed = false;
        _exec_node(node, type_id, f.sync_only, yielded);

        // If a new frame was pushed, break — the outer loop will handle it
        if (!_exec_stack.empty() && &_exec_stack.back() != &f) return;
        if (yielded) return;
        if (!_running) return;
    }

    // Body exhausted
    _pop_body_frame();
}

void TamaInterpreter::_step_loop_ctrl(ExecFrame &f, bool &yielded) {
    bool is_repeatf = (f.kind == ExecFrame::Kind::REPEATF_CTRL);

    // Between-iteration wait (REPEATF only)
    if (is_repeatf && f.between_iters) {
        f.between_iters = false;
        yielded = true;
        return;
    }

    // Check termination
    bool done = false;
    if (f.kind == ExecFrame::Kind::WHILE_CTRL) {
        done = (_eval_float(f.while_cond) == 0.0f);
    } else {
        done = (f.loop_n >= 0 && f.loop_i >= f.loop_n);
    }

    if (done || _breaking) {
        _breaking = false;
        _exec_stack.pop_back();
        return;
    }

    // Set up index variable (BEFORE snapshotting pre_keys for the body frame)
    if (!f.loop_index_var.is_empty())
        _scope[f.loop_index_var] = Variant((float)f.loop_i);

    // For REPEATF, run body synchronously via _exec_body_sync
    if (is_repeatf) {
        // Snapshot pre_keys (includes index_var now — will be cleaned up after body)
        auto pre = _snapshot_scope_keys();
        _exec_body_sync(f.loop_body);
        // Clean up: remove any keys body added (including index_var)
        Array cur = _scope.keys();
        for (int i = 0; i < cur.size(); ++i) {
            std::string k = ((String)cur[i]).utf8().get_data();
            if (std::find(pre.begin(), pre.end(), k) == pre.end())
                _scope.erase(cur[i]);
        }
        f.loop_i++;
        // Wait one frame between iterations (unless this was the last)
        bool more = (f.loop_n < 0 || f.loop_i < f.loop_n);
        f.between_iters = more;
        if (f.between_iters) yielded = true;
        if (!_running || _breaking) {
            _breaking = false;
            _exec_stack.pop_back();
        }
    } else {
        // REPEAT_CTRL / WHILE_CTRL: push a BODY frame for this iteration
        f.loop_i++;
        _push_body(f.loop_body);
        // (outer step loop will immediately process the new body frame)
    }
}

// ---------------------------------------------------------------------------
// Single node dispatch
// ---------------------------------------------------------------------------

void TamaInterpreter::_exec_node(Object *node, int type_id, bool sync_only, bool &yielded) {
    using NT = TamaNodeType;

    switch ((NT)type_id) {

    case NT::BREAK:
        _breaking = true;
        _running  = false; // will be handled in body loop
        return;

    case NT::VANISH:
        _running = false;
        emit_signal("vanished");
        return;

    case NT::VAR_DECL: {
        String var_name = (String)node->get("var_name");
        String expr     = (String)node->get("expr");
        _scope[var_name] = _eval_arg(expr);
        return;
    }

    case NT::FIRE_CALL:
        _exec_fire_call(node);
        return;

    case NT::INLINE_FIRE:
        _exec_fire_node(node);
        return;

    case NT::CHDIR:  _emit_chdir(node); return;
    case NT::CHSPD:  _emit_chspd(node); return;
    case NT::CHPOS:  _emit_chpos(node); return;
    case NT::ACCEL:  _emit_accel(node); return;

    case NT::IF: {
        Array conditions = (Array)node->get("conditions");
        Array bodies     = (Array)node->get("bodies");
        Array else_body  = (Array)node->get("else_body");
        bool taken = false;
        for (int i = 0; i < conditions.size() && !taken; ++i) {
            if (_eval_float((String)conditions[i]) != 0.0f) {
                Array branch = (Array)bodies[i];
                if (sync_only) {
                    _exec_body_sync(branch);
                } else {
                    _push_body(branch);
                    yielded = false; // new frame pushed — outer loop handles
                }
                taken = true;
            }
        }
        if (!taken && else_body.size() > 0) {
            if (sync_only) {
                _exec_body_sync(else_body);
            } else {
                _push_body(else_body);
            }
        }
        return;
    }

    // --- Suspension-inducing nodes ---

    case NT::WAIT:
        if (!sync_only) {
            float secs = _eval_float((String)node->get("expr"));
            if (secs > 0.0f && !_exec_stack.empty()) {
                _exec_stack.back().suspend    = ExecFrame::Suspend::WAIT_TIME;
                _exec_stack.back().wait_secs  = secs;
                yielded = true;
            }
        }
        return;

    case NT::WAIT_FRAMES:
        if (!sync_only) {
            int frames = (int)std::round(_eval_float((String)node->get("expr")));
            if (frames > 0 && !_exec_stack.empty()) {
                _exec_stack.back().suspend      = ExecFrame::Suspend::WAIT_FRAMES;
                _exec_stack.back().wait_frames  = frames;
                yielded = true;
            }
        }
        return;

    case NT::REPEAT: {
        String count_str = ((String)node->get("count")).strip_edges();
        int n = count_str.is_empty() ? -1 : (int)std::round(_eval_float(count_str));
        String idx_var = (String)node->get("index_var");
        Array  body    = (Array)node->get("body");
        if (sync_only) {
            // Run all iterations synchronously
            int i = 0;
            while (!idx_var.is_empty() || n < 0 || i < n) {
                if (!idx_var.is_empty()) _scope[idx_var] = Variant((float)i);
                auto pre = _snapshot_scope_keys();
                _exec_body_sync(body);
                Array cur = _scope.keys();
                for (int j = 0; j < cur.size(); ++j) {
                    std::string k = ((String)cur[j]).utf8().get_data();
                    if (std::find(pre.begin(), pre.end(), k) == pre.end())
                        _scope.erase(cur[j]);
                }
                if (_breaking) { _breaking = false; break; }
                ++i;
                if (n >= 0 && i >= n) break;
            }
        } else {
            _push_repeat_ctrl(body, n, idx_var);
        }
        return;
    }

    case NT::WHILE: {
        String cond = (String)node->get("condition");
        Array  body = (Array)node->get("body");
        if (sync_only) {
            while (_eval_float(cond) != 0.0f) {
                auto pre = _snapshot_scope_keys();
                _exec_body_sync(body);
                Array cur = _scope.keys();
                for (int j = 0; j < cur.size(); ++j) {
                    std::string k = ((String)cur[j]).utf8().get_data();
                    if (std::find(pre.begin(), pre.end(), k) == pre.end())
                        _scope.erase(cur[j]);
                }
                if (_breaking) { _breaking = false; break; }
            }
        } else {
            _push_while_ctrl(body, cond);
        }
        return;
    }

    case NT::REPEAT_FRAME: {
        String count_str = ((String)node->get("count")).strip_edges();
        String idx_var   = (String)node->get("index_var");
        Array  body      = (Array)node->get("body");

        if (count_str.is_empty()) {
            // Infinite repeatf: run every frame forever (push a REPEATF_CTRL with n=-1)
            if (!sync_only) {
                _push_repeatf_ctrl(body, -1, idx_var);
            }
            // In sync context: no-op (can't run infinite loop)
        } else {
            int n = (int)std::round(_eval_float(count_str));
            if (!sync_only) {
                _push_repeatf_ctrl(body, n, idx_var);
            } else {
                // Sync context: run N times with no frame yield
                for (int i = 0; i < n && _running; ++i) {
                    if (!idx_var.is_empty()) _scope[idx_var] = Variant((float)i);
                    auto pre = _snapshot_scope_keys();
                    _exec_body_sync(body);
                    Array cur = _scope.keys();
                    for (int j = 0; j < cur.size(); ++j) {
                        std::string k = ((String)cur[j]).utf8().get_data();
                        if (std::find(pre.begin(), pre.end(), k) == pre.end())
                            _scope.erase(cur[j]);
                    }
                    if (_breaking) { _breaking = false; break; }
                }
            }
        }
        return;
    }

    case NT::ACT_CALL: {
        if (sync_only) return; // no-op in sync context
        String name     = (String)node->get("name");
        bool   is_async = (bool)node->get("is_async");
        Array  args     = (Array)node->get("args");

        // Resolve through scope for first-class refs or inline acts
        Variant scope_val = _scope.get(name, Variant());
        if (scope_val.get_type() == Variant::OBJECT) {
            Object *sv = scope_val.operator Object *();
            if (sv) {
                int sv_tid = (int)sv->get("type_id");
                if (sv_tid == (int)TamaNodeType::INLINE_ACT) {
                    Array body = (Array)sv->get("body");
                    if (is_async) {
                        _run_async_act(sv, _scope.duplicate());
                    } else {
                        _push_body(body);
                    }
                    return;
                }
            }
        }
        // TamaRef in scope?
        Ref<TamaRef> tref = Object::cast_to<TamaRef>(scope_val.operator Object *());

        std::string ref_name = name.utf8().get_data();
        Array pre_bound;
        if (tref.is_valid()) {
            ref_name  = tref->name.utf8().get_data();
            pre_bound = tref->bound_args;
        }

        Object *act_def = _find_act(ref_name);
        if (!act_def) {
            UtilityFunctions::push_warning(String("TamaInterpreter: unknown act '") + String(ref_name.c_str()) + "'");
            return;
        }
        Array params = (Array)act_def->get("params");
        Array body   = (Array)act_def->get("body");

        // Evaluate args
        Array eval_args;
        for (int i = 0; i < pre_bound.size(); ++i) eval_args.push_back(pre_bound[i]);
        for (int i = 0; i < args.size(); ++i)      eval_args.push_back(_eval_arg(args[i]));

        if (is_async) {
            Dictionary scope_copy = _scope_snapshot_plus_params(params, eval_args);
            _run_async_act(act_def, scope_copy); // passes act_def as stand-in (body read inside)
        } else {
            // Isolated scope: save current, set up new scope = current + params
            _scope_saves.push_back(_scope);
            _scope = _scope_snapshot_plus_params(params, eval_args);
            _push_body(body, false, true); // pops_scope=true
        }
        return;
    }

    case NT::INLINE_ACT: {
        if (sync_only) return;
        Array body     = (Array)node->get("body");
        bool is_async  = (bool)node->get("is_async");
        if (is_async) {
            _run_async_act(node, _scope.duplicate());
        } else {
            _push_body(body);
        }
        return;
    }

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Sync body execution (for repeatf body — no suspension)
// ---------------------------------------------------------------------------

void TamaInterpreter::_exec_body_sync(const Array &body) {
    for (int i = 0; i < body.size(); ++i) {
        if (!_running || _breaking) return;
        Object *node = body[i].operator Object *();
        if (!node) continue;
        bool unused_yielded = false;
        _exec_node(node, (int)node->get("type_id"), true, unused_yielded);
    }
}

// ---------------------------------------------------------------------------
// Async acts
// ---------------------------------------------------------------------------

void TamaInterpreter::_run_async_act(Object *act_node, const Dictionary &scope_copy) {
    TamaInterpreter *child = memnew(TamaInterpreter);
    child->_context  = _context;
    child->_program  = _program;
    child->_fires_map  = _fires_map;
    child->_acts_map   = _acts_map;
    child->_bullets_map = _bullets_map;

    // Forward child signals to this interpreter's own signals via dedicated methods
    child->connect("bullet_fired",      callable_mp(this, &TamaInterpreter::_fwd_bullet_fired));
    child->connect("vanished",          callable_mp(this, &TamaInterpreter::_fwd_vanished));
    child->connect("changed_direction", callable_mp(this, &TamaInterpreter::_fwd_changed_direction));
    child->connect("changed_speed",     callable_mp(this, &TamaInterpreter::_fwd_changed_speed));
    child->connect("changed_position",  callable_mp(this, &TamaInterpreter::_fwd_changed_position));
    child->connect("accelerated",       callable_mp(this, &TamaInterpreter::_fwd_accelerated));

    child->_scope    = scope_copy;
    child->_running  = true;

    int type_id = (int)act_node->get("type_id");
    if (type_id == (int)TamaNodeType::INLINE_ACT) {
        child->_push_body((Array)act_node->get("body"));
    } else if (type_id == (int)TamaNodeType::ACT_DEF) {
        child->_push_body((Array)act_node->get("body"));
    }
    // ACT_CALL: already resolved by caller

    _async_children.push_back(child);
}

// ---------------------------------------------------------------------------
// Fire emission
// ---------------------------------------------------------------------------

void TamaInterpreter::_exec_fire_call(Object *node) {
    String name  = (String)node->get("name");
    Array  args  = (Array)node->get("args");

    // Scope may hold an InlineFireNode or TamaRef for this name
    Variant scope_val = _scope.get(name, Variant());
    if (scope_val.get_type() == Variant::OBJECT) {
        Object *sv = scope_val.operator Object *();
        if (sv && (int)sv->get("type_id") == (int)TamaNodeType::INLINE_FIRE) {
            _exec_fire_node(sv);
            return;
        }
    }

    std::string ref_name = name.utf8().get_data();
    Array pre_bound;
    Ref<TamaRef> tref = Object::cast_to<TamaRef>(scope_val.operator Object *());
    if (tref.is_valid()) {
        ref_name  = tref->name.utf8().get_data();
        pre_bound = tref->bound_args;
    }

    Object *fire_def = _find_fire(ref_name);
    if (!fire_def) {
        UtilityFunctions::push_warning(String("TamaInterpreter: unknown fire '") + String(ref_name.c_str()) + "'");
        return;
    }

    Array params = (Array)fire_def->get("params");
    Array eval_args;
    for (int i = 0; i < pre_bound.size(); ++i) eval_args.push_back(pre_bound[i]);
    for (int i = 0; i < args.size(); ++i)      eval_args.push_back(_eval_arg(args[i]));

    // Build a temporary scope for the fire def
    Dictionary saved = _scope;
    _scope = _scope_snapshot_plus_params(params, eval_args);
    _exec_fire_node(fire_def);
    _scope = saved;
}

void TamaInterpreter::_exec_fire_node(Object *node) {
    Ref<TamaBulletFireData> data = memnew(TamaBulletFireData);

    // Direction
    Object *dir_node = Object::cast_to<Object>(node->get("dir"));
    if (dir_node) {
        data->dir_type  = _get_dir_type(dir_node);
        data->dir_value = _eval_float((String)dir_node->get("expr"));
    }

    // Speed
    Object *spd_node = Object::cast_to<Object>(node->get("speed"));
    if (spd_node) {
        data->speed_type  = _get_speed_type(spd_node);
        data->speed_value = _eval_float((String)spd_node->get("expr"));
    }

    // Offset
    Object *off_node = Object::cast_to<Object>(node->get("offset"));
    if (off_node) {
        int off_tid = (int)off_node->get("type_id");
        if (off_tid == (int)TamaNodeType::OFFSET_INLINE) {
            data->offset_mode  = 1; // INLINE
            data->offset_value = _eval_float((String)off_node->get("expr"));
        } else if (off_tid == (int)TamaNodeType::OFFSET) {
            data->offset_mode = 2; // BLOCK
            Object *ox = Object::cast_to<Object>(off_node->get("x"));
            Object *oy = Object::cast_to<Object>(off_node->get("y"));
            if (ox) { data->offset_x_type = _get_axis_type(ox); data->offset_x = _eval_float((String)ox->get("expr")); }
            if (oy) { data->offset_y_type = _get_axis_type(oy); data->offset_y = _eval_float((String)oy->get("expr")); }
        }
    }

    // Pos
    Object *pos_node = Object::cast_to<Object>(node->get("pos"));
    if (pos_node && (int)pos_node->get("type_id") == (int)TamaNodeType::POS) {
        data->has_pos = true;
        Object *px = Object::cast_to<Object>(pos_node->get("x"));
        Object *py = Object::cast_to<Object>(pos_node->get("y"));
        if (px) { data->pos_x_set = true; data->pos_x_type = _get_axis_type(px); data->pos_x = _eval_float((String)px->get("expr")); }
        if (py) { data->pos_y_set = true; data->pos_y_type = _get_axis_type(py); data->pos_y = _eval_float((String)py->get("expr")); }
    }

    // Bullet
    Object *bul_node = Object::cast_to<Object>(node->get("bullet"));
    if (bul_node) {
        int bul_tid = (int)bul_node->get("type_id");
        if (bul_tid == (int)TamaNodeType::INLINE_BULLET) {
            String btype = (String)bul_node->get("bullet_type");
            // Resolve bullet_type through scope if it's a variable name
            Variant sv = _scope.get(btype, Variant());
            data->bullet_type       = (sv.get_type() == Variant::STRING) ? (String)sv : btype;
            data->bullet_emitter_act = Object::cast_to<Object>(bul_node->get("emitter_act"));
            data->bullet_act         = Object::cast_to<Object>(bul_node->get("act"));
            Object *mvmt = Object::cast_to<Object>(bul_node->get("mvmt"));
            if (mvmt) _populate_mvmt(data.ptr(), mvmt);
        } else if (bul_tid == (int)TamaNodeType::BULLET_CALL) {
            std::string bul_name = ((String)bul_node->get("name")).utf8().get_data();
            Array       bul_args = (Array)bul_node->get("args");
            Array       pre_bound;

            // Scope override?
            Variant sv = _scope.get(String(bul_name.c_str()), Variant());
            if (sv.get_type() == Variant::OBJECT) {
                Object *svo = sv.operator Object *();
                if (svo && (int)svo->get("type_id") == (int)TamaNodeType::INLINE_BULLET) {
                    String btype = (String)svo->get("bullet_type");
                    Variant btsv = _scope.get(btype, Variant());
                    data->bullet_type        = (btsv.get_type() == Variant::STRING) ? (String)btsv : btype;
                    data->bullet_emitter_act = Object::cast_to<Object>(svo->get("emitter_act"));
                    data->bullet_act         = Object::cast_to<Object>(svo->get("act"));
                    Object *mvmt = Object::cast_to<Object>(svo->get("mvmt"));
                    if (mvmt) _populate_mvmt(data.ptr(), mvmt);
                    goto bullet_done;
                }
                Ref<TamaRef> tref = Object::cast_to<TamaRef>(svo);
                if (tref.is_valid()) {
                    pre_bound = tref->bound_args;
                    bul_name  = tref->name.utf8().get_data();
                }
            }
            {
                Object *bullet_def = _find_bullet(bul_name);
                if (!bullet_def) {
                    UtilityFunctions::push_warning(String("TamaInterpreter: unknown bullet '") + String(bul_name.c_str()) + "'");
                    goto bullet_done;
                }
                Array params = (Array)bullet_def->get("params");
                Array eval_args;
                for (int i = 0; i < pre_bound.size(); ++i) eval_args.push_back(pre_bound[i]);
                for (int i = 0; i < bul_args.size(); ++i)  eval_args.push_back(_eval_arg(bul_args[i]));

                String btype = (String)bullet_def->get("bullet_type");
                Variant btsv = _scope.get(btype, Variant());
                data->bullet_type        = (btsv.get_type() == Variant::STRING) ? (String)btsv : btype;
                data->bullet_emitter_act = Object::cast_to<Object>(bullet_def->get("emitter_act"));
                data->bullet_act         = Object::cast_to<Object>(bullet_def->get("act"));
                data->bullet_params      = params;
                data->bullet_args        = eval_args;
                Object *mvmt = Object::cast_to<Object>(bullet_def->get("mvmt"));
                if (mvmt) _populate_mvmt(data.ptr(), mvmt);
            }
        }
    }
    bullet_done:

    data->source_program = _program;
    emit_signal("bullet_fired", data);
}

void TamaInterpreter::_populate_mvmt(TamaBulletFireData *data, Object *mvmt_node) {
    Object *mx = Object::cast_to<Object>(mvmt_node->get("x"));
    Object *my = Object::cast_to<Object>(mvmt_node->get("y"));
    if (mx) {
        data->mvmt_x_set  = true;
        data->mvmt_x_type = _get_axis_type(mx);
        data->mvmt_x_expr = (String)mx->get("expr");
    }
    if (my) {
        data->mvmt_y_set  = true;
        data->mvmt_y_type = _get_axis_type(my);
        data->mvmt_y_expr = (String)my->get("expr");
    }
}

// ---------------------------------------------------------------------------
// Signal emission helpers
// ---------------------------------------------------------------------------

void TamaInterpreter::_emit_chdir(Object *node) {
    Object *dir_node = Object::cast_to<Object>(node->get("dir"));
    if (!dir_node) return;
    Ref<TamaChdirData> d = memnew(TamaChdirData);
    d->dir_type  = _get_dir_type(dir_node);
    d->dir_value = _eval_float((String)dir_node->get("expr"));
    Object *over = Object::cast_to<Object>(node->get("over"));
    d->over = over ? _eval_float((String)over->get("expr")) : 0.0f;
    emit_signal("changed_direction", d);
}

void TamaInterpreter::_emit_chspd(Object *node) {
    Object *spd_node = Object::cast_to<Object>(node->get("speed"));
    if (!spd_node) return;
    Ref<TamaChspdData> d = memnew(TamaChspdData);
    d->speed_type  = _get_speed_type(spd_node);
    d->speed_value = _eval_float((String)spd_node->get("expr"));
    Object *over = Object::cast_to<Object>(node->get("over"));
    d->over = over ? _eval_float((String)over->get("expr")) : 0.0f;
    emit_signal("changed_speed", d);
}

void TamaInterpreter::_emit_chpos(Object *node) {
    Ref<TamaChposData> d = memnew(TamaChposData);
    Object *xn = Object::cast_to<Object>(node->get("x"));
    Object *yn = Object::cast_to<Object>(node->get("y"));
    Object *ov = Object::cast_to<Object>(node->get("over"));
    if (xn) { d->has_x = true; d->x_type = _get_axis_type(xn); d->x = _eval_float((String)xn->get("expr")); }
    if (yn) { d->has_y = true; d->y_type = _get_axis_type(yn); d->y = _eval_float((String)yn->get("expr")); }
    d->over = ov ? _eval_float((String)ov->get("expr")) : 0.0f;
    emit_signal("changed_position", d);
}

void TamaInterpreter::_emit_accel(Object *node) {
    Object *xn = Object::cast_to<Object>(node->get("x"));
    Object *yn = Object::cast_to<Object>(node->get("y"));
    if (!xn && !yn) return;
    Ref<TamaAccelData> d = memnew(TamaAccelData);
    Object *ov = Object::cast_to<Object>(node->get("over"));
    if (xn) { d->has_x = true; d->x_type = _get_axis_type(xn); d->x = _eval_float((String)xn->get("expr")); }
    if (yn) { d->has_y = true; d->y_type = _get_axis_type(yn); d->y = _eval_float((String)yn->get("expr")); }
    d->over = ov ? _eval_float((String)ov->get("expr")) : 0.0f;
    emit_signal("accelerated", d);
}

// ---------------------------------------------------------------------------
// Expression evaluation
// ---------------------------------------------------------------------------

Variant TamaInterpreter::_eval_arg(const Variant &arg) {
    if (arg.get_type() == Variant::OBJECT) {
        Object *obj = arg.operator Object *();
        if (!obj) return Variant(0.0f);
        int tid = (int)obj->get("type_id");
        if (tid == (int)TamaNodeType::REF_CALL_ARG) {
            // Build a TamaRef
            String ref_name = (String)obj->get("name");
            Array  sub_args = (Array)obj->get("args");
            Ref<TamaRef> tr = memnew(TamaRef);
            tr->name = ref_name;
            for (int i = 0; i < sub_args.size(); ++i)
                tr->bound_args.push_back(_eval_arg(sub_args[i]));
            // Merge with existing TamaRef in scope if present
            Variant sv = _scope.get(ref_name, Variant());
            Ref<TamaRef> existing = Object::cast_to<TamaRef>(sv.operator Object *());
            if (existing.is_valid()) {
                Ref<TamaRef> merged = memnew(TamaRef);
                merged->name = existing->name;
                merged->bound_args = existing->bound_args;
                for (int i = 0; i < tr->bound_args.size(); ++i)
                    merged->bound_args.push_back(tr->bound_args[i]);
                return Variant(merged.ptr());
            }
            return Variant(tr.ptr());
        }
        // Pass AST nodes through as-is (InlineBulletNode, InlineActNode, etc.)
        return arg;
    }

    if (arg.get_type() != Variant::STRING) return arg;

    String expr    = (String)arg;
    String stripped = expr.strip_edges();

    // Fast-path: literal number
    if (stripped.is_valid_float()) return Variant(stripped.to_float());

    // Bare identifier in scope
    if (stripped.is_valid_identifier()) {
        Variant sv = _scope.get(stripped, Variant());
        if (sv.get_type() != Variant::NIL) {
            // Numeric scope value → float
            if (sv.get_type() == Variant::FLOAT || sv.get_type() == Variant::INT || sv.get_type() == Variant::BOOL)
                return Variant((float)sv);
            return sv; // pass through non-numeric (TamaRef, Object, String)
        }
        // Qualifier keywords
        if (stripped == "aim" || stripped == "abs" || stripped == "rel" || stripped == "seq")
            return stripped;
        if (stripped == "true")  return Variant(1.0f);
        if (stripped == "false") return Variant(0.0f);
        return stripped; // treat as bullet/act type string name
    }

    // Full expression — evaluate via TamaExprRuntime
    return Variant(_eval_float(expr));
}

float TamaInterpreter::_eval_float(const String &expr) {
    String s = expr.strip_edges();
    if (s.is_empty()) return 0.0f;
    if (s.is_valid_float()) return s.to_float();

    // Fast-path: single scope variable
    Variant sv = _scope.get(s, Variant());
    if (sv.get_type() != Variant::NIL) {
        if (sv.get_type() == Variant::FLOAT || sv.get_type() == Variant::INT || sv.get_type() == Variant::BOOL)
            return (float)sv;
    }

    // Build var_names / var_values arrays from numeric scope entries
    TamaExprRuntime *er = TamaExprRuntime::get_singleton();
    if (!er) return 0.0f;

    PackedStringArray var_names;
    PackedFloat64Array var_values;
    Array scope_keys = _scope.keys();
    for (int i = 0; i < scope_keys.size(); ++i) {
        Variant val = _scope[scope_keys[i]];
        if (val.get_type() == Variant::FLOAT || val.get_type() == Variant::INT || val.get_type() == Variant::BOOL) {
            var_names.push_back((String)scope_keys[i]);
            var_values.push_back((double)(float)val);
        }
    }

    return (float)(double)er->eval(s, var_names, var_values, _context);
}

float TamaInterpreter::_eval_arg_as_float(const Variant &arg) {
    Variant v = _eval_arg(arg);
    if (v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT || v.get_type() == Variant::BOOL)
        return (float)v;
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Qualifier resolution
// ---------------------------------------------------------------------------

int TamaInterpreter::_get_dir_type(Object *dir_node) const {
    String type_var = (String)dir_node->get("dir_type_var");
    if (type_var.is_empty()) return (int)dir_node->get("dir_type");
    Variant sv = _scope.get(type_var, Variant());
    if (sv.get_type() == Variant::STRING) {
        String s = (String)sv;
        if (s == "aim") return 0; if (s == "abs") return 1;
        if (s == "rel") return 2; if (s == "seq") return 3;
    }
    return 0; // AIM
}

int TamaInterpreter::_get_speed_type(Object *spd_node) const {
    String type_var = (String)spd_node->get("speed_type_var");
    if (type_var.is_empty()) return (int)spd_node->get("speed_type");
    Variant sv = _scope.get(type_var, Variant());
    if (sv.get_type() == Variant::STRING) {
        String s = (String)sv;
        if (s == "abs") return 0; if (s == "rel") return 1; if (s == "seq") return 2;
    }
    return 0; // ABS
}

int TamaInterpreter::_get_axis_type(Object *axis_node) const {
    String type_var = (String)axis_node->get("axis_type_var");
    if (type_var.is_empty()) return (int)axis_node->get("axis_type");
    Variant sv = _scope.get(type_var, Variant());
    if (sv.get_type() == Variant::STRING) {
        String s = (String)sv;
        if (s == "abs") return 0; if (s == "rel") return 1; if (s == "seq") return 2;
    }
    return 1; // REL
}

// ---------------------------------------------------------------------------
// Definition lookups
// ---------------------------------------------------------------------------

Object *TamaInterpreter::_find_fire(const std::string &name) const {
    auto it = _fires_map.find(name);
    return it != _fires_map.end() ? it->second : nullptr;
}

Object *TamaInterpreter::_find_act(const std::string &name) const {
    auto it = _acts_map.find(name);
    return it != _acts_map.end() ? it->second : nullptr;
}

Object *TamaInterpreter::_find_bullet(const std::string &name) const {
    auto it = _bullets_map.find(name);
    return it != _bullets_map.end() ? it->second : nullptr;
}

// ---------------------------------------------------------------------------
// Scope helpers
// ---------------------------------------------------------------------------

Dictionary TamaInterpreter::_scope_snapshot_plus_params(
        const Array &params, const Array &args) const {
    Dictionary d = _scope.duplicate();
    int n = std::min(params.size(), args.size());
    for (int i = 0; i < n; ++i)
        d[(String)params[i]] = args[i];
    return d;
}
