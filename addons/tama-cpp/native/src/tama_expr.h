#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

// ---------------------------------------------------------------------------
// Bytecode
// ---------------------------------------------------------------------------

enum class TamaOp : uint8_t {
    PUSH_CONST,     // const_val: double
    PUSH_VAR,       // slot:      uint8_t
    NEG, NOT,
    ADD, SUB, MUL, DIV, MOD,
    EQ, NEQ, LT, GT, LEQ, GEQ,
    AND, OR,
    CALL_BUILTIN,   // builtin.id, builtin.argc
    CALL_CONTEXT,   // ctx_fn.name_idx, ctx_fn.argc
};

struct TamaInstr {
    TamaOp op;
    union {
        double  const_val;
        uint8_t slot;
        struct { uint8_t id;       uint8_t argc; } builtin;
        struct { uint8_t name_idx; uint8_t argc; } ctx_fn;
    };
    explicit TamaInstr(TamaOp o) : op(o), const_val(0.0) {}
};

// Compiled form of a single expression string + variable binding signature.
struct TamaExprChunk {
    std::vector<TamaInstr>   code;
    std::vector<std::string> ctx_fn_names; // indexed by CALL_CONTEXT.name_idx
};

// ---------------------------------------------------------------------------
// GDExtension singleton class
// ---------------------------------------------------------------------------

class TamaExprRuntime : public godot::Object {
    GDCLASS(TamaExprRuntime, godot::Object)

    static TamaExprRuntime *_singleton;

    // cache key: expr_string + "|" + comma-joined var names
    std::unordered_map<std::string, TamaExprChunk> _cache;

    // user-registered context functions (name → Callable)
    std::unordered_map<std::string, godot::Callable> _user_fns;

    // --- compiler internals ---
    struct CompileState {
        const std::string                  &src;
        const std::vector<std::string>     &var_names;
        std::unordered_map<std::string, uint8_t> var_slots; // name → index

        size_t pos = 0;
        TamaExprChunk result;

        CompileState(const std::string &s, const std::vector<std::string> &v)
            : src(s), var_names(v) {
            for (size_t i = 0; i < v.size() && i < 255; ++i)
                var_slots[v[i]] = (uint8_t)i;
        }

        char peek(size_t offset = 0) const {
            size_t i = pos + offset;
            return i < src.size() ? src[i] : '\0';
        }
        void skip_ws() {
            while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t')) ++pos;
        }
        bool at_end() const { return pos >= src.size(); }
    };

    static TamaExprChunk _compile(const std::string &expr, const std::vector<std::string> &var_names);
    static bool _compile_expr(CompileState &cs);   // or-level
    static bool _compile_and(CompileState &cs);
    static bool _compile_cmp(CompileState &cs);
    static bool _compile_add(CompileState &cs);
    static bool _compile_mul(CompileState &cs);
    static bool _compile_unary(CompileState &cs);
    static bool _compile_primary(CompileState &cs);
    static bool _compile_call(CompileState &cs, const std::string &name);

    static double _eval_chunk(const TamaExprChunk &chunk,
                              const double *values, size_t count,
                              godot::Object *context,
                              const std::unordered_map<std::string, godot::Callable> &user_fns);

    const TamaExprChunk &_get_chunk(const std::string &expr,
                                    const std::vector<std::string> &var_names,
                                    const std::string &cache_key);

protected:
    static void _bind_methods();

public:
    static TamaExprRuntime *get_singleton() { return _singleton; }

    TamaExprRuntime();
    ~TamaExprRuntime();

    // Evaluate an expression string.
    // expr:       expression string
    // var_names:  variable names (parallel with var_values)
    // var_values: variable float values
    // context:    TamaContext object (for context functions like time(), get_1_in())
    double eval(const godot::String &expr,
                const godot::PackedStringArray &var_names,
                const godot::PackedFloat64Array &var_values,
                godot::Object *context);

    // Register / unregister a user-defined context function.
    void register_function(const godot::String &name, const godot::Callable &callable);
    void unregister_function(const godot::String &name);

    // Flush the compiled chunk cache (call after script hot-reload).
    void clear_cache();

    // Internal: get or compile a chunk by expression string + variable names.
    // Returns a stable pointer (valid until clear_cache() is called).
    const TamaExprChunk *get_chunk(const std::string &expr,
                                   const std::vector<std::string> &var_names,
                                   const std::string &cache_key);

    // Evaluate a pre-compiled chunk directly.
    double eval_chunk(const TamaExprChunk &chunk,
                      const double *values, size_t count,
                      godot::Object *context = nullptr) const;
};
