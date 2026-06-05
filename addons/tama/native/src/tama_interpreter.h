#pragma once
#include "tama_expr.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
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
// Signal payload classes
// ---------------------------------------------------------------------------

class _TamaBulletFireData : public godot::RefCounted {
    GDCLASS(_TamaBulletFireData, godot::RefCounted)
protected:
    static void _bind_methods();
public:
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
    godot::String bullet_type;
    godot::Object *bullet_emitter_act = nullptr;
    godot::Object *bullet_act         = nullptr;
    godot::Array   bullet_params;    // Array[String]
    godot::Array   bullet_args;      // Array (float | String | _TamaRef | ...)
    bool           mvmt_x_set  = false;
    int            mvmt_x_type = 0;
    godot::String  mvmt_x_expr;
    bool           mvmt_y_set  = false;
    int            mvmt_y_type = 0;
    godot::String  mvmt_y_expr;
    godot::Object *source_program = nullptr;

    // Accessors (required by _bind_methods)
    int    get_dir_type()        const;
    float  get_dir_value()       const;
    int    get_speed_type()      const;
    float  get_speed_value()     const;
    int    get_offset_mode()     const;
    float  get_offset_value()    const;
    int    get_offset_x_type()   const;
    float  get_offset_x()        const;
    int    get_offset_y_type()   const;
    float  get_offset_y()        const;
    bool   get_has_pos()         const;
    bool   get_pos_x_set()       const;
    int    get_pos_x_type()      const;
    float  get_pos_x()           const;
    bool   get_pos_y_set()       const;
    int    get_pos_y_type()      const;
    float  get_pos_y()           const;
    godot::String  get_bullet_type()        const;
    godot::Object *get_bullet_emitter_act() const;
    godot::Object *get_bullet_act()         const;
    godot::Array   get_bullet_params()      const;
    godot::Array   get_bullet_args()        const;
    bool           get_mvmt_x_set()         const;
    int            get_mvmt_x_type()        const;
    godot::String  get_mvmt_x_expr()        const;
    bool           get_mvmt_y_set()         const;
    int            get_mvmt_y_type()        const;
    godot::String  get_mvmt_y_expr()        const;
    godot::Object *get_source_program()     const;
};

class _TamaChdirData : public godot::RefCounted {
    GDCLASS(_TamaChdirData, godot::RefCounted)
protected:
    static void _bind_methods();
public:
    int   dir_type  = 0;
    float dir_value = 0.0f;
    float over      = 0.0f;
    int   get_dir_type()  const;
    float get_dir_value() const;
    float get_over()      const;
};

class _TamaChspdData : public godot::RefCounted {
    GDCLASS(_TamaChspdData, godot::RefCounted)
protected:
    static void _bind_methods();
public:
    int   speed_type  = 0;
    float speed_value = 0.0f;
    float over        = 0.0f;
    int   get_speed_type()  const;
    float get_speed_value() const;
    float get_over()        const;
};

class _TamaChposData : public godot::RefCounted {
    GDCLASS(_TamaChposData, godot::RefCounted)
protected:
    static void _bind_methods();
public:
    bool  has_x  = false;
    int   x_type = 0;
    float x      = 0.0f;
    bool  has_y  = false;
    int   y_type = 0;
    float y      = 0.0f;
    float over   = 0.0f;
    bool  get_has_x()  const;
    int   get_x_type() const;
    float get_x()      const;
    bool  get_has_y()  const;
    int   get_y_type() const;
    float get_y()      const;
    float get_over()   const;
};

class _TamaAccelData : public godot::RefCounted {
    GDCLASS(_TamaAccelData, godot::RefCounted)
protected:
    static void _bind_methods();
public:
    bool  has_x  = false;
    int   x_type = 0;
    float x      = 0.0f;
    bool  has_y  = false;
    int   y_type = 0;
    float y      = 0.0f;
    float over   = 0.0f;
    bool  get_has_x()  const;
    int   get_x_type() const;
    float get_x()      const;
    bool  get_has_y()  const;
    int   get_y_type() const;
    float get_y()      const;
    float get_over()   const;
};

// First-class value stored in scope — wraps a definition name + pre-bound args.
class _TamaRef : public godot::RefCounted {
    GDCLASS(_TamaRef, godot::RefCounted)
protected:
    static void _bind_methods();
public:
    godot::String name;
    godot::Array  bound_args;
    godot::String get_ref_name()   const;
    godot::Array  get_bound_args() const;
};

// ---------------------------------------------------------------------------
// Execution frame for the state-machine interpreter
// ---------------------------------------------------------------------------

struct ExecFrame {
    enum class Kind { BODY, REPEAT_CTRL, WHILE_CTRL, REPEATF_CTRL };
    Kind kind = Kind::BODY;

