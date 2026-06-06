#pragma once
#include "tama_expr.h"
#include "tama_ast_nodes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

// ---------------------------------------------------------------------------
// NodeType enum
// ---------------------------------------------------------------------------
enum class TamaNodeType : int {
    NONE = 0, PROGRAM = 1, EXPORT_VAR = 2, MAIN = 3,
    FIRE_DEF = 4, ACT_DEF = 5, BULLET_DEF = 6,
    DIR = 7, SPEED = 8, OFFSET = 9, POS = 10,
    OFFSET_INLINE = 11, OFFSET_AXIS = 12,
    BULLET_CALL = 13, REF_CALL_ARG = 14, INLINE_BULLET = 15,
    WAIT = 16, WAIT_FRAMES = 17, VANISH = 18, BREAK = 19,
    REPEAT = 20, CHDIR = 21, CHSPD = 22, CHPOS = 23, ACCEL = 24,
    REPEAT_FRAME = 25, MVMT = 26, OVER = 27,
    INLINE_ACT = 28, INLINE_FIRE = 29, FIRE_CALL = 30,
    WHILE = 31, IF = 32, VAR_DECL = 33, ACT_CALL = 34,
};

// ---------------------------------------------------------------------------
// Pure C++ event structs (no heap allocation, no Godot machinery)
// ---------------------------------------------------------------------------

struct TamaChdirEvent  { int dir_type; float dir_value; float over; };
struct TamaChspdEvent  { int speed_type; float speed_value; float over; };
struct TamaChposEvent  { bool has_x; int x_type; float x; bool has_y; int y_type; float y; float over; };
struct TamaAccelEvent  { bool has_x; int x_type; float x; bool has_y; int y_type; float y; float over; };

class TamaBulletEventHandler {
public:
    virtual void on_chdir    (const TamaChdirEvent  &) {}
    virtual void on_chspd    (const TamaChspdEvent  &) {}
    virtual void on_chpos    (const TamaChposEvent  &) {}
    virtual void on_accel    (const TamaAccelEvent  &) {}
    virtual void on_vanished ()                        {}
    virtual ~TamaBulletEventHandler() = default;
};

// ---------------------------------------------------------------------------
// Scope types
// ---------------------------------------------------------------------------

// Scope value — either a Godot Variant (numerics, strings, _TamaRef objects) or an AST node.
struct TamaScopeVal {
    bool is_node = false;
    godot::Variant var;
    _TamaASTNode *node = nullptr;
    std::shared_ptr<_TamaASTNode> _owner; // propagated from TamaArgVal when needed

    TamaScopeVal() = default;
    TamaScopeVal(const godot::Variant &v) : var(v) {}
    TamaScopeVal(_TamaASTNode *n) : is_node(true), node(n) {}
    TamaScopeVal(const TamaArgVal &av) {
        if (av.is_node) { is_node = true; node = av.node; _owner = av._owner; }
        else { var = av.var; }
    }
};
using TamaScope = std::unordered_map<std::string, TamaScopeVal>;

// ---------------------------------------------------------------------------
// Fire event data — plain C++ struct, stack-allocated per bullet fired
// ---------------------------------------------------------------------------

