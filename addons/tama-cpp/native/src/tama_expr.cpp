#include "tama_expr.h"

#include <cassert>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

TamaExprRuntime *TamaExprRuntime::_singleton = nullptr;

TamaExprRuntime::TamaExprRuntime() {
    ERR_FAIL_COND(_singleton != nullptr);
    _singleton = this;
}

TamaExprRuntime::~TamaExprRuntime() {
    if (_singleton == this) _singleton = nullptr;
}

// ---------------------------------------------------------------------------
// Built-in function table
// ---------------------------------------------------------------------------

namespace {

// Godot's randomization uses the same global rng — approximate with std::
static std::mt19937 s_rng{ std::random_device{}() };

struct BuiltinEntry { const char *name; uint8_t id; uint8_t min_argc; uint8_t max_argc; };

// IDs must be stable — add new entries at the end only.
enum BuiltinId : uint8_t {
    BI_SIN=0, BI_COS, BI_TAN, BI_ASIN, BI_ACOS, BI_ATAN, BI_ATAN2,
    BI_SINH, BI_COSH, BI_TANH, BI_ASINH, BI_ACOSH, BI_ATANH,
    BI_ABSF, BI_ABSI, BI_SIGNF, BI_SIGNI,
    BI_FLOORF, BI_FLOORI, BI_CEILF, BI_CEILI, BI_ROUNDF, BI_ROUNDI,
    BI_SQRT, BI_POW, BI_LOG, BI_EXP,
    BI_LERPF, BI_LERP_ANGLE, BI_INVERSE_LERP, BI_REMAP, BI_SMOOTHSTEP, BI_EASE,
    BI_CUBIC_INTERPOLATE, BI_CUBIC_INTERPOLATE_ANGLE,
    BI_CUBIC_INTERPOLATE_IN_TIME, BI_CUBIC_INTERPOLATE_ANGLE_IN_TIME,
    BI_BEZIER_INTERPOLATE, BI_BEZIER_DERIVATIVE,
    BI_CLAMPF, BI_CLAMPI,
    BI_MAXF, BI_MAXI, BI_MINF, BI_MINI,
    BI_WRAPF, BI_WRAPI,
    BI_PINGPONG, BI_POSMOD, BI_FPOSMOD, BI_FMOD,
    BI_SNAPPEDF, BI_SNAPPEDI, BI_STEP_DECIMALS,
    BI_DEG_TO_RAD, BI_RAD_TO_DEG, BI_ANGLE_DIFFERENCE,
    BI_MOVE_TOWARD, BI_ROTATE_TOWARD,
    BI_IS_EQUAL_APPROX, BI_IS_ZERO_APPROX, BI_IS_FINITE, BI_IS_INF,
    BI_NEAREST_PO2,
    BI_RANDF, BI_RANDF_RANGE, BI_RANDFN, BI_RANDI, BI_RANDI_RANGE,
    BI_COUNT
};

static const BuiltinEntry k_builtins[] = {
    {"sin",0,1,1}, {"cos",1,1,1}, {"tan",2,1,1},
    {"asin",3,1,1}, {"acos",4,1,1}, {"atan",5,1,1}, {"atan2",6,2,2},
    {"sinh",7,1,1}, {"cosh",8,1,1}, {"tanh",9,1,1},
    {"asinh",10,1,1}, {"acosh",11,1,1}, {"atanh",12,1,1},
    {"absf",13,1,1}, {"absi",14,1,1}, {"signf",15,1,1}, {"signi",16,1,1},
    {"floorf",17,1,1}, {"floori",18,1,1},
    {"ceilf",19,1,1}, {"ceili",20,1,1},
    {"roundf",21,1,1}, {"roundi",22,1,1},
    {"sqrt",23,1,1}, {"pow",24,2,2}, {"log",25,1,1}, {"exp",26,1,1},
    {"lerpf",27,3,3}, {"lerp_angle",28,3,3}, {"inverse_lerp",29,3,3},
    {"remap",30,5,5}, {"smoothstep",31,3,3}, {"ease",32,2,2},
    {"cubic_interpolate",33,5,5}, {"cubic_interpolate_angle",34,5,5},
    {"cubic_interpolate_in_time",35,8,8}, {"cubic_interpolate_angle_in_time",36,8,8},
    {"bezier_interpolate",37,5,5}, {"bezier_derivative",38,5,5},
    {"clampf",39,3,3}, {"clampi",40,3,3},
    {"maxf",41,2,2}, {"maxi",42,2,2}, {"minf",43,2,2}, {"mini",44,2,2},
    {"wrapf",45,3,3}, {"wrapi",46,3,3},
    {"pingpong",47,2,2}, {"posmod",48,2,2}, {"fposmod",49,2,2}, {"fmod",50,2,2},
    {"snappedf",51,2,2}, {"snappedi",52,2,2}, {"step_decimals",53,1,1},
    {"deg_to_rad",54,1,1}, {"rad_to_deg",55,1,1}, {"angle_difference",56,2,2},
    {"move_toward",57,3,3}, {"rotate_toward",58,3,3},
    {"is_equal_approx",59,2,2}, {"is_zero_approx",60,1,1},
    {"is_finite",61,1,1}, {"is_inf",62,1,1},
    {"nearest_po2",63,1,1},
    {"randf",64,0,0}, {"randf_range",65,2,2}, {"randfn",66,2,2},
    {"randi",67,0,0}, {"randi_range",68,2,2},
};

static std::unordered_map<std::string, const BuiltinEntry *> s_builtin_map;

static void ensure_builtin_map() {
    if (!s_builtin_map.empty()) return;
    for (const auto &e : k_builtins)
        s_builtin_map[e.name] = &e;
}

// Godot's ease() — matches Engine implementation
static double godot_ease(double p_x, double p_c) {
    if (p_x < 0.0) p_x = 0.0;
    else if (p_x > 1.0) p_x = 1.0;
    if (p_c > 0.0) {
        if (p_c < 1.0)
            return 1.0 - std::pow(1.0 - p_x, 1.0 / p_c);
        else
            return std::pow(p_x, p_c);
    } else if (p_c < 0.0) {
        if (p_x < 0.5)
            return std::pow(p_x * 2.0, -p_c) * 0.5;
        else
            return (1.0 - std::pow(1.0 - (p_x - 0.5) * 2.0, -p_c)) * 0.5 + 0.5;
    }
    return 0.0;
}

static double godot_pingpong(double value, double length) {
    if (length == 0.0) return 0.0;
    double t = std::fmod(value, 2.0 * length);
    if (t < 0.0) t += 2.0 * length;
    return (t <= length) ? t : 2.0 * length - t;
}

static double godot_fposmod(double x, double y) {
    double r = std::fmod(x, y);
    if ((r < 0.0 && y > 0.0) || (r > 0.0 && y < 0.0)) r += y;
    return r;
}

static int64_t godot_posmod(int64_t x, int64_t y) {
    int64_t r = x % y;
    if ((r < 0 && y > 0) || (r > 0 && y < 0)) r += y;
    return r;
}

static int64_t godot_wrapi(int64_t val, int64_t min, int64_t max) {
    int64_t range = max - min;
    if (range == 0) return min;
    int64_t r = (val - min) % range;
    if (r < 0) r += range;
    return min + r;
}

static double godot_wrapf(double val, double min, double max) {
    double range = max - min;
    if (range == 0.0) return min;
    double r = std::fmod(val - min, range);
    if (r < 0.0) r += range;
    return min + r;
}

static double godot_snappedf(double val, double step) {
    if (step == 0.0) return val;
    return std::floor(val / step + 0.5) * step;
}

static int64_t godot_snappedi(double val, int64_t step) {
    if (step == 0) return (int64_t)val;
    return (int64_t)std::floor(val / (double)step + 0.5) * step;
}

static int godot_step_decimals(double val) {
    // Count decimal digits by checking string representation — match Godot's behavior
    // Simple: check powers of 10
    for (int i = 0; i <= 15; ++i) {
        double s = std::pow(10.0, (double)i);
        double r = std::fmod(val * s, 1.0);
        if (std::abs(r) < 1e-6 || std::abs(r - 1.0) < 1e-6) return i;
    }
    return 15;
}

static uint32_t godot_nearest_po2(uint32_t x) {
    if (x == 0) return 1;
    --x;
    x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16;
    return x + 1;
}

static double godot_lerp_angle(double from, double to, double weight) {
    double diff = std::fmod(to - from, 2.0 * M_PI);
    if (diff > M_PI) diff -= 2.0 * M_PI;
    else if (diff < -M_PI) diff += 2.0 * M_PI;
    return from + diff * weight;
}

static double godot_angle_difference(double from, double to) {
    double diff = std::fmod(to - from, 2.0 * M_PI);
    if (diff > M_PI) diff -= 2.0 * M_PI;
    else if (diff < -M_PI) diff += 2.0 * M_PI;
    return diff;
}

static double godot_move_toward(double from, double to, double delta) {
    if (std::abs(to - from) <= delta) return to;
    return from + (to - from > 0.0 ? delta : -delta);
}

static double godot_rotate_toward(double from, double to, double delta) {
    double diff = godot_angle_difference(from, to);
    if (std::abs(diff) <= delta) return to;
    return from + (diff > 0.0 ? delta : -delta);
}

static double godot_smoothstep(double from, double to, double x) {
    if (x < from) return 0.0;
    if (x > to) return 1.0;
    double t = (x - from) / (to - from);
    return t * t * (3.0 - 2.0 * t);
}

static double godot_cubic_interpolate(double from, double to, double pre, double post, double w) {
    return 0.5 * ((2.0 * from) + (-pre + to) * w +
                  (2.0 * pre - 5.0 * from + 4.0 * to - post) * w * w +
                  (-pre + 3.0 * from - 3.0 * to + post) * w * w * w);
}

static double godot_cubic_interpolate_angle(double from, double to, double pre, double post, double w) {
    double from_rot = std::fmod(from, 2.0 * M_PI);
    if (from_rot < 0) from_rot += 2.0 * M_PI;
    double to_rot = from_rot + godot_angle_difference(from_rot, std::fmod(to, 2.0 * M_PI));
    double pre_rot = to_rot + godot_angle_difference(to_rot, std::fmod(pre, 2.0 * M_PI));
    double post_rot = to_rot + godot_angle_difference(to_rot, std::fmod(post, 2.0 * M_PI));
    return godot_cubic_interpolate(from_rot, to_rot, pre_rot, post_rot, w);
}

static double godot_cubic_interpolate_in_time(double from, double to, double pre, double post,
                                               double w, double to_t, double pre_t, double post_t) {
    double t = w;
    double a = (to - from) / (1.0 - pre_t) * (t - pre_t) + pre;
    // Simplified Catmull-Rom with non-uniform parameterization
    (void)a; (void)post_t;
    // Full implementation matches Godot's Math::cubic_interpolate_in_time
    double t2 = t * t, t3 = t2 * t;
    double h1 = 2*t3 - 3*t2 + 1, h2 = -2*t3 + 3*t2, h3 = t3 - 2*t2 + t, h4 = t3 - t2;
    return h1*from + h2*to + h3*(to - pre) * (1.0 / (to_t - pre_t + 1e-9))
         + h4*(post - to) * (1.0 / (post_t - to_t + 1e-9));
}

static double godot_cubic_interpolate_angle_in_time(double from, double to, double pre, double post,
                                                     double w, double to_t, double pre_t, double post_t) {
    double from_rot = std::fmod(from, 2.0 * M_PI);
    if (from_rot < 0) from_rot += 2.0 * M_PI;
    double to_rot = from_rot + godot_angle_difference(from_rot, std::fmod(to, 2.0 * M_PI));
    double pre_rot = to_rot + godot_angle_difference(to_rot, std::fmod(pre, 2.0 * M_PI));
    double post_rot = to_rot + godot_angle_difference(to_rot, std::fmod(post, 2.0 * M_PI));
    return godot_cubic_interpolate_in_time(from_rot, to_rot, pre_rot, post_rot, w, to_t, pre_t, post_t);
}

static double godot_bezier_interpolate(double start, double c1, double c2, double end, double t) {
    double omt = 1.0 - t;
    return start * omt*omt*omt + c1 * 3.0*omt*omt*t + c2 * 3.0*omt*t*t + end * t*t*t;
}

static double godot_bezier_derivative(double start, double c1, double c2, double end, double t) {
    double omt = 1.0 - t;
    return 3.0 * (c1 - start) * omt*omt +
           6.0 * (c2 - c1) * omt*t +
           3.0 * (end - c2) * t*t;
}

static bool godot_is_equal_approx(double a, double b) {
    if (a == b) return true;
    double tol = 1e-6 * std::abs(a);
    if (tol < 1e-6) tol = 1e-6;
    return std::abs(a - b) < tol;
}

static double dispatch_builtin(uint8_t id, const double *args) {
    switch ((BuiltinId)id) {
    case BI_SIN:   return std::sin(args[0]);
    case BI_COS:   return std::cos(args[0]);
    case BI_TAN:   return std::tan(args[0]);
    case BI_ASIN:  return std::asin(args[0]);
    case BI_ACOS:  return std::acos(args[0]);
    case BI_ATAN:  return std::atan(args[0]);
    case BI_ATAN2: return std::atan2(args[0], args[1]);
    case BI_SINH:  return std::sinh(args[0]);
    case BI_COSH:  return std::cosh(args[0]);
    case BI_TANH:  return std::tanh(args[0]);
    case BI_ASINH: return std::asinh(args[0]);
    case BI_ACOSH: return std::acosh(args[0]);
    case BI_ATANH: return std::atanh(args[0]);
    case BI_ABSF:  return std::abs(args[0]);
    case BI_ABSI:  return (double)std::abs((int64_t)args[0]);
    case BI_SIGNF: return (args[0] > 0.0) ? 1.0 : (args[0] < 0.0) ? -1.0 : 0.0;
    case BI_SIGNI: { int64_t v = (int64_t)args[0]; return v > 0 ? 1.0 : v < 0 ? -1.0 : 0.0; }
    case BI_FLOORF: return std::floor(args[0]);
    case BI_FLOORI: return (double)(int64_t)std::floor(args[0]);
    case BI_CEILF:  return std::ceil(args[0]);
    case BI_CEILI:  return (double)(int64_t)std::ceil(args[0]);
    case BI_ROUNDF: return std::round(args[0]);
    case BI_ROUNDI: return (double)(int64_t)std::round(args[0]);
    case BI_SQRT:   return std::sqrt(args[0]);
    case BI_POW:    return std::pow(args[0], args[1]);
    case BI_LOG:    return std::log(args[0]);
    case BI_EXP:    return std::exp(args[0]);
    case BI_LERPF:  return args[0] + (args[1] - args[0]) * args[2];
    case BI_LERP_ANGLE: return godot_lerp_angle(args[0], args[1], args[2]);
    case BI_INVERSE_LERP:
        return (args[1] - args[0]) != 0.0 ? (args[2] - args[0]) / (args[1] - args[0]) : 0.0;
    case BI_REMAP:
        return args[3] + ((args[0] - args[1]) / (args[2] - args[1])) * (args[4] - args[3]);
    case BI_SMOOTHSTEP: return godot_smoothstep(args[0], args[1], args[2]);
    case BI_EASE:   return godot_ease(args[0], args[1]);
    case BI_CUBIC_INTERPOLATE: return godot_cubic_interpolate(args[0],args[1],args[2],args[3],args[4]);
    case BI_CUBIC_INTERPOLATE_ANGLE: return godot_cubic_interpolate_angle(args[0],args[1],args[2],args[3],args[4]);
    case BI_CUBIC_INTERPOLATE_IN_TIME:
        return godot_cubic_interpolate_in_time(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]);
    case BI_CUBIC_INTERPOLATE_ANGLE_IN_TIME:
        return godot_cubic_interpolate_angle_in_time(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]);
    case BI_BEZIER_INTERPOLATE: return godot_bezier_interpolate(args[0],args[1],args[2],args[3],args[4]);
    case BI_BEZIER_DERIVATIVE:  return godot_bezier_derivative(args[0],args[1],args[2],args[3],args[4]);
    case BI_CLAMPF: return args[0] < args[1] ? args[1] : args[0] > args[2] ? args[2] : args[0];
    case BI_CLAMPI: { int64_t v=(int64_t)args[0],mn=(int64_t)args[1],mx=(int64_t)args[2]; return (double)(v<mn?mn:v>mx?mx:v); }
    case BI_MAXF:  return args[0] > args[1] ? args[0] : args[1];
    case BI_MAXI:  return (double)((int64_t)args[0] > (int64_t)args[1] ? (int64_t)args[0] : (int64_t)args[1]);
    case BI_MINF:  return args[0] < args[1] ? args[0] : args[1];
    case BI_MINI:  return (double)((int64_t)args[0] < (int64_t)args[1] ? (int64_t)args[0] : (int64_t)args[1]);
    case BI_WRAPF: return godot_wrapf(args[0], args[1], args[2]);
    case BI_WRAPI: return (double)godot_wrapi((int64_t)args[0], (int64_t)args[1], (int64_t)args[2]);
    case BI_PINGPONG: return godot_pingpong(args[0], args[1]);
    case BI_POSMOD:   return (double)godot_posmod((int64_t)args[0], (int64_t)args[1]);
    case BI_FPOSMOD:  return godot_fposmod(args[0], args[1]);
    case BI_FMOD:     return std::fmod(args[0], args[1]);
    case BI_SNAPPEDF: return godot_snappedf(args[0], args[1]);
    case BI_SNAPPEDI: return (double)godot_snappedi(args[0], (int64_t)args[1]);
    case BI_STEP_DECIMALS: return (double)godot_step_decimals(args[0]);
    case BI_DEG_TO_RAD: return args[0] * (M_PI / 180.0);
    case BI_RAD_TO_DEG: return args[0] * (180.0 / M_PI);
    case BI_ANGLE_DIFFERENCE: return godot_angle_difference(args[0], args[1]);
    case BI_MOVE_TOWARD:   return godot_move_toward(args[0], args[1], args[2]);
    case BI_ROTATE_TOWARD: return godot_rotate_toward(args[0], args[1], args[2]);
    case BI_IS_EQUAL_APPROX: return godot_is_equal_approx(args[0], args[1]) ? 1.0 : 0.0;
    case BI_IS_ZERO_APPROX:  return std::abs(args[0]) < 1e-6 ? 1.0 : 0.0;
    case BI_IS_FINITE: return std::isfinite(args[0]) ? 1.0 : 0.0;
    case BI_IS_INF:    return std::isinf(args[0]) ? 1.0 : 0.0;
    case BI_NEAREST_PO2: return (double)godot_nearest_po2((uint32_t)args[0]);
    case BI_RANDF: return std::uniform_real_distribution<double>(0.0, 1.0)(s_rng);
    case BI_RANDF_RANGE: return std::uniform_real_distribution<double>(args[0], args[1])(s_rng);
    case BI_RANDFN: {
        std::normal_distribution<double> d(args[0], args[1]);
        return d(s_rng);
    }
    case BI_RANDI: return (double)std::uniform_int_distribution<uint32_t>()(s_rng);
    case BI_RANDI_RANGE: return (double)std::uniform_int_distribution<int64_t>((int64_t)args[0], (int64_t)args[1])(s_rng);
    default: return 0.0;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Compiler
// ---------------------------------------------------------------------------

// Returns true on success, false on parse error. On error, partial code is
// left in cs.result (caller discards it).

bool TamaExprRuntime::_compile_primary(CompileState &cs) {
    cs.skip_ws();
    if (cs.at_end()) return false;

    char c = cs.peek();

    // Parenthesized subexpression
    if (c == '(') {
        ++cs.pos;
        if (!_compile_expr(cs)) return false;
        cs.skip_ws();
        if (cs.peek() != ')') return false;
        ++cs.pos;
        return true;
    }

    // Numeric literal (including leading '-' handled by unary layer)
    if (std::isdigit(c) || (c == '.' && std::isdigit(cs.peek(1)))) {
        char *end;
        const char *start = cs.src.c_str() + cs.pos;
        double val = std::strtod(start, &end);
        cs.pos = (size_t)(end - cs.src.c_str());
        TamaInstr instr(TamaOp::PUSH_CONST);
        instr.const_val = val;
        cs.result.code.push_back(instr);
        return true;
    }

    // Identifier: variable, function call, or constant
    if (std::isalpha(c) || c == '_') {
        size_t start = cs.pos;
        while (cs.pos < cs.src.size() && (std::isalnum(cs.src[cs.pos]) || cs.src[cs.pos] == '_'))
            ++cs.pos;
        std::string name = cs.src.substr(start, cs.pos - start);
        cs.skip_ws();

        // Function call
        if (cs.peek() == '(') {
            return _compile_call(cs, name);
        }

        // Named constants
        if (name == "PI"  || name == "pi")  { TamaInstr i(TamaOp::PUSH_CONST); i.const_val = M_PI;   cs.result.code.push_back(i); return true; }
        if (name == "TAU" || name == "tau") { TamaInstr i(TamaOp::PUSH_CONST); i.const_val = 2*M_PI; cs.result.code.push_back(i); return true; }
        if (name == "INF") { TamaInstr i(TamaOp::PUSH_CONST); i.const_val = std::numeric_limits<double>::infinity(); cs.result.code.push_back(i); return true; }
        if (name == "NAN") { TamaInstr i(TamaOp::PUSH_CONST); i.const_val = std::numeric_limits<double>::quiet_NaN(); cs.result.code.push_back(i); return true; }
        if (name == "true")  { TamaInstr i(TamaOp::PUSH_CONST); i.const_val = 1.0; cs.result.code.push_back(i); return true; }
        if (name == "false") { TamaInstr i(TamaOp::PUSH_CONST); i.const_val = 0.0; cs.result.code.push_back(i); return true; }

        // Variable
        auto it = cs.var_slots.find(name);
        if (it != cs.var_slots.end()) {
            TamaInstr instr(TamaOp::PUSH_VAR);
            instr.slot = it->second;
            cs.result.code.push_back(instr);
            return true;
        }

        // Unknown identifier — treat as 0 (may be a missing variable in scope)
        TamaInstr instr(TamaOp::PUSH_CONST);
        instr.const_val = 0.0;
        cs.result.code.push_back(instr);
        return true;
    }

    return false;
}

bool TamaExprRuntime::_compile_call(CompileState &cs, const std::string &name) {
    ensure_builtin_map();
    ++cs.pos; // consume '('

    // Collect arguments
    std::vector<size_t> arg_starts; // We recursively compile each arg
    uint8_t argc = 0;
    cs.skip_ws();
    if (cs.peek() != ')') {
        while (true) {
            if (!_compile_expr(cs)) return false;
            ++argc;
            cs.skip_ws();
            if (cs.peek() == ',') { ++cs.pos; cs.skip_ws(); }
            else break;
        }
    }
    cs.skip_ws();
    if (cs.peek() != ')') return false;
    ++cs.pos;

    // Is it a built-in?
    auto bit = s_builtin_map.find(name);
    if (bit != s_builtin_map.end()) {
        TamaInstr instr(TamaOp::CALL_BUILTIN);
        instr.builtin.id   = bit->second->id;
        instr.builtin.argc = argc;
        cs.result.code.push_back(instr);
        return true;
    }

    // Context function (TamaContext method or user-registered)
    uint8_t name_idx = (uint8_t)cs.result.ctx_fn_names.size();
    cs.result.ctx_fn_names.push_back(name);
    TamaInstr instr(TamaOp::CALL_CONTEXT);
    instr.ctx_fn.name_idx = name_idx;
    instr.ctx_fn.argc     = argc;
    cs.result.code.push_back(instr);
    return true;
}

bool TamaExprRuntime::_compile_unary(CompileState &cs) {
    cs.skip_ws();
    char c = cs.peek();
    if (c == '-') {
        ++cs.pos;
        if (!_compile_unary(cs)) return false;
        cs.result.code.push_back(TamaInstr(TamaOp::NEG));
        return true;
    }
    if (c == '!') {
        ++cs.pos;
        if (!_compile_unary(cs)) return false;
        cs.result.code.push_back(TamaInstr(TamaOp::NOT));
        return true;
    }
    return _compile_primary(cs);
}

bool TamaExprRuntime::_compile_mul(CompileState &cs) {
    if (!_compile_unary(cs)) return false;
    while (true) {
        cs.skip_ws();
        char c = cs.peek();
        if (c != '*' && c != '/' && c != '%') break;
        ++cs.pos;
        if (!_compile_unary(cs)) return false;
        TamaOp op = (c == '*') ? TamaOp::MUL : (c == '/') ? TamaOp::DIV : TamaOp::MOD;
        cs.result.code.push_back(TamaInstr(op));
    }
    return true;
}

bool TamaExprRuntime::_compile_add(CompileState &cs) {
    if (!_compile_mul(cs)) return false;
    while (true) {
        cs.skip_ws();
        char c = cs.peek();
        if (c != '+' && c != '-') break;
        ++cs.pos;
        if (!_compile_mul(cs)) return false;
        TamaOp op = (c == '+') ? TamaOp::ADD : TamaOp::SUB;
        cs.result.code.push_back(TamaInstr(op));
    }
    return true;
}

bool TamaExprRuntime::_compile_cmp(CompileState &cs) {
    if (!_compile_add(cs)) return false;
    while (true) {
        cs.skip_ws();
        char c0 = cs.peek(), c1 = cs.peek(1);
        TamaOp op;
        size_t len = 2;
        if      (c0=='=' && c1=='=') op = TamaOp::EQ;
        else if (c0=='!' && c1=='=') op = TamaOp::NEQ;
        else if (c0=='<' && c1=='=') op = TamaOp::LEQ;
        else if (c0=='>' && c1=='=') op = TamaOp::GEQ;
        else if (c0=='<') { op = TamaOp::LT; len = 1; }
        else if (c0=='>') { op = TamaOp::GT; len = 1; }
        else break;
        cs.pos += len;
        if (!_compile_add(cs)) return false;
        cs.result.code.push_back(TamaInstr(op));
    }
    return true;
}

bool TamaExprRuntime::_compile_and(CompileState &cs) {
    if (!_compile_cmp(cs)) return false;
    while (cs.skip_ws(), cs.peek() == '&' && cs.peek(1) == '&') {
        cs.pos += 2;
        if (!_compile_cmp(cs)) return false;
        cs.result.code.push_back(TamaInstr(TamaOp::AND));
    }
    return true;
}

bool TamaExprRuntime::_compile_expr(CompileState &cs) {
    if (!_compile_and(cs)) return false;
    while (cs.skip_ws(), cs.peek() == '|' && cs.peek(1) == '|') {
        cs.pos += 2;
        if (!_compile_and(cs)) return false;
        cs.result.code.push_back(TamaInstr(TamaOp::OR));
    }
    return true;
}

TamaExprChunk TamaExprRuntime::_compile(const std::string &expr, const std::vector<std::string> &var_names) {
    CompileState cs(expr, var_names);
    if (!_compile_expr(cs)) {
        // Return empty chunk — eval will return 0.0
        return TamaExprChunk{};
    }
    return std::move(cs.result);
}

// ---------------------------------------------------------------------------
// Stack-machine evaluator
// ---------------------------------------------------------------------------

double TamaExprRuntime::_eval_chunk(
        const TamaExprChunk &chunk,
        const double *values, size_t count,
        Object *context,
        const std::unordered_map<std::string, Callable> &user_fns)
{
    static thread_local double stack[256];
    int top = 0;

    for (const TamaInstr &instr : chunk.code) {
        switch (instr.op) {
        case TamaOp::PUSH_CONST:
            stack[top++] = instr.const_val;
            break;
        case TamaOp::PUSH_VAR:
            stack[top++] = (instr.slot < count) ? values[instr.slot] : 0.0;
            break;
        case TamaOp::NEG: stack[top-1] = -stack[top-1]; break;
        case TamaOp::NOT: stack[top-1] = (stack[top-1] == 0.0) ? 1.0 : 0.0; break;
        case TamaOp::ADD: { double b=stack[--top]; stack[top-1]+=b; } break;
        case TamaOp::SUB: { double b=stack[--top]; stack[top-1]-=b; } break;
        case TamaOp::MUL: { double b=stack[--top]; stack[top-1]*=b; } break;
        case TamaOp::DIV: { double b=stack[--top]; stack[top-1] = (b!=0.0) ? stack[top-1]/b : 0.0; } break;
        case TamaOp::MOD: { double b=stack[--top]; stack[top-1] = (b!=0.0) ? std::fmod(stack[top-1],b) : 0.0; } break;
        case TamaOp::EQ:  { double b=stack[--top]; stack[top-1] = (stack[top-1]==b) ? 1.0 : 0.0; } break;
        case TamaOp::NEQ: { double b=stack[--top]; stack[top-1] = (stack[top-1]!=b) ? 1.0 : 0.0; } break;
        case TamaOp::LT:  { double b=stack[--top]; stack[top-1] = (stack[top-1]< b) ? 1.0 : 0.0; } break;
        case TamaOp::GT:  { double b=stack[--top]; stack[top-1] = (stack[top-1]> b) ? 1.0 : 0.0; } break;
        case TamaOp::LEQ: { double b=stack[--top]; stack[top-1] = (stack[top-1]<=b) ? 1.0 : 0.0; } break;
        case TamaOp::GEQ: { double b=stack[--top]; stack[top-1] = (stack[top-1]>=b) ? 1.0 : 0.0; } break;
        case TamaOp::AND: { double b=stack[--top]; stack[top-1] = (stack[top-1]!=0.0 && b!=0.0) ? 1.0 : 0.0; } break;
        case TamaOp::OR:  { double b=stack[--top]; stack[top-1] = (stack[top-1]!=0.0 || b!=0.0) ? 1.0 : 0.0; } break;
        case TamaOp::CALL_BUILTIN: {
            uint8_t argc = instr.builtin.argc;
            top -= argc;
            stack[top] = dispatch_builtin(instr.builtin.id, &stack[top]);
            ++top;
            break;
        }
        case TamaOp::CALL_CONTEXT: {
            uint8_t argc = instr.ctx_fn.argc;
            const std::string &fn_name = chunk.ctx_fn_names[instr.ctx_fn.name_idx];
            // Collect args as Variant array
            Array call_args;
            top -= argc;
            for (uint8_t i = 0; i < argc; ++i)
                call_args.push_back(stack[top + i]);

            double result = 0.0;
            // Check user-registered functions first
            auto uit = user_fns.find(fn_name);
            if (uit != user_fns.end()) {
                Variant r = uit->second.callv(call_args);
                result = (double)r;
            } else if (context != nullptr) {
                // Call on the TamaContext object
                Variant r = context->callv(StringName(fn_name.c_str()), call_args);
                result = (double)r;
            }
            stack[top++] = result;
            break;
        }
        }
    }
    return (top > 0) ? stack[top-1] : 0.0;
}

// ---------------------------------------------------------------------------
// Cache lookup / public eval
// ---------------------------------------------------------------------------

const TamaExprChunk &TamaExprRuntime::_get_chunk(
        const std::string &expr,
        const std::vector<std::string> &var_names,
        const std::string &cache_key)
{
    auto it = _cache.find(cache_key);
    if (it != _cache.end()) return it->second;
    auto [ins, _] = _cache.emplace(cache_key, _compile(expr, var_names));
    return ins->second;
}

double TamaExprRuntime::eval(
        const String &p_expr,
        const PackedStringArray &p_var_names,
        const PackedFloat64Array &p_var_values,
        Object *p_context)
{
    std::string expr = p_expr.utf8().get_data();
    // Strip whitespace
    while (!expr.empty() && (expr.front() == ' ' || expr.front() == '\t')) expr.erase(expr.begin());
    while (!expr.empty() && (expr.back()  == ' ' || expr.back()  == '\t')) expr.pop_back();
    if (expr.empty()) return 0.0;

    // Fast-path: literal number
    {
        char *end;
        double v = std::strtod(expr.c_str(), &end);
        if (end == expr.c_str() + expr.size()) return v;
    }

    int64_t nv = p_var_names.size();
    int64_t nval = p_var_values.size();

    // Build variable name list and values array
    std::vector<std::string> var_names;
    var_names.reserve((size_t)nv);
    for (int64_t i = 0; i < nv; ++i)
        var_names.push_back(p_var_names[i].utf8().get_data());

    // Build cache key: "expr|name0,name1,..."
    std::string cache_key = expr + "|";
    for (size_t i = 0; i < var_names.size(); ++i) {
        if (i > 0) cache_key += ',';
        cache_key += var_names[i];
    }

    // Fast-path: single variable name that exactly matches the expression
    if (nv > 0 && nval > 0) {
        for (int64_t i = 0; i < nv; ++i) {
            if (var_names[(size_t)i] == expr)
                return p_var_values[i];
        }
    }

    const TamaExprChunk &chunk = _get_chunk(expr, var_names, cache_key);
    if (chunk.code.empty()) return 0.0;

    const double *values_ptr = p_var_values.ptr();
    size_t values_count = (size_t)nval;

    return _eval_chunk(chunk, values_ptr, values_count, p_context, _user_fns);
}

// ---------------------------------------------------------------------------
// User function registration
// ---------------------------------------------------------------------------

void TamaExprRuntime::register_function(const String &p_name, const Callable &p_callable) {
    _user_fns[p_name.utf8().get_data()] = p_callable;
    // Invalidate cache entries that might reference this function name
    std::string name = p_name.utf8().get_data();
    for (auto it = _cache.begin(); it != _cache.end(); ) {
        bool has_ctx = false;
        for (const auto &fn : it->second.ctx_fn_names)
            if (fn == name) { has_ctx = true; break; }
        it = has_ctx ? _cache.erase(it) : std::next(it);
    }
}

void TamaExprRuntime::unregister_function(const String &p_name) {
    _user_fns.erase(p_name.utf8().get_data());
}

void TamaExprRuntime::clear_cache() {
    _cache.clear();
}

const TamaExprChunk *TamaExprRuntime::get_chunk(
        const std::string &expr,
        const std::vector<std::string> &var_names,
        const std::string &cache_key)
{
    return &_get_chunk(expr, var_names, cache_key);
}

double TamaExprRuntime::eval_chunk(
        const TamaExprChunk &chunk,
        const double *values, size_t count,
        Object *context) const
{
    return _eval_chunk(chunk, values, count, context, _user_fns);
}

// ---------------------------------------------------------------------------
// GDExtension binding
// ---------------------------------------------------------------------------

void TamaExprRuntime::_bind_methods() {
    ClassDB::bind_method(D_METHOD("eval", "expr", "var_names", "var_values", "context"),
                         &TamaExprRuntime::eval);
    ClassDB::bind_method(D_METHOD("register_function", "name", "callable"),
                         &TamaExprRuntime::register_function);
    ClassDB::bind_method(D_METHOD("unregister_function", "name"),
                         &TamaExprRuntime::unregister_function);
    ClassDB::bind_method(D_METHOD("clear_cache"),
                         &TamaExprRuntime::clear_cache);
}
