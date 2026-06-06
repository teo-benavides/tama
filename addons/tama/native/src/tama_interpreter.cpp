#include "tama_interpreter.h"
#include "tama_ast_nodes.h"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ---------------------------------------------------------------------------
// Inline helper: cast Variant/Object* to _TamaASTNode*
// ---------------------------------------------------------------------------

static inline _TamaASTNode *astn(Object *o) {
    return Object::cast_to<_TamaASTNode>(o);
}
static inline _TamaASTNode *astn(const Variant &v) {
    return Object::cast_to<_TamaASTNode>(v.operator Object *());
}

// ===========================================================================
// Payload class bindings
// ===========================================================================

void _TamaRef::_bind_methods() {
    // Internal class — no GDScript-accessible properties needed.
}

// ===========================================================================
// _TamaInterpreter
// ===========================================================================

void _TamaInterpreter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("step", "delta"),                 &_TamaInterpreter::step);
    ClassDB::bind_method(D_METHOD("start", "program", "scope"),    &_TamaInterpreter::start);
    ClassDB::bind_method(D_METHOD("start_act", "program", "act", "scope"), &_TamaInterpreter::start_act);
    ClassDB::bind_method(D_METHOD("stop"),                          &_TamaInterpreter::stop);
    ClassDB::bind_method(D_METHOD("is_running"),                    &_TamaInterpreter::is_running);
    ClassDB::bind_method(D_METHOD("set_context", "ctx"),              &_TamaInterpreter::set_context);
    ClassDB::bind_method(D_METHOD("get_context"),                     &_TamaInterpreter::get_context);
    ClassDB::bind_method(D_METHOD("eval_expr", "expr", "scope"),      &_TamaInterpreter::eval_expr);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "context"), "set_context", "get_context");
}

void _TamaInterpreter::_ready() {
    set_physics_process(false);
}