struct TamaBulletFireData {
    int    dir_type       = 0;   // DirType: AIM=0, ABS=1, REL=2, SEQ=3
    float  dir_value      = 0.0f;
    int    speed_type     = 0;   // ValueType: ABS=0, REL=1, SEQ=2
    float  speed_value    = 0.0f;
    int    offset_mode    = 0;   // OffsetMode: NONE=0, INLINE=1, BLOCK=2
    float  offset_value   = 0.0f;
    int    offset_x_type  = 1;   // REL
    float  offset_x       = 0.0f;
    int    offset_y_type  = 1;   // REL
    float  offset_y       = 0.0f;
    bool   has_pos        = false;
    bool   pos_x_set      = false;
    int    pos_x_type     = 0;   // ABS
    float  pos_x          = 0.0f;
    bool   pos_y_set      = false;
    int    pos_y_type     = 0;   // ABS
    float  pos_y          = 0.0f;
    godot::String   bullet_type;
    _TamaASTNode   *bullet_emitter_act = nullptr;
    _TamaASTNode   *bullet_act         = nullptr;
    std::vector<godot::String> bullet_params;
    std::vector<TamaArgVal>    bullet_args;
    bool            mvmt_x_set  = false;
    int             mvmt_x_type = 0;
    godot::String   mvmt_x_expr;
    bool            mvmt_y_set  = false;
    int             mvmt_y_type = 0;
    godot::String   mvmt_y_expr;
    _TamaASTNode   *source_program = nullptr;
};

// ---------------------------------------------------------------------------
// First-class value stored in scope — wraps a definition name + pre-bound args.
// ---------------------------------------------------------------------------
class _TamaRef : public godot::RefCounted {
    GDCLASS(_TamaRef, godot::RefCounted)
protected:
    static void _bind_methods();
public:
    godot::String name;
    std::vector<TamaArgVal> bound_args;
};

// ---------------------------------------------------------------------------
// Execution frame for the state-machine interpreter
// ---------------------------------------------------------------------------

struct ExecFrame {
    enum class Kind { BODY, REPEAT_CTRL, WHILE_CTRL, REPEATF_CTRL };
    Kind kind = Kind::BODY;

    // -- BODY --
    std::vector<_TamaASTNode*> body; // raw ptrs; kept alive by shared_ptr in AST
    int pc          = 0;
    bool sync_only  = false;  // if true, skip suspension-inducing nodes
    bool pops_scope = false;  // true if this frame should restore the scope save on pop

    // Pre-existing scope keys at frame entry — new keys are removed on pop.
    std::vector<std::string> pre_keys;

    // -- Suspension (BODY frames) --
    enum class Suspend { NONE, WAIT_TIME, WAIT_FRAMES };
    Suspend suspend   = Suspend::NONE;
    float   wait_secs = 0.0f;
    int     wait_frames = 0;

    // -- Loop control (REPEAT_CTRL / WHILE_CTRL / REPEATF_CTRL) --
    std::vector<_TamaASTNode*> loop_body;
    int           loop_n = -1;  // -1 = infinite
    int           loop_i = 0;
    godot::String loop_index_var;
    godot::String while_cond;
    bool          between_iters = false; // unused — kept for ABI stability
};

// ---------------------------------------------------------------------------
// _TamaInterpreter
// ---------------------------------------------------------------------------

class _TamaInterpreter : public godot::Node {
    GDCLASS(_TamaInterpreter, godot::Node)

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    _TamaASTNode           *_program       = nullptr;
    godot::Object         *_context       = nullptr;
    TamaBulletEventHandler *_event_handler = nullptr;
    bool           _running  = false;
    bool           _breaking = false;

    // Execution stack
    std::vector<ExecFrame> _exec_stack;

    // Flat scope shared across all frames in this execution context.
    // Saved/restored for isolated named-act scopes.
    TamaScope _scope;
    std::vector<TamaScope> _scope_saves;

    // Async children (act calls with is_async=true) — stepped in step()
    std::vector<_TamaInterpreter *> _async_children;

    // Definition lookup tables (built once from program)
    std::unordered_map<std::string, _TamaASTNode *> _fires_map;
    std::unordered_map<std::string, _TamaASTNode *> _acts_map;
    std::unordered_map<std::string, _TamaASTNode *> _bullets_map;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    void _build_lookup_tables();

