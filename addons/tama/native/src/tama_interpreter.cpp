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
    ClassDB::bind_method(D_METHOD("step", "delta"),                  &_TamaInterpreter::step);
    ClassDB::bind_method(D_METHOD("stop"),                           &_TamaInterpreter::stop);
    ClassDB::bind_method(D_METHOD("is_running"),                     &_TamaInterpreter::is_running);
    ClassDB::bind_method(D_METHOD("set_context", "ctx"),             &_TamaInterpreter::set_context);
    ClassDB::bind_method(D_METHOD("get_context"),                    &_TamaInterpreter::get_context);
    ClassDB::bind_method(D_METHOD("eval_expr", "expr", "scope"),     &_TamaInterpreter::eval_expr);

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

void _TamaInterpreter::start(_TamaASTNode *program, TamaScope scope) {
    _program = program;
    _scope   = std::move(scope);
    _exec_stack.clear();
    _scope_saves.clear();
    _async_children.clear();
    _running  = true;
    _breaking = false;

    if (!_program) { _running = false; return; }
    _build_lookup_tables();

    _TamaASTNode *main_node = _program->main.get();
    if (!main_node) {
        UtilityFunctions::push_error("_TamaInterpreter: program has no main block");
        _running = false;
        return;
    }
    _push_body(main_node->body);

    if (is_inside_tree()) set_physics_process(true);
}

