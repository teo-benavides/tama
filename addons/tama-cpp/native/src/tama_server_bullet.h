#pragma once
#include "tama_expr.h"

#include <cstdint>
#include <string>
#include <vector>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2.hpp>

// ---------------------------------------------------------------------------
// Internal per-bullet state (flat array element)
// ---------------------------------------------------------------------------

struct TweenState1D {
    bool  active   = false;
    float from     = 0.0f;
    float to       = 0.0f;
    float elapsed  = 0.0f;
    float duration = 1.0f;

    // Advance by delta and return interpolated value. Deactivates when done.
    float step(float delta) {
        elapsed += delta;
        float t = elapsed / duration;
        if (t >= 1.0f) { t = 1.0f; active = false; }
        return from + (to - from) * t;
    }
};

struct TweenState2D {
    bool           active   = false;
    godot::Vector2 from;
    godot::Vector2 to;
    float          elapsed  = 0.0f;
    float          duration = 1.0f;

    godot::Vector2 step(float delta) {
        elapsed += delta;
        float t = elapsed / duration;
        if (t >= 1.0f) { t = 1.0f; active = false; }
        return from.lerp(to, t);
    }
};

struct BulletState {
    // Motion
    godot::Vector2 position;
    godot::Vector2 initial_position;
    float angle      = 0.0f;
    float speed      = 0.0f;
    float speed_x    = 0.0f;
    float speed_y    = 0.0f;
    bool  rotates    = true;
    float last_angle = 0.0f;
    float last_speed = 0.0f;
    godot::Vector2 texture_scale = {1.0f, 1.0f};

    // Pool management
    bool    active      = false;
    int32_t active_idx  = -1;  // index in pool's _active[]
    int32_t global_slot = -1;  // PhysicsServer2D shape index
    void   *batch_ptr   = nullptr; // raw BatchData* — void* to avoid circular header dep
    int32_t local_slot  = -1;  // index within batch's multimesh

    // Cached RIDs (set at spawn, not hot to look up)
    godot::RID area_rid;
    godot::RID multimesh_rid;

    // Tweens
    TweenState1D angle_tween;
    TweenState1D speed_tween;
    TweenState2D pos_tween;
    TweenState1D sx_tween;
    TweenState1D sy_tween;

    // mvmt expressions (set at spawn, evaluated every frame)
    bool mvmt_x_set  = false;
    bool mvmt_y_set  = false;
    int  mvmt_x_type = 0;     // ValueType enum: 0=ABS, 1=REL, 2=SEQ
    int  mvmt_y_type = 0;
    const TamaExprChunk *mvmt_x_chunk = nullptr;  // stable ptr into TamaExprRuntime cache
    const TamaExprChunk *mvmt_y_chunk = nullptr;
    std::vector<double> mvmt_values;               // snapshot of variable values at spawn time

    godot::Object *runner = nullptr;

    // Wrapper Object exposed through bullet_hit signal (created once per slot)
    // Pointer set by TamaServerBulletPool::register_type, valid for pool lifetime.
    godot::Object *wrapper = nullptr;
};

// ---------------------------------------------------------------------------
// Thin proxy exposed through the bullet_hit signal
// ---------------------------------------------------------------------------

class TamaServerBulletPool; // forward declaration

class TamaServerBullet : public godot::Object {
    GDCLASS(TamaServerBullet, godot::Object)

    // Pointer back to the owning pool and this bullet's slot index.
    // Only valid while the pool is alive.
    TamaServerBulletPool *_pool     = nullptr;
    int32_t               _slot_idx = -1;

    // Cached type key for fast slot lookup
    std::string _type_key;

public:
    BulletState *_state = nullptr; // direct pointer, set at registration (public for pool access)

protected:
    static void _bind_methods();

public:
    TamaServerBullet() = default;

    // Called once by the pool when the slot is created.
    void _init_slot(TamaServerBulletPool *pool, BulletState *state);

    godot::Vector2 get_position()  const;
    float          get_angle()     const;
    float          get_speed()     const;
    bool           get_active()    const;
    float          get_speed_x()   const;
    float          get_speed_y()   const;
};
