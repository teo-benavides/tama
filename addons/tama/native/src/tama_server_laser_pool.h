#pragma once
#include "tama_animated_texture.h"
#include "tama_draw_job.h"
#include "tama_interpreter.h"
#include "tama_server_laser.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

class _TamaDrawCoordinator;

class TamaServerLaserPool : public godot::Node2D {
    GDCLASS(TamaServerLaserPool, godot::Node2D)
    friend class _TamaDrawCoordinator;

    // ------------------------------------------------------------------
    // Internal structures
    // ------------------------------------------------------------------

    struct TypeData {
        godot::Object *config_obj = nullptr;

        // Config snapshot (read once at register_type)
        float  width            = 20.0f;
        float  length           = 1000.0f;
        std::vector<TamaAnimFrame> texture_frames;      // empty = no texture; size>1 = animated
        bool   tile_x           = false;
        bool   tile_y           = false;
        std::vector<TamaAnimFrame> base_texture_frames; // empty = no base texture; size>1 = animated

        int blend_mode      = 0;
        int z_index         = 0;
        int    delay_frames     = 120;
        int    expand_frames    = 10;
        int    duration_frames  = 120;
        int    fade_frames      = 30;
        int    pool_size        = 100;

        // Laser state flat array (indexed by global_slot)
        std::vector<LaserState>        lasers;
        std::vector<TamaServerLaser *> wrappers;

        // FIFO ring buffer for slot allocation
        std::vector<int32_t> ring;
        int ring_r     = 0;
        int ring_w     = 0;
        int ring_count = 0;
    };

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    std::unordered_map<std::string, TypeData *> _types;
    std::vector<LaserState *> _active;
    std::vector<LaserState *> _to_recycle;

    struct PendingReg { godot::String key; godot::Object *config; };
    std::vector<PendingReg> _pending_regs;

    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------

    void _recycle_internal(LaserState *l);

    friend class TamaServerLaser;

protected:
    static void _bind_methods();

public:
    TamaServerLaserPool()  = default;
    ~TamaServerLaserPool() override;

    void _ready()                        override;
    void _physics_process(double delta)  override;
    void _exit_tree()                    override;

    void register_type(const godot::String &key, godot::Object *config);

    int get_active_count() const { return (int)_active.size(); }

    void collect_draw_jobs(std::vector<DrawJob> &out);

    // Returns the TamaServerLaser wrapper, or null if the pool is full.
    godot::Object *spawn(const TamaBulletFireData &data, godot::Object *config,
                         float angle, godot::Vector2 position,
                         godot::Object *context);

    void recycle(godot::Object *laser_wrapper);
    void recycle_all();

    // laser_hit(laser: TamaServerLaser, body_instance_id: int)
};