void _TamaInterpreter::start_act(_TamaASTNode *program, _TamaASTNode *act_n, TamaScope scope) {
    _program = program;
    _scope   = std::move(scope);
    _exec_stack.clear();
    _scope_saves.clear();
    _async_children.clear();
    _running  = true;
    _breaking = false;

    if (!_program || !act_n) { _running = false; return; }
    _build_lookup_tables();

    if (act_n->type_id == (int)TamaNodeType::INLINE_ACT) {
        _push_body(act_n->body);
    } else if (act_n->type_id == (int)TamaNodeType::ACT_CALL) {
        const String &name_str = act_n->name;
        std::string name = name_str.utf8().get_data();

        // Check if name resolves through scope
        auto sit = _scope.find(name);
        if (sit != _scope.end()) {
            if (sit->second.is_node) {
                _TamaASTNode *sv = sit->second.node;
                if (sv && sv->type_id == (int)TamaNodeType::INLINE_ACT) {
                    _push_body(sv->body);
                    goto start_act_done;
                }
            } else {
                Ref<_TamaRef> tref = Object::cast_to<_TamaRef>(sit->second.var.operator Object *());
                if (tref.is_valid()) {
                    std::string ref_name = tref->name.utf8().get_data();
                    _TamaASTNode *act_def = _find_act(ref_name);
                    if (!act_def) {
                        UtilityFunctions::push_error(String("_TamaInterpreter: unknown act '") + String(ref_name.c_str()) + "'");
                        _running = false;
                        return;
                    }
                    if (!tref->bound_args.empty()) {
                        _scope_saves.push_back(_scope);
                        _scope = _scope_snapshot_plus_params(act_def->params, tref->bound_args);
                        _push_body(act_def->body, false, true);
                    } else {
                        _push_body(act_def->body);
                    }
                    goto start_act_done;
                }
            }
        }
        {
            _TamaASTNode *act_def = _find_act(name);
            if (!act_def) {
                UtilityFunctions::push_error(String("_TamaInterpreter: unknown act '") + String(name.c_str()) + "'");
                _running = false;
                return;
            }
            _push_body(act_def->body);
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
        if (_async_children.empty() && _exec_stack.empty()) {
            if (is_inside_tree()) set_physics_process(false);
            if (_finished_cb) _finished_cb();
        }
        return;
    }

    bool yielded = false;

    while (!yielded && _running && !_exec_stack.empty()) {
        ExecFrame &f = _exec_stack.back();

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

    if (_exec_stack.empty() && _running) {
        _running = false;
        if (_async_children.empty()) {
            if (is_inside_tree()) set_physics_process(false);
            if (_finished_cb) _finished_cb();
        }
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

    for (const auto &sp : _program->fires)
        if (sp) _fires_map[sp->name.utf8().get_data()] = sp.get();
    for (const auto &sp : _program->acts)
        if (sp) _acts_map[sp->name.utf8().get_data()] = sp.get();
    for (const auto &sp : _program->bullets)
        if (sp) _bullets_map[sp->name.utf8().get_data()] = sp.get();
}

std::vector<std::string> _TamaInterpreter::_snapshot_scope_keys() const {
    std::vector<std::string> keys;
    keys.reserve(_scope.size());
    for (const auto &kv : _scope) keys.push_back(kv.first);
    return keys;
}

static void _fill_body(std::vector<_TamaASTNode*> &dst,
                       const std::vector<std::shared_ptr<_TamaASTNode>> &src) {
    dst.reserve(src.size());
    for (const auto &p : src) dst.push_back(p.get());
}

void _TamaInterpreter::_push_body(const std::vector<std::shared_ptr<_TamaASTNode>> &body,
                                   bool sync_only, bool pops_scope) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::BODY;
    f.pc         = 0;
    f.sync_only  = sync_only;
    f.pops_scope = pops_scope;
    f.pre_keys   = _snapshot_scope_keys();
    _fill_body(f.body, body);
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_body(const std::vector<_TamaASTNode*> &body,
                                   bool sync_only, bool pops_scope) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::BODY;
    f.body       = body;
    f.pc         = 0;
    f.sync_only  = sync_only;
    f.pops_scope = pops_scope;
    f.pre_keys   = _snapshot_scope_keys();
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_repeat_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body,
                                          int n, const String &idx_var) {
    ExecFrame f;
    f.kind           = ExecFrame::Kind::REPEAT_CTRL;
    f.loop_n         = n;
    f.loop_i         = 0;
    f.loop_index_var = idx_var;
    f.between_iters  = false;
    _fill_body(f.loop_body, body);
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_while_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body,
                                         const String &cond) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::WHILE_CTRL;
    f.while_cond = cond;
    f.loop_n     = -1;
    f.loop_i     = 0;
    _fill_body(f.loop_body, body);
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_repeatf_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body,
                                           int n, const String &idx_var) {
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

    const auto &pre = f.pre_keys;
    for (auto it = _scope.begin(); it != _scope.end(); ) {
        if (std::find(pre.begin(), pre.end(), it->first) == pre.end())
            it = _scope.erase(it);
        else
            ++it;
    }

    if (f.pops_scope && !_scope_saves.empty()) {
        _scope = std::move(_scope_saves.back());
        _scope_saves.pop_back();
    }

    _exec_stack.pop_back();
}

float _TamaInterpreter::eval_expr(const String &expr, const Dictionary &scope) {
    TamaScope saved = _scope;
    _scope.clear();
    Array keys = scope.keys();
    for (int i = 0; i < keys.size(); ++i) {
        std::string k = ((String)keys[i]).utf8().get_data();
        _scope[k] = TamaScopeVal(scope[keys[i]]);
    }
    float result = _eval_float(expr);
    _scope = std::move(saved);
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

        if (!_exec_stack.empty() && &_exec_stack.back() != &f) return;
        if (yielded) return;
        if (!_running) return;
    }

    _pop_body_frame();
}

