#pragma once
#include "tama_interpreter.h"
#include "tama_server_bullet.h"  // TweenState1D

#include <cstdint>
#include <string>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/vector2.hpp>

// ---------------------------------------------------------------------------
// Per-laser state (flat array element in TamaServerLaserPool::TypeData)
// ---------------------------------------------------------------------------

struct LaserState {
    godot::Vector2 position;
    float angle      = 0.0f;
    float last_angle = 0.0f;  // for SEQ direction type support

    bool    active         = false;
    int64_t spawn_frame    = -1;
    int32_t active_idx     = -1;
    int32_t global_slot    = -1;
    void   *type_data_ptr  = nullptr;  // points to owning TypeData; set in spawn()

    // Lifecycle phase: 0=delay, 1=expand, 2=active, 3=fade
    int phase         = 0;
    int frame_counter = 0;

    // Angle tween (for chdir with over > 0)
    TweenState1D angle_tween;

    // Texture animation state (per-laser, advances during active phases)
    int   tex_anim_frame  = 0;
    float tex_anim_time   = 0.0f;
    int   base_anim_frame = 0;
    float base_anim_time  = 0.0f;

    // Per-frame computed render values
    float current_width   = 1.0f;
    float current_opacity = 1.0f;

    _TamaInterpreter *runner  = nullptr;
    godot::Object    *wrapper = nullptr;
};

// ---------------------------------------------------------------------------
// Thin wrapper Object exposed through the laser_hit signal
// ---------------------------------------------------------------------------

class TamaServerLaserPool; // forward declaration

class TamaServerLaser : public godot::Object, public TamaBulletEventHandler {
    GDCLASS(TamaServerLaser, godot::Object)

    TamaServerLaserPool *_pool     = nullptr;
    std::string          _type_key;

public:
    LaserState *_state = nullptr;

protected:
    static void _bind_methods();

public:
    TamaServerLaser() = default;

    void _init_slot(TamaServerLaserPool *pool, LaserState *state, const std::string &key);

    godot::Vector2 get_position() const;
    float          get_angle()    const;
    bool           get_active()   const;
    int            get_phase()    const;

    // TamaBulletEventHandler
    void on_chdir    (const TamaChdirEvent    &e) override;
    void on_chpos    (const TamaChposEvent    &e) override;
    void on_vanished ()                            override;
    // speed/accel are no-ops for lasers
    void on_chspd    (const TamaChspdEvent    &)  override {}
    void on_chrotspd (const TamaChrotspdEvent &)  override {}
    void on_accel    (const TamaAccelEvent    &)  override {}
};
