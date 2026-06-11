#pragma once
#include "tama_expr.h"
#include "tama_ast_nodes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/object.hpp>
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
    CHROTSPD = 35, DELAY = 36, CONTEXT_CALL = 37,
};

// ---------------------------------------------------------------------------
// Pure C++ event structs (no heap allocation, no Godot machinery)
// ---------------------------------------------------------------------------

struct TamaChdirEvent    { int dir_type; float dir_value; float over; };
struct TamaChspdEvent    { int speed_type; float speed_value; float over; };
struct TamaChrotspdEvent { int speed_type; float speed_value; float over; };
struct TamaChposEvent    { bool has_x; int x_type; float x; bool has_y; int y_type; float y; float over; };
struct TamaAccelEvent    { bool has_x; int x_type; float x; bool has_y; int y_type; float y; float over; };

class TamaBulletEventHandler {
public:
    virtual void on_chdir    (const TamaChdirEvent    &) {}
    virtual void on_chspd    (const TamaChspdEvent    &) {}
    virtual void on_chrotspd (const TamaChrotspdEvent &) {}
    virtual void on_chpos    (const TamaChposEvent    &) {}
    virtual void on_accel    (const TamaAccelEvent    &) {}
    virtual void on_vanished ()                          {}
    virtual ~TamaBulletEventHandler() = default;
};

// ---------------------------------------------------------------------------
// Scope types — native tagged union, no Godot::Variant boxing for floats
// ---------------------------------------------------------------------------

struct TamaScopeVal {
    enum Kind : uint8_t { FLOAT = 0, STRING = 1, NODE = 2, REF = 3 } kind = FLOAT;
    float f = 0.0f;
    std::string str;
    _TamaASTNode *node = nullptr;
    std::shared_ptr<_TamaASTNode> _owner;
    std::shared_ptr<TamaRef> ref;

    TamaScopeVal() = default;
    explicit TamaScopeVal(float v) : kind(FLOAT), f(v) {}
    TamaScopeVal(_TamaASTNode *n) : kind(NODE), node(n) {}

    TamaScopeVal(const godot::Variant &v) {
        switch (v.get_type()) {
            case godot::Variant::FLOAT:  kind = FLOAT;  f = (float)(double)v;               break;
            case godot::Variant::INT:    kind = FLOAT;  f = (float)(int64_t)v;              break;
            case godot::Variant::BOOL:   kind = FLOAT;  f = (bool)v ? 1.0f : 0.0f;         break;
            case godot::Variant::STRING: kind = STRING; str = ((godot::String)v).utf8().get_data(); break;
            default:                     kind = FLOAT;  f = 0.0f;                           break;
        }
    }

    TamaScopeVal(const TamaArgVal &av) {
        if (av.ref) { kind = REF; ref = av.ref; }
        else if (av.is_node) { kind = NODE; node = av.node; _owner = av._owner; }
        else { *this = TamaScopeVal(av.var); }
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
    std::string     bullet_type;
    _TamaASTNode   *bullet_emitter_act = nullptr;
    _TamaASTNode   *bullet_act         = nullptr;
    std::vector<std::string>  bullet_params;
    std::vector<TamaArgVal>   bullet_args;
    bool            mvmt_x_set  = false;
    int             mvmt_x_type = 0;
    std::string     mvmt_x_expr;
    bool            mvmt_y_set  = false;
    int             mvmt_y_type = 0;
    std::string     mvmt_y_expr;
    bool            has_rot_speed   = false;
    int             rot_speed_type  = 0;    // ABS=0, REL=1, SEQ=2
    float           rot_speed_value = 0.0f;
    int             bounces_max  = 0;  // 0=none, -1=infinite, N=N bounces
    int             bounces_axis = 0;  // 0=both, 1=x (left/right), 2=y (top/bottom)
    int             spawn_delay_override = -1; // -1 = use TypeData default; >= 0 overrides it
    _TamaASTNode   *source_program = nullptr;
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
    bool sync_only  = false;
    bool pops_scope = false;

    // Pre-existing scope keys at frame entry — new keys are removed on pop.
    std::vector<std::string> pre_keys;

    // -- Suspension (BODY frames) --
    enum class Suspend { NONE, WAIT_TIME, WAIT_FRAMES };
    Suspend suspend   = Suspend::NONE;
    float   wait_secs = 0.0f;
    int     wait_frames = 0;

    // -- Loop control (REPEAT_CTRL / WHILE_CTRL / REPEATF_CTRL) --
    std::vector<_TamaASTNode*> loop_body;
    int         loop_n = -1;  // -1 = infinite
    int         loop_i = 0;
    std::string loop_index_var;
    std::string while_cond;
    bool        between_iters = false;
};

// ---------------------------------------------------------------------------
// _TamaInterpreter — plain C++ class (no Node, no GDCLASS)
// ---------------------------------------------------------------------------

class _TamaInterpreter {
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
    TamaScope _scope;
    std::vector<TamaScope> _scope_saves;

    // Async children (act calls with is_async=true) — stepped in step()
    std::vector<std::unique_ptr<_TamaInterpreter>> _async_children;

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
    void _push_repeat_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body, int n, const std::string &idx_var);
    void _push_while_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body, const std::string &cond);
    void _push_repeatf_ctrl(const std::vector<std::shared_ptr<_TamaASTNode>> &body, int n, const std::string &idx_var);

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

    // Context call execution
    void _exec_context_call(_TamaASTNode *n);

    // Signal emission helpers
    void _emit_chdir    (_TamaASTNode *n);
    void _emit_chspd    (_TamaASTNode *n);
    void _emit_chrotspd (_TamaASTNode *n);
    void _emit_chpos    (_TamaASTNode *n);
    void _emit_accel    (_TamaASTNode *n);

    // Scope helpers
    TamaScope _scope_snapshot_plus_params(
        const std::vector<std::string> &params,
        const std::vector<TamaArgVal> &args) const;

    // Arg/expr evaluation
    TamaArgVal _eval_arg(const TamaArgVal &arg);
    float      _eval_float(const std::string &expr);
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

public:
    _TamaInterpreter() = default;
    ~_TamaInterpreter() = default;

    // Public API
    void step(double delta);

    void start(_TamaASTNode *program, TamaScope scope);
    void start_act(_TamaASTNode *program, _TamaASTNode *act, TamaScope scope);
    void stop();
    bool is_running() const { return _running || !_exec_stack.empty() || !_async_children.empty(); }

    // Evaluate an expression using an external var set (for mvmt expressions).
    // Does not touch _scope. Uses _context for context functions.
    float eval_expr_native(const std::string &expr,
                           const std::vector<std::string> &var_names,
                           const std::vector<double> &var_values);

    // C++ callbacks — set by spawn manager / emitter, no Godot signal overhead
    std::function<void(const TamaBulletFireData &)> _fire_cb;
    std::function<void()>                           _finished_cb;

    // Event handler (C++ only — no Godot signal overhead)
    void set_event_handler(TamaBulletEventHandler *h) { _event_handler = h; }

    // Property
    void           set_context(godot::Object *ctx) { _context = ctx; }
    godot::Object *get_context() const             { return _context; }
};