void _TamaInterpreter::_step_loop_ctrl(ExecFrame &f, bool &yielded) {
    bool is_repeatf = (f.kind == ExecFrame::Kind::REPEATF_CTRL);

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

    if (!f.loop_index_var.is_empty())
        _scope[f.loop_index_var.utf8().get_data()] = TamaScopeVal(Variant((float)f.loop_i));

    if (is_repeatf) {
        auto pre = _snapshot_scope_keys();
        _exec_body_sync(f.loop_body);
        for (auto it = _scope.begin(); it != _scope.end(); ) {
            if (std::find(pre.begin(), pre.end(), it->first) == pre.end())
                it = _scope.erase(it);
            else
                ++it;
        }
        f.loop_i++;
        bool more = (f.loop_n < 0 || f.loop_i < f.loop_n);
        if (more) yielded = true;
        if (!_running || _breaking) {
            _breaking = false;
            _exec_stack.pop_back();
        }
    } else {
        f.loop_i++;
        _push_body(f.loop_body);
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
        _running  = false;
        return;

    case NT::VANISH:
        _running = false;
        if (_event_handler) _event_handler->on_vanished();
        return;

    case NT::VAR_DECL:
        _scope[n->var_name.utf8().get_data()] = TamaScopeVal(
            _eval_arg(TamaArgVal(Variant(n->expr))));
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
        bool taken = false;
        for (int i = 0; i < (int)n->conditions.size() && !taken; ++i) {
            if (_eval_float(n->conditions[i]) != 0.0f) {
                if (sync_only) _exec_body_sync(n->bodies[i]);
                else           _push_body(n->bodies[i]);
                taken = true;
            }
        }
        if (!taken && !n->else_body.empty()) {
            if (sync_only) _exec_body_sync(n->else_body);
            else           _push_body(n->else_body);
        }
        return;
    }

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
        if (sync_only) {
            int i = 0;
            while (count_n < 0 || i < count_n) {
                if (!idx_var.is_empty())
                    _scope[idx_var.utf8().get_data()] = TamaScopeVal(Variant((float)i));
                auto pre = _snapshot_scope_keys();
                _exec_body_sync(n->body);
                for (auto it = _scope.begin(); it != _scope.end(); ) {
                    if (std::find(pre.begin(), pre.end(), it->first) == pre.end())
                        it = _scope.erase(it);
                    else ++it;
                }
                if (_breaking) { _breaking = false; break; }
                ++i;
                if (count_n >= 0 && i >= count_n) break;
            }
        } else {
            _push_repeat_ctrl(n->body, count_n, idx_var);
        }
        return;
    }

    case NT::WHILE: {
        const String &cond = n->condition;
        if (sync_only) {
            while (_eval_float(cond) != 0.0f) {
                auto pre = _snapshot_scope_keys();
                _exec_body_sync(n->body);
                for (auto it = _scope.begin(); it != _scope.end(); ) {
                    if (std::find(pre.begin(), pre.end(), it->first) == pre.end())
                        it = _scope.erase(it);
                    else ++it;
                }
                if (_breaking) { _breaking = false; break; }
            }
        } else {
            _push_while_ctrl(n->body, cond);
        }
        return;
    }

    case NT::REPEAT_FRAME: {
        String count_str = n->count.strip_edges();
        const String &idx_var = n->index_var;
        if (count_str.is_empty()) {
            if (!sync_only) _push_repeatf_ctrl(n->body, -1, idx_var);
        } else {
            int rf_n = (int)std::round(_eval_float(count_str));
            if (!sync_only) {
                _push_repeatf_ctrl(n->body, rf_n, idx_var);
            } else {
                for (int i = 0; i < rf_n && _running; ++i) {
                    if (!idx_var.is_empty())
                        _scope[idx_var.utf8().get_data()] = TamaScopeVal(Variant((float)i));
                    auto pre = _snapshot_scope_keys();
                    _exec_body_sync(n->body);
                    for (auto it = _scope.begin(); it != _scope.end(); ) {
                        if (std::find(pre.begin(), pre.end(), it->first) == pre.end())
                            it = _scope.erase(it);
                        else ++it;
                    }
                    if (_breaking) { _breaking = false; break; }
                }
            }
        }
        return;
    }

    case NT::ACT_CALL: {
        if (sync_only) return;
        const String &name = n->name;
        bool   is_async    = n->is_async;
        std::string name_s = name.utf8().get_data();

        auto sit = _scope.find(name_s);
        if (sit != _scope.end()) {
            if (sit->second.is_node) {
                _TamaASTNode *sv = sit->second.node;
                if (sv && sv->type_id == (int)TamaNodeType::INLINE_ACT) {
                    if (is_async) _run_async_act(sv, _scope);
                    else          _push_body(sv->body);
                    return;
                }
            } else {
                Ref<_TamaRef> tref = Object::cast_to<_TamaRef>(sit->second.var.operator Object *());
                if (tref.is_valid()) {
                    std::string ref_name = tref->name.utf8().get_data();
                    _TamaASTNode *act_def = _find_act(ref_name);
                    if (!act_def) {
                        UtilityFunctions::push_warning(String("_TamaInterpreter: unknown act '") + String(ref_name.c_str()) + "'");
                        return;
                    }
                    std::vector<TamaArgVal> eval_args;
                    for (const TamaArgVal &ba : tref->bound_args) eval_args.push_back(ba);
                    for (const TamaArgVal &a : n->args)           eval_args.push_back(_eval_arg(a));
                    if (is_async) {
                        TamaScope sc = _scope_snapshot_plus_params(act_def->params, eval_args);
                        _run_async_act(act_def, std::move(sc));
                    } else {
                        _scope_saves.push_back(_scope);
                        _scope = _scope_snapshot_plus_params(act_def->params, eval_args);
                        _push_body(act_def->body, false, true);
                    }
                    return;
                }
            }
        }

        _TamaASTNode *act_def = _find_act(name_s);
        if (!act_def) {
            UtilityFunctions::push_warning(String("_TamaInterpreter: unknown act '") + name + "'");
            return;
        }
        std::vector<TamaArgVal> eval_args;
        for (const TamaArgVal &a : n->args) eval_args.push_back(_eval_arg(a));
        if (is_async) {
            TamaScope sc = _scope_snapshot_plus_params(act_def->params, eval_args);
            _run_async_act(act_def, std::move(sc));
        } else {
            _scope_saves.push_back(_scope);
            _scope = _scope_snapshot_plus_params(act_def->params, eval_args);
            _push_body(act_def->body, false, true);
        }
        return;
    }

    case NT::INLINE_ACT: {
        if (sync_only) return;
        bool is_async = n->is_async;
        if (is_async) _run_async_act(n, _scope);
        else          _push_body(n->body);
        return;
    }

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Sync body execution
// ---------------------------------------------------------------------------

