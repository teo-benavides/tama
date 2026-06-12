#include "tama_interpreter.h"
#include "tama_ast_nodes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ===========================================================================
// _TamaInterpreter
// ===========================================================================

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
        const std::string &name = act_n->name;

        auto sit = _scope.find(name);
        if (sit != _scope.end()) {
            if (sit->second.kind == TamaScopeVal::NODE) {
                _TamaASTNode *sv = sit->second.node;
                if (sv && sv->type_id == (int)TamaNodeType::INLINE_ACT) {
                    _push_body(sv->body);
                    goto start_act_done;
                }
            } else if (sit->second.kind == TamaScopeVal::REF) {
                const auto &tref = sit->second.ref;
                _TamaASTNode *act_def = _find_act(tref->name);
                if (!act_def) {
                    UtilityFunctions::push_error(String("_TamaInterpreter: unknown act '") + String(tref->name.c_str()) + "'");
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
}

void _TamaInterpreter::stop() {
    _running = false;
    _exec_stack.clear();
    for (auto &child : _async_children)
        child->stop();
    _async_children.clear();
}

// ---------------------------------------------------------------------------
// Step — called every physics frame by the owning Node
// ---------------------------------------------------------------------------

void _TamaInterpreter::step(double p_delta) {
    float delta = (float)p_delta;

    for (auto it = _async_children.begin(); it != _async_children.end(); ) {
        if ((*it)->is_running()) {
            (*it)->step(delta);
            ++it;
        } else {
            it = _async_children.erase(it);
        }
    }

    if (!_running) {
        if (_async_children.empty() && _exec_stack.empty()) {
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
        if (sp) _fires_map[sp->name] = sp.get();
    for (const auto &sp : _program->acts)
        if (sp) _acts_map[sp->name] = sp.get();
    for (const auto &sp : _program->bullets)
        if (sp) _bullets_map[sp->name] = sp.get();
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
                                          int n, const std::string &idx_var) {
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
                                         const std::string &cond) {
    ExecFrame f;
    f.kind       = ExecFrame::Kind::WHILE_CTRL;
    f.while_cond = cond;
    f.loop_n     = -1;
    f.loop_i     = 0;
    _fill_body(f.loop_body, body);
    _exec_stack.push_back(std::move(f));
}

void _TamaInterpreter::_push_repeatf_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body,
                                           int n, const std::string &idx_var) {
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

    if (!f.loop_index_var.empty())
        _scope[f.loop_index_var] = TamaScopeVal((float)f.loop_i);

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
        _scope[n->var_name] = TamaScopeVal(
            _eval_arg(TamaArgVal(Variant(String(n->expr.c_str())))));
        return;

    case NT::FIRE_CALL:
        _exec_fire_call(n);
        return;

    case NT::INLINE_FIRE:
        _exec_fire_node(n);
        return;

    case NT::CONTEXT_CALL:
        _exec_context_call(n);
        return;

    case NT::CHDIR:    _emit_chdir(n);    return;
    case NT::CHSPD:    _emit_chspd(n);    return;
    case NT::CHROTSPD: _emit_chrotspd(n); return;
    case NT::CHPOS:    _emit_chpos(n);    return;
    case NT::ACCEL:    _emit_accel(n);    return;
    case NT::EVENT:    _emit_event(n);    return;

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
        const std::string &count_str = n->count;
        int count_n = count_str.empty() ? -1 : (int)std::round(_eval_float(count_str));
        const std::string &idx_var = n->index_var;
        if (sync_only) {
            int i = 0;
            while (count_n < 0 || i < count_n) {
                if (!idx_var.empty())
                    _scope[idx_var] = TamaScopeVal((float)i);
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
        const std::string &cond = n->condition;
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
        const std::string &count_str = n->count;
        const std::string &idx_var   = n->index_var;
        if (count_str.empty()) {
            if (!sync_only) _push_repeatf_ctrl(n->body, -1, idx_var);
        } else {
            int rf_n = (int)std::round(_eval_float(count_str));
            if (!sync_only) {
                _push_repeatf_ctrl(n->body, rf_n, idx_var);
            } else {
                for (int i = 0; i < rf_n && _running; ++i) {
                    if (!idx_var.empty())
                        _scope[idx_var] = TamaScopeVal((float)i);
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
        const std::string &name = n->name;
        bool is_async = n->is_async;

        auto sit = _scope.find(name);
        if (sit != _scope.end()) {
            if (sit->second.kind == TamaScopeVal::NODE) {
                _TamaASTNode *sv = sit->second.node;
                if (sv && sv->type_id == (int)TamaNodeType::INLINE_ACT) {
                    if (is_async) _run_async_act(sv, _scope);
                    else          _push_body(sv->body);
                    return;
                }
            } else if (sit->second.kind == TamaScopeVal::REF) {
                const auto &tref = sit->second.ref;
                _TamaASTNode *act_def = _find_act(tref->name);
                if (!act_def) {
                    UtilityFunctions::push_warning(String("_TamaInterpreter: unknown act '") + String(tref->name.c_str()) + "'");
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

        _TamaASTNode *act_def = _find_act(name);
        if (!act_def) {
            UtilityFunctions::push_warning(String("_TamaInterpreter: unknown act '") + String(name.c_str()) + "'");
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
    auto child = std::make_unique<_TamaInterpreter>();
    child->_context     = _context;
    child->_program     = _program;
    child->_fires_map   = _fires_map;
    child->_acts_map    = _acts_map;
    child->_bullets_map = _bullets_map;
    child->_fire_cb       = _fire_cb;
    child->_event_cb      = _event_cb;
    child->_event_handler = _event_handler;
    child->_scope   = std::move(scope_copy);
    child->_running = true;

    if (act_n && (act_n->type_id == (int)TamaNodeType::INLINE_ACT ||
                  act_n->type_id == (int)TamaNodeType::ACT_DEF)) {
        child->_push_body(act_n->body);
    }
    _async_children.push_back(std::move(child));
}

// ---------------------------------------------------------------------------
// Fire emission
// ---------------------------------------------------------------------------

void _TamaInterpreter::_exec_fire_call(_TamaASTNode *n) {
    const std::string &name = n->name;

    auto sit = _scope.find(name);
    if (sit != _scope.end()) {
        if (sit->second.kind == TamaScopeVal::NODE) {
            _TamaASTNode *sv = sit->second.node;
            if (sv && sv->type_id == (int)TamaNodeType::INLINE_FIRE) {
                _exec_fire_node(sv);
                return;
            }
        } else if (sit->second.kind == TamaScopeVal::REF) {
            const auto &tref = sit->second.ref;
            _TamaASTNode *fire_def = _find_fire(tref->name);
            if (!fire_def) {
                UtilityFunctions::push_warning(String("_TamaInterpreter: unknown fire '") + String(tref->name.c_str()) + "'");
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

    _TamaASTNode *fire_def = _find_fire(name);
    if (!fire_def) {
        UtilityFunctions::push_warning(String("_TamaInterpreter: unknown fire '") + String(name.c_str()) + "'");
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

    _TamaASTNode *rotspd_node = node->rotspd.get();
    if (rotspd_node) {
        data.has_rot_speed   = true;
        data.rot_speed_type  = _get_speed_type(rotspd_node);
        data.rot_speed_value = _eval_float(rotspd_node->expr);
    }

    _TamaASTNode *delay_node = node->delay.get();
    if (delay_node) {
        int d = (int)_eval_float(delay_node->expr);
        data.spawn_delay_override = d < 0 ? 0 : d;
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
            const std::string &btype = bul_node->bullet_type;
            auto bsit = _scope.find(btype);
            if (bsit != _scope.end() && bsit->second.kind == TamaScopeVal::STRING)
                data.bullet_type = bsit->second.str;
            else
                data.bullet_type = btype;
            data.bullet_emitter_act = bul_node->emitter_act.get();
            data.bullet_act         = bul_node->act.get();
            _TamaASTNode *mvmt = bul_node->mvmt.get();
            if (mvmt) _populate_mvmt(&data, mvmt);
            if (!bul_node->count.empty()) {
                data.bounces_max  = (int)_eval_float(bul_node->count);
                data.bounces_axis = bul_node->axis == "x" ? 1
                                  : bul_node->axis == "y" ? 2 : 0;
            }
        } else if (bul_node->type_id == (int)TamaNodeType::BULLET_CALL) {
            std::string bul_name = bul_node->name;

            auto bsit = _scope.find(bul_name);
            if (bsit != _scope.end()) {
                if (bsit->second.kind == TamaScopeVal::NODE) {
                    _TamaASTNode *svo = bsit->second.node;
                    if (svo && svo->type_id == (int)TamaNodeType::INLINE_BULLET) {
                        const std::string &btype = svo->bullet_type;
                        auto btsit = _scope.find(btype);
                        if (btsit != _scope.end() && btsit->second.kind == TamaScopeVal::STRING)
                            data.bullet_type = btsit->second.str;
                        else
                            data.bullet_type = btype;
                        data.bullet_emitter_act = svo->emitter_act.get();
                        data.bullet_act         = svo->act.get();
                        _TamaASTNode *mvmt = svo->mvmt.get();
                        if (mvmt) _populate_mvmt(&data, mvmt);
                        if (!svo->count.empty()) {
                            data.bounces_max  = (int)_eval_float(svo->count);
                            data.bounces_axis = svo->axis == "x" ? 1
                                              : svo->axis == "y" ? 2 : 0;
                        }
                        goto bullet_done;
                    }
                } else if (bsit->second.kind == TamaScopeVal::REF) {
                    const auto &tref = bsit->second.ref;
                    bul_name = tref->name;
                    _TamaASTNode *bullet_def = _find_bullet(bul_name);
                    if (!bullet_def) {
                        UtilityFunctions::push_warning(String("_TamaInterpreter: unknown bullet '") + String(bul_name.c_str()) + "'");
                        goto bullet_done;
                    }
                    std::vector<TamaArgVal> eval_args;
                    for (const TamaArgVal &ba : tref->bound_args) eval_args.push_back(ba);
                    for (const TamaArgVal &a : bul_node->args)    eval_args.push_back(_eval_arg(a));
                    const std::string &btype = bullet_def->bullet_type;
                    auto btsit = _scope.find(btype);
                    if (btsit != _scope.end() && btsit->second.kind == TamaScopeVal::STRING)
                        data.bullet_type = btsit->second.str;
                    else
                        data.bullet_type = btype;
                    data.bullet_emitter_act = bullet_def->emitter_act.get();
                    data.bullet_act         = bullet_def->act.get();
                    data.bullet_params      = bullet_def->params;
                    data.bullet_args        = eval_args;
                    _TamaASTNode *mvmt = bullet_def->mvmt.get();
                    if (mvmt) _populate_mvmt(&data, mvmt);
                    if (!bullet_def->count.empty()) {
                        data.bounces_max  = (int)_eval_float(bullet_def->count);
                        data.bounces_axis = bullet_def->axis == "x" ? 1
                                          : bullet_def->axis == "y" ? 2 : 0;
                    }
                    goto bullet_done;
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
                const std::string &btype = bullet_def->bullet_type;
                auto btsit = _scope.find(btype);
                if (btsit != _scope.end() && btsit->second.kind == TamaScopeVal::STRING)
                    data.bullet_type = btsit->second.str;
                else
                    data.bullet_type = btype;
                data.bullet_emitter_act = bullet_def->emitter_act.get();
                data.bullet_act         = bullet_def->act.get();
                data.bullet_params      = bullet_def->params;
                data.bullet_args        = eval_args;
                _TamaASTNode *mvmt = bullet_def->mvmt.get();
                if (mvmt) _populate_mvmt(&data, mvmt);
                if (!bullet_def->count.empty()) {
                    data.bounces_max  = (int)_eval_float(bullet_def->count);
                    data.bounces_axis = bullet_def->axis == "x" ? 1
                                      : bullet_def->axis == "y" ? 2 : 0;
                }
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

void _TamaInterpreter::_emit_chrotspd(_TamaASTNode *n) {
    if (!_event_handler) return;
    _TamaASTNode *spd_node = n->speed.get();
    if (!spd_node) return;
    _TamaASTNode *over = n->over.get();
    TamaChrotspdEvent e;
    e.speed_type  = _get_speed_type(spd_node);
    e.speed_value = _eval_float(spd_node->expr);
    e.over        = over ? _eval_float(over->expr) : 0.0f;
    _event_handler->on_chrotspd(e);
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

void _TamaInterpreter::_emit_event(_TamaASTNode *n) {
    if (!_event_cb) return;
    godot::Array args;
    for (const TamaArgVal &raw_arg : n->args) {
        if (raw_arg.is_literal_string) {
            args.push_back(raw_arg.var);
        } else {
            TamaArgVal ev = _eval_arg(raw_arg);
            if (!ev.is_node) args.push_back(ev.var);
        }
    }
    _event_cb(n->name, args);
}

// ---------------------------------------------------------------------------
// Context call
// ---------------------------------------------------------------------------

void _TamaInterpreter::_exec_context_call(_TamaASTNode *n) {
    if (!_context) {
        UtilityFunctions::push_warning(
            String("TamaScript: context call '") + String(n->name.c_str()) + "' skipped — no context set");
        return;
    }
    godot::String method_name(n->name.c_str());
    if (!_context->has_method(method_name)) {
        UtilityFunctions::push_warning(
            String("TamaScript: context has no method '") + method_name + "'");
        return;
    }
    godot::Array call_args;
    for (const TamaArgVal &raw_arg : n->args) {
        if (raw_arg.is_literal_string) {
            call_args.push_back(raw_arg.var);
        } else {
            TamaArgVal ev = _eval_arg(raw_arg);
            if (!ev.is_node) call_args.push_back(ev.var);
        }
    }
    _context->callv(method_name, call_args);
}

// ---------------------------------------------------------------------------
// Expression evaluation
// ---------------------------------------------------------------------------

TamaArgVal _TamaInterpreter::_eval_arg(const TamaArgVal &arg) {
    if (arg.is_literal_string) return arg;
    if (arg.ref) return arg;

    if (arg.is_node) {
        _TamaASTNode *obj = arg.node;
        if (!obj) return TamaArgVal(Variant(0.0f));
        if (obj->type_id == (int)TamaNodeType::REF_CALL_ARG) {
            auto tr = std::make_shared<TamaRef>();
            tr->name = obj->name;
            for (const TamaArgVal &sa : obj->args)
                tr->bound_args.push_back(_eval_arg(sa));
            // Merge with existing ref in scope if present
            auto sit = _scope.find(obj->name);
            if (sit != _scope.end() && sit->second.kind == TamaScopeVal::REF) {
                auto merged = std::make_shared<TamaRef>();
                merged->name = sit->second.ref->name;
                merged->bound_args = sit->second.ref->bound_args;
                for (const TamaArgVal &ba : tr->bound_args) merged->bound_args.push_back(ba);
                return TamaArgVal(merged);
            }
            return TamaArgVal(tr);
        }
        return arg;
    }

    if (arg.var.get_type() != Variant::STRING) return arg;

    String expr    = (String)arg.var;
    String stripped = expr.strip_edges();

    if (stripped.is_valid_float()) return TamaArgVal(Variant(stripped.to_float()));

    if (stripped.is_valid_identifier()) {
        auto sit = _scope.find(std::string(stripped.utf8().get_data()));
        if (sit != _scope.end()) {
            switch (sit->second.kind) {
                case TamaScopeVal::REF:    return TamaArgVal(sit->second.ref);
                case TamaScopeVal::NODE:   return TamaArgVal(sit->second.node);
                case TamaScopeVal::FLOAT:  return TamaArgVal(Variant(sit->second.f));
                case TamaScopeVal::STRING: return TamaArgVal(Variant(String(sit->second.str.c_str())));
            }
        }
        if (stripped == "aim" || stripped == "abs" || stripped == "rel" || stripped == "seq")
            return TamaArgVal(Variant(stripped));
        if (stripped == "true")  return TamaArgVal(Variant(1.0f));
        if (stripped == "false") return TamaArgVal(Variant(0.0f));
        return TamaArgVal(Variant(stripped));
    }

    return TamaArgVal(Variant(_eval_float(std::string(expr.utf8().get_data()))));
}

float _TamaInterpreter::_eval_float(const std::string &expr) {
    // Trim whitespace
    size_t beg = expr.find_first_not_of(" \t");
    if (beg == std::string::npos) return 0.0f;
    size_t end = expr.find_last_not_of(" \t");
    const std::string s = (beg == 0 && end == expr.size() - 1) ? expr : expr.substr(beg, end - beg + 1);

    // Fast numeric path
    char *endp;
    float fv = std::strtof(s.c_str(), &endp);
    if (endp == s.c_str() + s.size()) return fv;

    // Identifier path — direct scope lookup
    bool is_ident = !s.empty() && (std::isalpha((unsigned char)s[0]) || s[0] == '_');
    if (is_ident) {
        for (size_t i = 1; i < s.size(); ++i) {
            if (!std::isalnum((unsigned char)s[i]) && s[i] != '_') { is_ident = false; break; }
        }
    }
    if (is_ident) {
        auto sit = _scope.find(s);
        if (sit != _scope.end() && sit->second.kind == TamaScopeVal::FLOAT)
            return sit->second.f;
    }

    // Full expression evaluator — build native var arrays (no Packed array allocation)
    _TamaExprRuntime *er = _TamaExprRuntime::get_singleton();
    if (!er) return 0.0f;

    std::vector<std::string> var_names;
    std::vector<double> var_values;
    for (const auto &kv : _scope) {
        if (kv.second.kind == TamaScopeVal::FLOAT) {
            var_names.push_back(kv.first);
            var_values.push_back((double)kv.second.f);
        }
    }

    std::string cache_key = s + "|";
    for (size_t i = 0; i < var_names.size(); ++i) {
        if (i > 0) cache_key += ',';
        cache_key += var_names[i];
    }

    const TamaExprChunk *chunk = er->get_chunk(s, var_names, cache_key);
    if (!chunk || chunk->code.empty()) return 0.0f;
    return (float)er->eval_chunk(*chunk, var_values.data(), var_values.size(), _context);
}

float _TamaInterpreter::eval_expr_native(
        const std::string &expr,
        const std::vector<std::string> &var_names,
        const std::vector<double> &var_values)
{
    // Fast numeric path
    char *endp;
    float fv = std::strtof(expr.c_str(), &endp);
    if (endp == expr.c_str() + expr.size()) return fv;

    // Fast path: exact variable name match
    for (size_t i = 0; i < var_names.size(); ++i)
        if (var_names[i] == expr) return (float)var_values[i];

    _TamaExprRuntime *er = _TamaExprRuntime::get_singleton();
    if (!er) return 0.0f;

    std::string cache_key = expr + "|";
    for (size_t i = 0; i < var_names.size(); ++i) {
        if (i > 0) cache_key += ',';
        cache_key += var_names[i];
    }

    const TamaExprChunk *chunk = er->get_chunk(expr, var_names, cache_key);
    if (!chunk || chunk->code.empty()) return 0.0f;
    return (float)er->eval_chunk(*chunk, var_values.data(), var_values.size(), _context);
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
    if (dir_node->dir_type_var.empty()) return dir_node->dir_type;
    auto sit = _scope.find(dir_node->dir_type_var);
    if (sit != _scope.end() && sit->second.kind == TamaScopeVal::STRING) {
        const std::string &s = sit->second.str;
        if (s == "aim") return 0; if (s == "abs") return 1;
        if (s == "rel") return 2; if (s == "seq") return 3;
    }
    return 0;
}

int _TamaInterpreter::_get_speed_type(_TamaASTNode *spd_node) const {
    if (spd_node->speed_type_var.empty()) return spd_node->speed_type;
    auto sit = _scope.find(spd_node->speed_type_var);
    if (sit != _scope.end() && sit->second.kind == TamaScopeVal::STRING) {
        const std::string &s = sit->second.str;
        if (s == "abs") return 0; if (s == "rel") return 1; if (s == "seq") return 2;
    }
    return 0;
}

int _TamaInterpreter::_get_axis_type(_TamaASTNode *axis_node) const {
    if (axis_node->axis_type_var.empty()) return axis_node->axis_type;
    auto sit = _scope.find(axis_node->axis_type_var);
    if (sit != _scope.end() && sit->second.kind == TamaScopeVal::STRING) {
        const std::string &s = sit->second.str;
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
        const std::vector<std::string> &params,
        const std::vector<TamaArgVal> &args) const {
    TamaScope scope = _scope;
    int n = (int)std::min(params.size(), args.size());
    for (int i = 0; i < n; ++i)
        scope[params[i]] = TamaScopeVal(args[i]);
    return scope;
}