    // -- BODY --
    godot::Array body;
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
    godot::Array  loop_body;
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

    godot::Object *_program = nullptr;
    godot::Object *_context = nullptr;
    bool           _running  = false;
    bool           _breaking = false;

    // Execution stack
    std::vector<ExecFrame> _exec_stack;

    // Flat scope shared across all frames in this execution context.
    // Saved/restored for isolated named-act scopes.
    godot::Dictionary _scope;
    std::vector<godot::Dictionary> _scope_saves;

    // Async children (act calls with is_async=true) — stepped in step()
    std::vector<_TamaInterpreter *> _async_children;

    // Definition lookup tables (built once from program)
    std::unordered_map<std::string, godot::Object *> _fires_map;
    std::unordered_map<std::string, godot::Object *> _acts_map;
    std::unordered_map<std::string, godot::Object *> _bullets_map;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    void _build_lookup_tables();

    // Push frames
    void _push_body(const godot::Array &body, bool sync_only = false, bool pops_scope = false);
    void _push_repeat_ctrl(const godot::Array &body, int n, const godot::String &idx_var);
    void _push_while_ctrl(const godot::Array &body, const godot::String &cond);
    void _push_repeatf_ctrl(const godot::Array &body, int n, const godot::String &idx_var);

    // Snapshot current scope keys into pre_keys for a frame
    std::vector<std::string> _snapshot_scope_keys() const;

    // Pop a BODY frame: restore scope to pre_keys, optionally pop scope save
    void _pop_body_frame();

    // Step the top frame
    void _step_body(ExecFrame &f, bool &yielded);
    void _step_loop_ctrl(ExecFrame &f, bool &yielded);

    // Execute a single node. Returns true if a new frame was pushed or suspension was set.
    void _exec_node(godot::Object *node, int type_id, bool sync_only, bool &yielded);

    // Fire node execution (InlineFireNode or FireDefNode)
    void _exec_fire_node(godot::Object *node);
    void _exec_fire_call(godot::Object *node);

    // Signal emission helpers
    void _emit_chdir(godot::Object *node);
    void _emit_chspd(godot::Object *node);
    void _emit_chpos(godot::Object *node);
    void _emit_accel(godot::Object *node);

    // Scope helpers
    godot::Dictionary _scope_snapshot_plus_params(
        const godot::Array &params, const godot::Array &args) const;

    // Arg/expr evaluation
    godot::Variant _eval_arg(const godot::Variant &arg);
    float          _eval_float(const godot::String &expr);
    float          _eval_arg_as_float(const godot::Variant &arg);

    // Qualifier resolution (reads dir_type_var / speed_type_var / axis_type_var from scope)
    int _get_dir_type(godot::Object *dir_node) const;
    int _get_speed_type(godot::Object *spd_node) const;
    int _get_axis_type(godot::Object *axis_node) const;

    // Definition lookups
    godot::Object *_find_fire(const std::string &name) const;
    godot::Object *_find_act(const std::string &name) const;
    godot::Object *_find_bullet(const std::string &name) const;

    // Run an async child act
    void _run_async_act(godot::Object *act_node, const godot::Dictionary &scope_copy);

    // Signal forwarders used to relay async-child signals to this interpreter's own signals
    void _fwd_bullet_fired(godot::Variant data)   { emit_signal("bullet_fired", data); }
    void _fwd_vanished()                           { emit_signal("vanished"); }
    void _fwd_changed_direction(godot::Variant d)  { emit_signal("changed_direction", d); }
    void _fwd_changed_speed(godot::Variant d)      { emit_signal("changed_speed", d); }
    void _fwd_changed_position(godot::Variant d)   { emit_signal("changed_position", d); }
    void _fwd_accelerated(godot::Variant d)        { emit_signal("accelerated", d); }

    // Populate mvmt fields on a BulletFireData
    void _populate_mvmt(_TamaBulletFireData *data, godot::Object *mvmt_node);

    // Sync body execution (for repeatf body — no suspension allowed)
    void _exec_body_sync(const godot::Array &body);

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

    void start(godot::Object *program, godot::Dictionary scope);
    void start_act(godot::Object *program, godot::Object *act, godot::Dictionary scope);
    void stop();
    bool is_running() const { return _running || !_exec_stack.empty() || !_async_children.empty(); }

    // Evaluate an expression with a given scope — used by TamaBullet for mvmt expressions.
    float eval_expr(const godot::String &expr, const godot::Dictionary &scope);

    // Property
    void           set_context(godot::Object *ctx) { _context = ctx; }
    godot::Object *get_context() const             { return _context; }

    // For pool: set the scene tree reference without being in the tree
    void set_tree_override(godot::Object *tree) { _tree_override = tree; }
    godot::Object *_tree_override = nullptr;
};