void _TamaInterpreter::_exec_body_sync(const std::vector<std::shared_ptr<_TamaASTNode>> &body) {
    for (const auto &sp : body) {
        if (!_running || _breaking) return;
        _TamaASTNode *n = sp.get();
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

void _TamaInterpreter::_run_async_act(_TamaASTNode *act_n, TamaScope scope_copy) {
    _TamaInterpreter *child = memnew(_TamaInterpreter);
    child->_context     = _context;
    child->_program     = _program;
    child->_fires_map   = _fires_map;
    child->_acts_map    = _acts_map;
    child->_bullets_map = _bullets_map;
    child->_fire_cb       = _fire_cb;
    child->_event_handler = _event_handler;
    child->_scope   = std::move(scope_copy);
    child->_running = true;

    if (act_n && (act_n->type_id == (int)TamaNodeType::INLINE_ACT ||
                  act_n->type_id == (int)TamaNodeType::ACT_DEF)) {
        child->_push_body(act_n->body);
    }
    _async_children.push_back(child);
}

// ---------------------------------------------------------------------------
// Fire emission
// ---------------------------------------------------------------------------

void _TamaInterpreter::_exec_fire_call(_TamaASTNode *n) {
    const String &name = n->name;
    std::string name_s = name.utf8().get_data();

    auto sit = _scope.find(name_s);
    if (sit != _scope.end()) {
        if (sit->second.is_node) {
            _TamaASTNode *sv = sit->second.node;
            if (sv && sv->type_id == (int)TamaNodeType::INLINE_FIRE) {
                _exec_fire_node(sv);
                return;
            }
        } else {
            Ref<_TamaRef> tref = Object::cast_to<_TamaRef>(sit->second.var.operator Object *());
            if (tref.is_valid()) {
                std::string ref_name = tref->name.utf8().get_data();
                _TamaASTNode *fire_def = _find_fire(ref_name);
                if (!fire_def) {
                    UtilityFunctions::push_warning(String("_TamaInterpreter: unknown fire '") + String(ref_name.c_str()) + "'");
                    return;
                }
                std::vector<TamaArgVal> eval_args;
                for (const TamaArgVal &ba : tref->bound_args) eval_args.push_back(ba);
                for (const TamaArgVal &a : n->args)           eval_args.push_back(_eval_arg(a));
                TamaScope saved = _scope;
                _scope = _scope_snapshot_plus_params(fire_def->params, eval_args);
                _exec_fire_node(fire_def);
                _scope = std::move(saved);
                return;
            }
        }
    }

    _TamaASTNode *fire_def = _find_fire(name_s);
    if (!fire_def) {
        UtilityFunctions::push_warning(String("_TamaInterpreter: unknown fire '") + name + "'");
        return;
    }
    std::vector<TamaArgVal> eval_args;
    for (const TamaArgVal &a : n->args) eval_args.push_back(_eval_arg(a));
    TamaScope saved = _scope;
    _scope = _scope_snapshot_plus_params(fire_def->params, eval_args);
    _exec_fire_node(fire_def);
    _scope = std::move(saved);
}

void _TamaInterpreter::_exec_fire_node(_TamaASTNode *node) {
    TamaBulletFireData data;

    _TamaASTNode *dir_node = node->dir.get();
    if (dir_node) {
        data.dir_type  = _get_dir_type(dir_node);
        data.dir_value = _eval_float(dir_node->expr);
    }

    _TamaASTNode *spd_node = node->speed.get();
    if (spd_node) {
        data.speed_type  = _get_speed_type(spd_node);
        data.speed_value = _eval_float(spd_node->expr);
    }

    _TamaASTNode *off_node = node->offset.get();
    if (off_node) {
        if (off_node->type_id == (int)TamaNodeType::OFFSET_INLINE) {
            data.offset_mode  = 1;
            data.offset_value = _eval_float(off_node->expr);
        } else if (off_node->type_id == (int)TamaNodeType::OFFSET) {
            data.offset_mode = 2;
            _TamaASTNode *ox = off_node->x.get();
            _TamaASTNode *oy = off_node->y.get();
            if (ox) { data.offset_x_type = _get_axis_type(ox); data.offset_x = _eval_float(ox->expr); }
            if (oy) { data.offset_y_type = _get_axis_type(oy); data.offset_y = _eval_float(oy->expr); }
        }
    }

    _TamaASTNode *pos_node = node->pos.get();
    if (pos_node && pos_node->type_id == (int)TamaNodeType::POS) {
        data.has_pos = true;
        _TamaASTNode *px = pos_node->x.get();
        _TamaASTNode *py = pos_node->y.get();
        if (px) { data.pos_x_set = true; data.pos_x_type = _get_axis_type(px); data.pos_x = _eval_float(px->expr); }
        if (py) { data.pos_y_set = true; data.pos_y_type = _get_axis_type(py); data.pos_y = _eval_float(py->expr); }
    }

    _TamaASTNode *bul_node = node->bullet.get();
    if (bul_node) {
        if (bul_node->type_id == (int)TamaNodeType::INLINE_BULLET) {
            const String &btype = bul_node->bullet_type;
            auto bsit = _scope.find(btype.utf8().get_data());
            if (bsit != _scope.end() && !bsit->second.is_node &&
                bsit->second.var.get_type() == Variant::STRING)
                data.bullet_type = (String)bsit->second.var;
            else
                data.bullet_type = btype;
            data.bullet_emitter_act = bul_node->emitter_act.get();
            data.bullet_act         = bul_node->act.get();
            _TamaASTNode *mvmt = bul_node->mvmt.get();
            if (mvmt) _populate_mvmt(&data, mvmt);
        } else if (bul_node->type_id == (int)TamaNodeType::BULLET_CALL) {
            std::string bul_name = bul_node->name.utf8().get_data();

            auto bsit = _scope.find(bul_name);
            if (bsit != _scope.end()) {
                if (bsit->second.is_node) {
                    _TamaASTNode *svo = bsit->second.node;
                    if (svo && svo->type_id == (int)TamaNodeType::INLINE_BULLET) {
                        const String &btype = svo->bullet_type;
                        auto btsit = _scope.find(btype.utf8().get_data());
                        if (btsit != _scope.end() && !btsit->second.is_node &&
                            btsit->second.var.get_type() == Variant::STRING)
                            data.bullet_type = (String)btsit->second.var;
                        else
                            data.bullet_type = btype;
                        data.bullet_emitter_act = svo->emitter_act.get();
                        data.bullet_act         = svo->act.get();
                        _TamaASTNode *mvmt = svo->mvmt.get();
                        if (mvmt) _populate_mvmt(&data, mvmt);
                        goto bullet_done;
                    }
                } else {
                    Ref<_TamaRef> tref = Object::cast_to<_TamaRef>(bsit->second.var.operator Object *());
                    if (tref.is_valid()) {
                        bul_name = tref->name.utf8().get_data();
                        _TamaASTNode *bullet_def = _find_bullet(bul_name);
                        if (!bullet_def) {
                            UtilityFunctions::push_warning(String("_TamaInterpreter: unknown bullet '") + String(bul_name.c_str()) + "'");
                            goto bullet_done;
                        }
                        std::vector<TamaArgVal> eval_args;
                        for (const TamaArgVal &ba : tref->bound_args) eval_args.push_back(ba);
                        for (const TamaArgVal &a : bul_node->args)    eval_args.push_back(_eval_arg(a));
                        const String &btype = bullet_def->bullet_type;
                        auto btsit = _scope.find(btype.utf8().get_data());
                        if (btsit != _scope.end() && !btsit->second.is_node &&
                            btsit->second.var.get_type() == Variant::STRING)
                            data.bullet_type = (String)btsit->second.var;
                        else
                            data.bullet_type = btype;
                        data.bullet_emitter_act = bullet_def->emitter_act.get();
                        data.bullet_act         = bullet_def->act.get();
                        data.bullet_params      = bullet_def->params;
                        data.bullet_args        = eval_args;
                        _TamaASTNode *mvmt = bullet_def->mvmt.get();
                        if (mvmt) _populate_mvmt(&data, mvmt);
                        goto bullet_done;
                    }
                }
            }
            {
                _TamaASTNode *bullet_def = _find_bullet(bul_name);
                if (!bullet_def) {
                    UtilityFunctions::push_warning(String("_TamaInterpreter: unknown bullet '") + String(bul_name.c_str()) + "'");
                    goto bullet_done;
                }
                std::vector<TamaArgVal> eval_args;
                for (const TamaArgVal &a : bul_node->args) eval_args.push_back(_eval_arg(a));
                const String &btype = bullet_def->bullet_type;
                auto btsit = _scope.find(btype.utf8().get_data());
                if (btsit != _scope.end() && !btsit->second.is_node &&
                    btsit->second.var.get_type() == Variant::STRING)
                    data.bullet_type = (String)btsit->second.var;
                else
                    data.bullet_type = btype;
                data.bullet_emitter_act = bullet_def->emitter_act.get();
                data.bullet_act         = bullet_def->act.get();
                data.bullet_params      = bullet_def->params;
                data.bullet_args        = eval_args;
                _TamaASTNode *mvmt = bullet_def->mvmt.get();
                if (mvmt) _populate_mvmt(&data, mvmt);
            }
        }
    }
    bullet_done:

    data.source_program = _program;
    if (_fire_cb) _fire_cb(data);
}

void _TamaInterpreter::_populate_mvmt(TamaBulletFireData *data, _TamaASTNode *mvmt_node) {
    _TamaASTNode *mx = mvmt_node->x.get();
    _TamaASTNode *my = mvmt_node->y.get();
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
    _TamaASTNode *dir_node = n->dir.get();
    if (!dir_node) return;
    _TamaASTNode *over = n->over.get();
    TamaChdirEvent e;
    e.dir_type  = _get_dir_type(dir_node);
    e.dir_value = _eval_float(dir_node->expr);
    e.over      = over ? _eval_float(over->expr) : 0.0f;
    _event_handler->on_chdir(e);
}

void _TamaInterpreter::_emit_chspd(_TamaASTNode *n) {
    if (!_event_handler) return;
    _TamaASTNode *spd_node = n->speed.get();
    if (!spd_node) return;
    _TamaASTNode *over = n->over.get();
    TamaChspdEvent e;
    e.speed_type  = _get_speed_type(spd_node);
    e.speed_value = _eval_float(spd_node->expr);
    e.over        = over ? _eval_float(over->expr) : 0.0f;
    _event_handler->on_chspd(e);
}

void _TamaInterpreter::_emit_chpos(_TamaASTNode *n) {
    if (!_event_handler) return;
    _TamaASTNode *xn = n->x.get();
    _TamaASTNode *yn = n->y.get();
    _TamaASTNode *ov = n->over.get();
    TamaChposEvent e{};
    if (xn) { e.has_x = true; e.x_type = _get_axis_type(xn); e.x = _eval_float(xn->expr); }
    if (yn) { e.has_y = true; e.y_type = _get_axis_type(yn); e.y = _eval_float(yn->expr); }
    e.over = ov ? _eval_float(ov->expr) : 0.0f;
    _event_handler->on_chpos(e);
}

void _TamaInterpreter::_emit_accel(_TamaASTNode *n) {
    if (!_event_handler) return;
    _TamaASTNode *xn = n->x.get();
    _TamaASTNode *yn = n->y.get();
    if (!xn && !yn) return;
    _TamaASTNode *ov = n->over.get();
    TamaAccelEvent e{};
    if (xn) { e.has_x = true; e.x_type = _get_axis_type(xn); e.x = _eval_float(xn->expr); }
    if (yn) { e.has_y = true; e.y_type = _get_axis_type(yn); e.y = _eval_float(yn->expr); }
    e.over = ov ? _eval_float(ov->expr) : 0.0f;
    _event_handler->on_accel(e);
}

// ---------------------------------------------------------------------------
// Expression evaluation
// ---------------------------------------------------------------------------

TamaArgVal _TamaInterpreter::_eval_arg(const TamaArgVal &arg) {
    if (arg.is_node) {
        _TamaASTNode *obj = arg.node;
        if (!obj) return TamaArgVal(Variant(0.0f));
        if (obj->type_id == (int)TamaNodeType::REF_CALL_ARG) {
            Ref<_TamaRef> tr = memnew(_TamaRef);
            tr->name = obj->name;
            for (const TamaArgVal &sa : obj->args)
                tr->bound_args.push_back(_eval_arg(sa));
            auto sit = _scope.find(obj->name.utf8().get_data());
            if (sit != _scope.end() && !sit->second.is_node) {
                Ref<_TamaRef> existing = Object::cast_to<_TamaRef>(sit->second.var.operator Object *());
                if (existing.is_valid()) {
                    Ref<_TamaRef> merged = memnew(_TamaRef);
                    merged->name = existing->name;
                    merged->bound_args = existing->bound_args;
                    for (const TamaArgVal &ba : tr->bound_args) merged->bound_args.push_back(ba);
                    return TamaArgVal(Variant(merged.ptr()));
                }
            }
            return TamaArgVal(Variant(tr.ptr()));
        }
        return arg;
    }

    if (arg.var.get_type() != Variant::STRING) return arg;

    String expr    = (String)arg.var;
    String stripped = expr.strip_edges();

    if (stripped.is_valid_float()) return TamaArgVal(Variant(stripped.to_float()));

    if (stripped.is_valid_identifier()) {
        auto sit = _scope.find(stripped.utf8().get_data());
        if (sit != _scope.end()) {
            if (sit->second.is_node) return TamaArgVal(sit->second.node);
            const Variant &sv = sit->second.var;
            if (sv.get_type() == Variant::FLOAT || sv.get_type() == Variant::INT || sv.get_type() == Variant::BOOL)
                return TamaArgVal(Variant((float)sv));
            return TamaArgVal(sv);
        }
        if (stripped == "aim" || stripped == "abs" || stripped == "rel" || stripped == "seq")
            return TamaArgVal(Variant(stripped));
        if (stripped == "true")  return TamaArgVal(Variant(1.0f));
        if (stripped == "false") return TamaArgVal(Variant(0.0f));
        return TamaArgVal(Variant(stripped));
    }

    return TamaArgVal(Variant(_eval_float(expr)));
}

float _TamaInterpreter::_eval_float(const String &expr) {
    String s = expr.strip_edges();
    if (s.is_empty()) return 0.0f;
    if (s.is_valid_float()) return s.to_float();

    auto sit = _scope.find(s.utf8().get_data());
    if (sit != _scope.end() && !sit->second.is_node) {
        const Variant &sv = sit->second.var;
        if (sv.get_type() == Variant::FLOAT || sv.get_type() == Variant::INT || sv.get_type() == Variant::BOOL)
            return (float)sv;
    }

    _TamaExprRuntime *er = _TamaExprRuntime::get_singleton();
    if (!er) return 0.0f;

    PackedStringArray var_names;
    PackedFloat64Array var_values;
    for (const auto &kv : _scope) {
        if (!kv.second.is_node) {
            const Variant &val = kv.second.var;
            if (val.get_type() == Variant::FLOAT || val.get_type() == Variant::INT || val.get_type() == Variant::BOOL) {
                var_names.push_back(String(kv.first.c_str()));
                var_values.push_back((double)(float)val);
            }
        }
    }

    return (float)(double)er->eval(s, var_names, var_values, _context);
}

float _TamaInterpreter::_eval_arg_float(const TamaArgVal &arg) {
    TamaArgVal v = _eval_arg(arg);
    if (!v.is_node && (v.var.get_type() == Variant::FLOAT || v.var.get_type() == Variant::INT || v.var.get_type() == Variant::BOOL))
        return (float)v.var;
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Qualifier resolution
// ---------------------------------------------------------------------------

int _TamaInterpreter::_get_dir_type(_TamaASTNode *dir_node) const {
    if (dir_node->dir_type_var.is_empty()) return dir_node->dir_type;
    auto sit = _scope.find(dir_node->dir_type_var.utf8().get_data());
    if (sit != _scope.end() && !sit->second.is_node && sit->second.var.get_type() == Variant::STRING) {
        String s = (String)sit->second.var;
        if (s == "aim") return 0; if (s == "abs") return 1;
        if (s == "rel") return 2; if (s == "seq") return 3;
    }
    return 0;
}

int _TamaInterpreter::_get_speed_type(_TamaASTNode *spd_node) const {
    if (spd_node->speed_type_var.is_empty()) return spd_node->speed_type;
    auto sit = _scope.find(spd_node->speed_type_var.utf8().get_data());
    if (sit != _scope.end() && !sit->second.is_node && sit->second.var.get_type() == Variant::STRING) {
        String s = (String)sit->second.var;
        if (s == "abs") return 0; if (s == "rel") return 1; if (s == "seq") return 2;
    }
    return 0;
}

int _TamaInterpreter::_get_axis_type(_TamaASTNode *axis_node) const {
    if (axis_node->axis_type_var.is_empty()) return axis_node->axis_type;
    auto sit = _scope.find(axis_node->axis_type_var.utf8().get_data());
    if (sit != _scope.end() && !sit->second.is_node && sit->second.var.get_type() == Variant::STRING) {
        String s = (String)sit->second.var;
        if (s == "abs") return 0; if (s == "rel") return 1; if (s == "seq") return 2;
    }
    return 1;
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

TamaScope _TamaInterpreter::_scope_snapshot_plus_params(
        const std::vector<godot::String> &params,
        const std::vector<TamaArgVal> &args) const {
    TamaScope scope = _scope;
    int n = (int)std::min(params.size(), args.size());
    for (int i = 0; i < n; ++i)
        scope[params[i].utf8().get_data()] = TamaScopeVal(args[i]);
    return scope;
}
