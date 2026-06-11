#pragma once
#include "tama_interpreter.h"
#include "tama_server_bullet.h"  // TweenState1D

#include <cstdint>
#include <string>
#include <vector>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2.hpp>

// ---------------------------------------------------------------------------
// Per-laser state (flat array element in TamaServerCurvedLaserPool::TypeData)
// ---------------------------------------------------------------------------

struct CurvedLaserState {
    godot::Vector2 position;
    float angle      = 0.0f;
    float last_angle = 0.0f;

    float speed           = 0.0f;
    float last_speed      = 0.0f;
    float rot_speed       = 0.0f;   // degrees/sec
    float last_rot_speed  = 0.0f;

    bool    active      = false;
    int64_t spawn_frame = -1;
    int32_t active_idx  = -1;
    int32_t global_slot = -1;

    // Collision: one rectangle shape per trail buffer slot, indexed by the slot
    // of the segment's NEWER node (stable as the trail scrolls). Once a segment's
    // two endpoints are frozen history points its shape never changes, so each
    // frame only the new head segment is (re)built and the scrolled-off tail
    // segment is disabled. collision_built tracks the one-time full rebuild on
    // first spawn (handles any trail already built before collision is wanted).
    bool collision_built = false;

    TweenState1D angle_tween;
    TweenState1D speed_tween;
    TweenState1D rot_speed_tween;

    // Position history – circular buffer; index via (trail_head + i) % capacity
    // i=0 is the newest (head/tip), i=trail_count-1 is the oldest (tail)
    std::vector<godot::Vector2> trail;
    int trail_head  = 0;
    int trail_count = 0;

    // Cached ribbon edge vertices, indexed by buffer slot (stable as the trail
    // scrolls). Updated incrementally in _physics_process: only the new head slot
    // and the previous head slot (which gains a new neighbor) are recomputed each
    // frame — O(1) instead of O(trail_count) sqrt/normalize in _draw().
    std::vector<godot::Vector2> edge_l;
    std::vector<godot::Vector2> edge_r;
    bool edge_built = false;

    // Texture animation
    int   tex_anim_frame = 0;
    float tex_anim_time  = 0.0f;

    float current_width = 1.0f;

    godot::RID area_rid;

    _TamaInterpreter *runner  = nullptr;
    godot::Object    *wrapper = nullptr;
};

// ---------------------------------------------------------------------------
// Thin wrapper Object exposed via laser_hit signal
// ---------------------------------------------------------------------------

class TamaServerCurvedLaserPool;  // forward

class TamaServerCurvedLaser : public godot::Object, public TamaBulletEventHandler {
    GDCLASS(TamaServerCurvedLaser, godot::Object)

    TamaServerCurvedLaserPool *_pool     = nullptr;
    std::string                _type_key;

public:
    CurvedLaserState *_state = nullptr;

protected:
    static void _bind_methods();

public:
    TamaServerCurvedLaser() = default;

    void _init_slot(TamaServerCurvedLaserPool *pool, CurvedLaserState *state, const std::string &key);

    godot::Vector2 get_position() const;
    float          get_angle()    const;
    bool           get_active()   const;

    void on_chdir    (const TamaChdirEvent    &e) override;
    void on_chspd    (const TamaChspdEvent    &e) override;
    void on_chrotspd (const TamaChrotspdEvent &e) override;
    void on_chpos    (const TamaChposEvent    &e) override;
    void on_vanished ()                            override;
    void on_accel    (const TamaAccelEvent    &)  override {}
};