void _TamaInterpreter::_physics_process(double delta) {
    step(delta);
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

void _TamaInterpreter::start(Object *program, Dictionary scope) {
    _program = astn(program);
    _scope   = scope;
    _exec_stack.clear();
    _scope_saves.clear();
    _async_children.clear();
    _running  = true;
    _breaking = false;

    if (!_program) { _running = false; return; }
    _build_lookup_tables();

    _TamaASTNode *main_node = _program->main.ptr();
    if (!main_node) {
        UtilityFunctions::push_error("_TamaInterpreter: program has no main block");
        _running = false;
        return;
    }
    _push_body(main_node->body);

    if (is_inside_tree()) set_physics_process(true);
}

void _TamaInterpreter::start_act(Object *program, Object *act, Dictionary scope) {
    _program = astn(program);
    _scope   = scope;
    _exec_stack.clear();
    _scope_saves.clear();
    _async_children.clear();
    _running  = true;
    _breaking = false;

    if (!_program || !act) { _running = false; return; }
    _build_lookup_tables();

    _TamaASTNode *act_n = astn(act);
    if (!act_n) { _running = false; return; }

    if (act_n->type_id == (int)TamaNodeType::INLINE_ACT) {
        _push_body(act_n->body);
    } else if (act_n->type_id == (int)TamaNodeType::ACT_CALL) {
        const String &name_str = act_n->name;
        std::string name = name_str.utf8().get_data();

        // Check if name resolves through scope (first-class acts / _TamaRef)
        Variant scope_val = _scope.get(name_str, Variant());
        if (scope_val.get_type() == Variant::OBJECT) {
            _TamaASTNode *sv = astn(scope_val);
            if (sv && sv->type_id == (int)TamaNodeType::INLINE_ACT) {
                _push_body(sv->body);
                goto start_act_done;
            }
        }
        {
            Ref<_TamaRef> tref = Object::cast_to<_TamaRef>(scope_val.operator Object *());
            std::string ref_name = name;
            Array pre_bound;
            if (tref.is_valid()) {
                ref_name  = tref->name.utf8().get_data();
                pre_bound = tref->bound_args;
            }
            _TamaASTNode *act_def = _find_act(ref_name);
            if (!act_def) {
                UtilityFunctions::push_error(String("_TamaInterpreter: unknown act '") + String(ref_name.c_str()) + "'");
                _running = false;
                return;
            }
            if (!pre_bound.is_empty()) {
                _scope_saves.push_back(_scope);
                _scope = _scope_snapshot_plus_params(act_def->params, pre_bound);
                _push_body(act_def->body, false, true);
            } else {
                _push_body(act_def->body);
            }
        }
        start_act_done:;
    } else {
        _running = false;
    }

    if (is_inside_tree()) set_physics_process(true);
}

void _TamaInterpreter::stop() {
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

void _TamaInterpreter::step(double p_delta) {
    float delta = (float)p_delta;

    // Step async children first
    for (auto it = _async_children.begin(); it != _async_children.end(); ) {
        _TamaInterpreter *child = *it;
        if (child->is_running()) {
            child->step(delta);
            ++it;
        } else {
            it = _async_children.erase(it);
        }
    }

    if (!_running) {
        // If all async children are done and we were waiting, fire finished callback
        if (_async_children.empty() && _exec_stack.empty()) {
            if (is_inside_tree()) set_physics_process(false);
            if (_finished_cb) _finished_cb();
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
            if (_finished_cb) _finished_cb();
        }
        // else wait for async children to finish (checked next step)
    }
}

// ---------------------------------------------------------------------------
// Frame management
// ---------------------------------------------------------------------------

void _TamaInterpreter::_build_lookup_tables() {
    _fires_map.clear();
    _acts_map.clear();
    _bullets_map.clear();
    if (!_program) return;

    for (int i = 0; i < _program->fires.size(); ++i) {
        _TamaASTNode *f = astn(_program->fires[i]);
        if (f) _fires_map[f->name.utf8().get_data()] = f;
    }
    for (int i = 0; i < _program->acts.size(); ++i) {
        _TamaASTNode *a = astn(_program->acts[i]);
        if (a) _acts_map[a->name.utf8().get_data()] = a;
    }
    for (int i = 0; i < _program->bullets.size(); ++i) {
        _TamaASTNode *b = astn(_program->bullets[i]);
        if (b) _bullets_map[b->name.utf8().get_data()] = b;
    }
}

std::vector<std::string> _TamaInterpreter::_snapshot_scope_keys() const {
    std::vector<std::string> keys;
    Array ks = _scope.keys();
    for (int i = 0; i < ks.size(); ++i)
        keys.push_back(((String)ks[i]).utf8().get_data());
    return keys;
}

static void _fill_body(std::vector<_TamaASTNode*> &dst, const Array &src) {
    dst.reserve(src.size());
    for (int i = 0; i < src.size(); ++i)
        dst.push_back(astn(src[i].operator Object*()));
}

void _TamaInterpreter::_push_body(const Array &body, bool sync_only, bool pops_scope) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::BODY;
    f.pc         = 0;
    f.sync_only  = sync_only;
    f.pops_scope = pops_scope;
    f.pre_keys   = _snapshot_scope_keys();
    _fill_body(f.body, body);
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_body(const std::vector<_TamaASTNode*> &body, bool sync_only, bool pops_scope) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::BODY;
    f.body       = body;
    f.pc         = 0;
    f.sync_only  = sync_only;
    f.pops_scope = pops_scope;
    f.pre_keys   = _snapshot_scope_keys();
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_repeat_ctrl(const Array &body, int n, const String &idx_var) {
    ExecFrame f;
    f.kind           = ExecFrame::Kind::REPEAT_CTRL;
    f.loop_n         = n;
    f.loop_i         = 0;
    f.loop_index_var = idx_var;
    f.between_iters  = false;
    _fill_body(f.loop_body, body);
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_while_ctrl(const Array &body, const String &cond) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::WHILE_CTRL;
    f.while_cond = cond;
    f.loop_n     = -1;
    f.loop_i     = 0;
    _fill_body(f.loop_body, body);
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_repeatf_ctrl(const Array &body, int n, const String &idx_var) {
    ExecFrame f;
    f.kind           = ExecFrame::Kind::REPEATF_CTRL;
    f.loop_n         = n;
    f.loop_i         = 0;
    f.loop_index_var = idx_var;
    f.between_iters  = false;
    _fill_body(f.loop_body, body);
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_pop_body_frame() {
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

float _TamaInterpreter::eval_expr(const String &expr, const Dictionary &scope) {
    Dictionary saved = _scope;
    _scope = scope;
    float result = _eval_float(expr);
    _scope = saved;
    return result;
}

// ---------------------------------------------------------------------------
// Frame stepping
// ---------------------------------------------------------------------------

void _TamaInterpreter::_step_body(ExecFrame &f, bool &yielded) {
    while (f.pc < (int)f.body.size()) {
        if (!_running) return;

        _TamaASTNode *n = f.body[f.pc];
        f.pc++;
        if (!n) continue;

        _exec_node(n, f.sync_only, yielded);

        // If a new frame was pushed, break — the outer loop will handle it
        if (!_exec_stack.empty() && &_exec_stack.back() != &f) return;
        if (yielded) return;
        if (!_running) return;
    }

    // Body exhausted
    _pop_body_frame();
}

void _TamaInterpreter::_step_loop_ctrl(ExecFrame &f, bool &yielded) {
    bool is_repeatf = (f.kind == ExecFrame::Kind::REPEATF_CTRL);

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
        // Yield after each iteration except the last, so each body runs once per frame.
        bool more = (f.loop_n < 0 || f.loop_i < f.loop_n);
        if (more) yielded = true;
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

void _TamaInterpreter::_exec_node(_TamaASTNode *n, bool sync_only, bool &yielded) {
    using NT = TamaNodeType;

    switch ((NT)n->type_id) {

    case NT::BREAK:
        _breaking = true;
        _running  = false; // will be handled in body loop
        return;

    case NT::VANISH:
        _running = false;
        if (_event_handler) _event_handler->on_vanished();
        return;

    case NT::VAR_DECL:
        _scope[n->var_name] = _eval_arg(n->expr);
        return;

    case NT::FIRE_CALL:
        _exec_fire_call(n);
        return;

    case NT::INLINE_FIRE:
        _exec_fire_node(n);
        return;

    case NT::CHDIR:  _emit_chdir(n); return;
    case NT::CHSPD:  _emit_chspd(n); return;
    case NT::CHPOS:  _emit_chpos(n); return;
    case NT::ACCEL:  _emit_accel(n); return;

    case NT::IF: {
        const Array &conditions = n->conditions;
        const Array &bodies     = n->bodies;
        const Array &else_body  = n->else_body;
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
            float secs = _eval_float(n->expr);
            if (secs > 0.0f && !_exec_stack.empty()) {
                _exec_stack.back().suspend    = ExecFrame::Suspend::WAIT_TIME;
                _exec_stack.back().wait_secs  = secs;
                yielded = true;
            }
        }
        return;

    case NT::WAIT_FRAMES:
        if (!sync_only) {
            int frames = (int)std::round(_eval_float(n->expr));
            if (frames > 0 && !_exec_stack.empty()) {
                _exec_stack.back().suspend      = ExecFrame::Suspend::WAIT_FRAMES;
                _exec_stack.back().wait_frames  = frames;
                yielded = true;
            }
        }
        return;

    case NT::REPEAT: {
        String count_str = n->count.strip_edges();
        int count_n = count_str.is_empty() ? -1 : (int)std::round(_eval_float(count_str));
        const String &idx_var = n->index_var;
        const Array  &body    = n->body;
        if (sync_only) {
            // Run all iterations synchronously
            int i = 0;
            while (!idx_var.is_empty() || count_n < 0 || i < count_n) {
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
                if (count_n >= 0 && i >= count_n) break;
            }
        } else {
            _push_repeat_ctrl(body, count_n, idx_var);
        }
        return;
    }

    case NT::WHILE: {
        const String &cond = n->condition;
        const Array  &body = n->body;
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
        String count_str = n->count.strip_edges();
        const String &idx_var = n->index_var;
        const Array  &body    = n->body;

        if (count_str.is_empty()) {
            // Infinite repeatf: run every frame forever (push a REPEATF_CTRL with n=-1)
            if (!sync_only) {
                _push_repeatf_ctrl(body, -1, idx_var);
            }
            // In sync context: no-op (can't run infinite loop)
        } else {
            int rf_n = (int)std::round(_eval_float(count_str));
            if (!sync_only) {
                _push_repeatf_ctrl(body, rf_n, idx_var);
            } else {
                // Sync context: run N times with no frame yield
                for (int i = 0; i < rf_n && _running; ++i) {
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
        const String &name = n->name;
        bool   is_async    = n->is_async;
        const Array &args  = n->args;

        // Resolve through scope for first-class refs or inline acts
        Variant scope_val = _scope.get(name, Variant());
        if (scope_val.get_type() == Variant::OBJECT) {
            _TamaASTNode *sv = astn(scope_val);
            if (sv && sv->type_id == (int)TamaNodeType::INLINE_ACT) {
                if (is_async) {
                    _run_async_act(sv, _scope.duplicate());
                } else {
                    _push_body(sv->body);
                }
                return;
            }
        }
        // _TamaRef in scope?
        Ref<_TamaRef> tref = Object::cast_to<_TamaRef>(scope_val.operator Object *());

        std::string ref_name = name.utf8().get_data();
        Array pre_bound;
        if (tref.is_valid()) {
            ref_name  = tref->name.utf8().get_data();
            pre_bound = tref->bound_args;
        }

        _TamaASTNode *act_def = _find_act(ref_name);
        if (!act_def) {
            UtilityFunctions::push_warning(String("_TamaInterpreter: unknown act '") + String(ref_name.c_str()) + "'");
            return;
        }

        // Evaluate args
        Array eval_args;
        for (int i = 0; i < pre_bound.size(); ++i) eval_args.push_back(pre_bound[i]);
        for (int i = 0; i < args.size(); ++i)      eval_args.push_back(_eval_arg(args[i]));

        if (is_async) {
            Dictionary scope_copy = _scope_snapshot_plus_params(act_def->params, eval_args);
            _run_async_act(act_def, scope_copy);
        } else {
            // Isolated scope: save current, set up new scope = current + params
            _scope_saves.push_back(_scope);
            _scope = _scope_snapshot_plus_params(act_def->params, eval_args);
            _push_body(act_def->body, false, true); // pops_scope=true
        }
        return;
    }

    case NT::INLINE_ACT: {
        if (sync_only) return;
        bool is_async = n->is_async;
        if (is_async) {
            _run_async_act(n, _scope.duplicate());
        } else {
            _push_body(n->body);
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

void _TamaInterpreter::_exec_body_sync(const Array &body) {
    for (int i = 0; i < body.size(); ++i) {
        if (!_running || _breaking) return;
        _TamaASTNode *n = astn(body[i].operator Object*());
        if (!n) continue;
        bool unused = false;
        _exec_node(n, true, unused);
    }
}

void _TamaInterpreter::_exec_body_sync(const std::vector<_TamaASTNode*> &body) {
    for (_TamaASTNode *n : body) {
        if (!_running || _breaking) return;
        if (!n) continue;
        bool unused = false;
        _exec_node(n, true, unused);
    }
}

// ---------------------------------------------------------------------------
// Async acts
// ---------------------------------------------------------------------------

void _TamaInterpreter::_run_async_act(_TamaASTNode *act_n, const Dictionary &scope_copy) {
    _TamaInterpreter *child = memnew(_TamaInterpreter);
    child->_context     = _context;
    child->_program     = _program;
    child->_fires_map   = _fires_map;
    child->_acts_map    = _acts_map;
    child->_bullets_map = _bullets_map;

    child->_fire_cb       = _fire_cb;
    child->_event_handler = _event_handler;

    child->_scope   = scope_copy;
    child->_running = true;

    if (act_n && (act_n->type_id == (int)TamaNodeType::INLINE_ACT ||
                  act_n->type_id == (int)TamaNodeType::ACT_DEF)) {
        child->_push_body(act_n->body);
    }
    // ACT_CALL: already resolved by caller

    _async_children.push_back(child);
}

// ---------------------------------------------------------------------------
// Fire emission
// ---------------------------------------------------------------------------

void _TamaInterpreter::_exec_fire_call(_TamaASTNode *n) {
    const String &name = n->name;
    const Array  &args = n->args;

    // Scope may hold an InlineFireNode or _TamaRef for this name
    Variant scope_val = _scope.get(name, Variant());
    if (scope_val.get_type() == Variant::OBJECT) {
        _TamaASTNode *sv = astn(scope_val);
        if (sv && sv->type_id == (int)TamaNodeType::INLINE_FIRE) {
            _exec_fire_node(sv);
            return;
        }
    }

    std::string ref_name = name.utf8().get_data();
    Array pre_bound;
    Ref<_TamaRef> tref = Object::cast_to<_TamaRef>(scope_val.operator Object *());
    if (tref.is_valid()) {
        ref_name  = tref->name.utf8().get_data();
        pre_bound = tref->bound_args;
    }

    _TamaASTNode *fire_def = _find_fire(ref_name);
    if (!fire_def) {
        UtilityFunctions::push_warning(String("_TamaInterpreter: unknown fire '") + String(ref_name.c_str()) + "'");
        return;
    }

    Array eval_args;
    for (int i = 0; i < pre_bound.size(); ++i) eval_args.push_back(pre_bound[i]);
    for (int i = 0; i < args.size(); ++i)      eval_args.push_back(_eval_arg(args[i]));

    // Build a temporary scope for the fire def
    Dictionary saved = _scope;
    _scope = _scope_snapshot_plus_params(fire_def->params, eval_args);
    _exec_fire_node(fire_def);
    _scope = saved;
}

void _TamaInterpreter::_exec_fire_node(_TamaASTNode *node) {
    TamaBulletFireData data;

    // Direction
    _TamaASTNode *dir_node = node->dir.ptr();
    if (dir_node) {
        data.dir_type  = _get_dir_type(dir_node);
        data.dir_value = _eval_float(dir_node->expr);
    }

    // Speed
    _TamaASTNode *spd_node = node->speed.ptr();
    if (spd_node) {
        data.speed_type  = _get_speed_type(spd_node);
        data.speed_value = _eval_float(spd_node->expr);
    }

    // Offset
    _TamaASTNode *off_node = node->offset.ptr();
    if (off_node) {
        if (off_node->type_id == (int)TamaNodeType::OFFSET_INLINE) {
            data.offset_mode  = 1; // INLINE
            data.offset_value = _eval_float(off_node->expr);
        } else if (off_node->type_id == (int)TamaNodeType::OFFSET) {
            data.offset_mode = 2; // BLOCK
            _TamaASTNode *ox = off_node->x.ptr();
            _TamaASTNode *oy = off_node->y.ptr();
            if (ox) { data.offset_x_type = _get_axis_type(ox); data.offset_x = _eval_float(ox->expr); }
            if (oy) { data.offset_y_type = _get_axis_type(oy); data.offset_y = _eval_float(oy->expr); }
        }
    }

    // Pos
    _TamaASTNode *pos_node = node->pos.ptr();
    if (pos_node && pos_node->type_id == (int)TamaNodeType::POS) {
        data.has_pos = true;
        _TamaASTNode *px = pos_node->x.ptr();
        _TamaASTNode *py = pos_node->y.ptr();
        if (px) { data.pos_x_set = true; data.pos_x_type = _get_axis_type(px); data.pos_x = _eval_float(px->expr); }
        if (py) { data.pos_y_set = true; data.pos_y_type = _get_axis_type(py); data.pos_y = _eval_float(py->expr); }
    }

    // Bullet
    _TamaASTNode *bul_node = node->bullet.ptr();
    if (bul_node) {
        if (bul_node->type_id == (int)TamaNodeType::INLINE_BULLET) {
            const String &btype = bul_node->bullet_type;
            // Resolve bullet_type through scope if it's a variable name
            Variant sv = _scope.get(btype, Variant());
            data.bullet_type        = (sv.get_type() == Variant::STRING) ? (String)sv : btype;
            data.bullet_emitter_act = bul_node->emitter_act.ptr();
            data.bullet_act         = bul_node->act.ptr();
            _TamaASTNode *mvmt = bul_node->mvmt.ptr();
            if (mvmt) _populate_mvmt(&data, mvmt);
        } else if (bul_node->type_id == (int)TamaNodeType::BULLET_CALL) {
            std::string bul_name = bul_node->name.utf8().get_data();
            const Array &bul_args = bul_node->args;
            Array pre_bound;

            // Scope override?
            Variant sv = _scope.get(String(bul_name.c_str()), Variant());
            if (sv.get_type() == Variant::OBJECT) {
                _TamaASTNode *svo = astn(sv);
                if (svo && svo->type_id == (int)TamaNodeType::INLINE_BULLET) {
                    const String &btype = svo->bullet_type;
                    Variant btsv = _scope.get(btype, Variant());
                    data.bullet_type        = (btsv.get_type() == Variant::STRING) ? (String)btsv : btype;
                    data.bullet_emitter_act = svo->emitter_act.ptr();
                    data.bullet_act         = svo->act.ptr();
                    _TamaASTNode *mvmt = svo->mvmt.ptr();
                    if (mvmt) _populate_mvmt(&data, mvmt);
                    goto bullet_done;
                }
                Ref<_TamaRef> tref = Object::cast_to<_TamaRef>(sv.operator Object *());
                if (tref.is_valid()) {
                    pre_bound = tref->bound_args;
                    bul_name  = tref->name.utf8().get_data();
                }
            }
            {
                _TamaASTNode *bullet_def = _find_bullet(bul_name);
                if (!bullet_def) {
                    UtilityFunctions::push_warning(String("_TamaInterpreter: unknown bullet '") + String(bul_name.c_str()) + "'");
                    goto bullet_done;
                }
                Array eval_args;
                for (int i = 0; i < pre_bound.size(); ++i) eval_args.push_back(pre_bound[i]);
                for (int i = 0; i < bul_args.size(); ++i)  eval_args.push_back(_eval_arg(bul_args[i]));

                const String &btype = bullet_def->bullet_type;
                Variant btsv = _scope.get(btype, Variant());
                data.bullet_type        = (btsv.get_type() == Variant::STRING) ? (String)btsv : btype;
                data.bullet_emitter_act = bullet_def->emitter_act.ptr();
                data.bullet_act         = bullet_def->act.ptr();
                data.bullet_params      = bullet_def->params;
                data.bullet_args        = eval_args;
                _TamaASTNode *mvmt = bullet_def->mvmt.ptr();
                if (mvmt) _populate_mvmt(&data, mvmt);
            }
        }
    }
    bullet_done:

    data.source_program = _program;
    if (_fire_cb) _fire_cb(data);
}

void _TamaInterpreter::_populate_mvmt(TamaBulletFireData *data, _TamaASTNode *mvmt_node) {
    _TamaASTNode *mx = mvmt_node->x.ptr();
    _TamaASTNode *my = mvmt_node->y.ptr();
    if (mx) {
        data->mvmt_x_set  = true;
        data->mvmt_x_type = _get_axis_type(mx);
        data->mvmt_x_expr = mx->expr;
    }
    if (my) {
        data->mvmt_y_set  = true;
        data->mvmt_y_type = _get_axis_type(my);
        data->mvmt_y_expr = my->expr;
    }
}

// ---------------------------------------------------------------------------
// Signal emission helpers
// ---------------------------------------------------------------------------

void _TamaInterpreter::_emit_chdir(_TamaASTNode *n) {
    if (!_event_handler) return;
    _TamaASTNode *dir_node = n->dir.ptr();
    if (!dir_node) return;
    _TamaASTNode *over = n->over.ptr();
    TamaChdirEvent e;
    e.dir_type  = _get_dir_type(dir_node);
    e.dir_value = _eval_float(dir_node->expr);
    e.over      = over ? _eval_float(over->expr) : 0.0f;
    _event_handler->on_chdir(e);
}

void _TamaInterpreter::_emit_chspd(_TamaASTNode *n) {
    if (!_event_handler) return;
    _TamaASTNode *spd_node = n->speed.ptr();
    if (!spd_node) return;
    _TamaASTNode *over = n->over.ptr();
    TamaChspdEvent e;
    e.speed_type  = _get_speed_type(spd_node);
    e.speed_value = _eval_float(spd_node->expr);
    e.over        = over ? _eval_float(over->expr) : 0.0f;
    _event_handler->on_chspd(e);
}

void _TamaInterpreter::_emit_chpos(_TamaASTNode *n) {
    if (!_event_handler) return;
    _TamaASTNode *xn = n->x.ptr();
    _TamaASTNode *yn = n->y.ptr();
    _TamaASTNode *ov = n->over.ptr();
    TamaChposEvent e{};
    if (xn) { e.has_x = true; e.x_type = _get_axis_type(xn); e.x = _eval_float(xn->expr); }
    if (yn) { e.has_y = true; e.y_type = _get_axis_type(yn); e.y = _eval_float(yn->expr); }
    e.over = ov ? _eval_float(ov->expr) : 0.0f;
    _event_handler->on_chpos(e);
}

void _TamaInterpreter::_emit_accel(_TamaASTNode *n) {
    if (!_event_handler) return;
    _TamaASTNode *xn = n->x.ptr();
    _TamaASTNode *yn = n->y.ptr();
    if (!xn && !yn) return;
    _TamaASTNode *ov = n->over.ptr();
    TamaAccelEvent e{};
    if (xn) { e.has_x = true; e.x_type = _get_axis_type(xn); e.x = _eval_float(xn->expr); }
    if (yn) { e.has_y = true; e.y_type = _get_axis_type(yn); e.y = _eval_float(yn->expr); }
    e.over = ov ? _eval_float(ov->expr) : 0.0f;
    _event_handler->on_accel(e);
}

// ---------------------------------------------------------------------------
// Expression evaluation
// ---------------------------------------------------------------------------

Variant _TamaInterpreter::_eval_arg(const Variant &arg) {
    if (arg.get_type() == Variant::OBJECT) {
        _TamaASTNode *obj = astn(arg.operator Object *());
        if (!obj) return Variant(0.0f);
        if (obj->type_id == (int)TamaNodeType::REF_CALL_ARG) {
            // Build a _TamaRef
            const String &ref_name = obj->name;
            const Array  &sub_args = obj->args;
            Ref<_TamaRef> tr = memnew(_TamaRef);
            tr->name = ref_name;
            for (int i = 0; i < sub_args.size(); ++i)
                tr->bound_args.push_back(_eval_arg(sub_args[i]));
            // Merge with existing _TamaRef in scope if present
            Variant sv = _scope.get(ref_name, Variant());
            Ref<_TamaRef> existing = Object::cast_to<_TamaRef>(sv.operator Object *());
            if (existing.is_valid()) {
                Ref<_TamaRef> merged = memnew(_TamaRef);
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
            return sv; // pass through non-numeric (_TamaRef, Object, String)
        }
        // Qualifier keywords
        if (stripped == "aim" || stripped == "abs" || stripped == "rel" || stripped == "seq")
            return stripped;
        if (stripped == "true")  return Variant(1.0f);
        if (stripped == "false") return Variant(0.0f);
        return stripped; // treat as bullet/act type string name
    }

    // Full expression — evaluate via _TamaExprRuntime
    return Variant(_eval_float(expr));
}

float _TamaInterpreter::_eval_float(const String &expr) {
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
    _TamaExprRuntime *er = _TamaExprRuntime::get_singleton();
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

float _TamaInterpreter::_eval_arg_as_float(const Variant &arg) {
    Variant v = _eval_arg(arg);
    if (v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT || v.get_type() == Variant::BOOL)
        return (float)v;
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Qualifier resolution
// ---------------------------------------------------------------------------

int _TamaInterpreter::_get_dir_type(_TamaASTNode *dir_node) const {
    if (dir_node->dir_type_var.is_empty()) return dir_node->dir_type;
    Variant sv = _scope.get(dir_node->dir_type_var, Variant());
    if (sv.get_type() == Variant::STRING) {
        String s = (String)sv;
        if (s == "aim") return 0; if (s == "abs") return 1;
        if (s == "rel") return 2; if (s == "seq") return 3;
    }
    return 0; // AIM
}

int _TamaInterpreter::_get_speed_type(_TamaASTNode *spd_node) const {
    if (spd_node->speed_type_var.is_empty()) return spd_node->speed_type;
    Variant sv = _scope.get(spd_node->speed_type_var, Variant());
    if (sv.get_type() == Variant::STRING) {
        String s = (String)sv;
        if (s == "abs") return 0; if (s == "rel") return 1; if (s == "seq") return 2;
    }
    return 0; // ABS
}

int _TamaInterpreter::_get_axis_type(_TamaASTNode *axis_node) const {
    if (axis_node->axis_type_var.is_empty()) return axis_node->axis_type;
    Variant sv = _scope.get(axis_node->axis_type_var, Variant());
    if (sv.get_type() == Variant::STRING) {
        String s = (String)sv;
        if (s == "abs") return 0; if (s == "rel") return 1; if (s == "seq") return 2;
    }
    return 1; // REL
}

// ---------------------------------------------------------------------------
// Definition lookups
// ---------------------------------------------------------------------------

_TamaASTNode *_TamaInterpreter::_find_fire(const std::string &name) const {
    auto it = _fires_map.find(name);
    return it != _fires_map.end() ? it->second : nullptr;
}

_TamaASTNode *_TamaInterpreter::_find_act(const std::string &name) const {
    auto it = _acts_map.find(name);
    return it != _acts_map.end() ? it->second : nullptr;
}

_TamaASTNode *_TamaInterpreter::_find_bullet(const std::string &name) const {
    auto it = _bullets_map.find(name);
    return it != _bullets_map.end() ? it->second : nullptr;
}

// ---------------------------------------------------------------------------
// Scope helpers
// ---------------------------------------------------------------------------

Dictionary _TamaInterpreter::_scope_snapshot_plus_params(
        const Array &params, const Array &args) const {
    Dictionary d = _scope.duplicate();
    int n = std::min(params.size(), args.size());
    for (int i = 0; i < n; ++i)
        d[(String)params[i]] = args[i];
    return d;
}