    // Push frames
    void _push_body(const std::vector<std::shared_ptr<_TamaASTNode>> &body, bool sync_only = false, bool pops_scope = false);
    void _push_body(const std::vector<_TamaASTNode*> &body, bool sync_only = false, bool pops_scope = false);
    void _push_repeat_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body, int n, const godot::String &idx_var);
    void _push_while_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body, const godot::String &cond);
    void _push_repeatf_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body, int n, const godot::String &idx_var);

    // Snapshot current scope keys into pre_keys for a frame
    std::vector<std::string> _snapshot_scope_keys() const;

    // Pop a BODY frame: restore scope to pre_keys, optionally pop scope save
    void _pop_body_frame();

    // Step the top frame
    void _step_body(ExecFrame &f, bool &yielded);
    void _step_loop_ctrl(ExecFrame &f, bool &yielded);

    // Execute a single node.
    void _exec_node(_TamaASTNode *n, bool sync_only, bool &yielded);

    // Fire node execution (InlineFireNode or FireDefNode)
    void _exec_fire_node(_TamaASTNode *node);
    void _exec_fire_call(_TamaASTNode *n);

    // Signal emission helpers
    void _emit_chdir(_TamaASTNode *n);
    void _emit_chspd(_TamaASTNode *n);
    void _emit_chpos(_TamaASTNode *n);
    void _emit_accel(_TamaASTNode *n);

    // Scope helpers
    TamaScope _scope_snapshot_plus_params(
        const std::vector<godot::String> &params,
        const std::vector<TamaArgVal> &args) const;

    // Arg/expr evaluation
    TamaArgVal _eval_arg(const TamaArgVal &arg);
    float      _eval_float(const godot::String &expr);
    float      _eval_arg_float(const TamaArgVal &arg);

    // Qualifier resolution (reads dir_type_var / speed_type_var / axis_type_var from scope)
    int _get_dir_type(_TamaASTNode *dir_node) const;
    int _get_speed_type(_TamaASTNode *spd_node) const;
    int _get_axis_type(_TamaASTNode *axis_node) const;

    // Definition lookups
    _TamaASTNode *_find_fire(const std::string &name) const;
    _TamaASTNode *_find_act(const std::string &name) const;
    _TamaASTNode *_find_bullet(const std::string &name) const;

    // Run an async child act
    void _run_async_act(_TamaASTNode *act_node, TamaScope scope_copy);

    // Populate mvmt fields on fire data
    void _populate_mvmt(TamaBulletFireData *data, _TamaASTNode *mvmt_node);

    // Sync body execution (for repeatf body — no suspension allowed)
    void _exec_body_sync(const std::vector<std::shared_ptr<_TamaASTNode>> &body);
    void _exec_body_sync(const std::vector<_TamaASTNode*> &body);

protected:
    static void _bind_methods();

public:
    _TamaInterpreter() = default;
    ~_TamaInterpreter() override = default;

    // Godot virtuals
    void _ready()                        override;
    void _physics_process(double delta)  override;

    // Public API
    void step(double delta);

    void start(_TamaASTNode *program, TamaScope scope);
    void start_act(_TamaASTNode *program, _TamaASTNode *act, TamaScope scope);
    void stop();
    bool is_running() const { return _running || !_exec_stack.empty() || !_async_children.empty(); }

    // Evaluate an expression with a given scope — used by TamaBullet for mvmt expressions.
    float eval_expr(const godot::String &expr, const godot::Dictionary &scope);

    // C++ callbacks — set by spawn manager / emitter, no Godot signal overhead
    std::function<void(const TamaBulletFireData &)> _fire_cb;
    std::function<void()>                           _finished_cb;

    // Event handler (C++ only — no Godot signal overhead)
    void set_event_handler(TamaBulletEventHandler *h) { _event_handler = h; }

    // Property
    void           set_context(godot::Object *ctx) { _context = ctx; }
    godot::Object *get_context() const             { return _context; }

    // For pool: set the scene tree reference without being in the tree
    void set_tree_override(godot::Object *tree) { _tree_override = tree; }
    godot::Object *_tree_override = nullptr;
};
