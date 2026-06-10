#pragma once
#include "tama_interpreter.h"
#include "tama_server_laser.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

class TamaServerLaserPool : public godot::Node2D {
    GDCLASS(TamaServerLaserPool, godot::Node2D)

    // ------------------------------------------------------------------
    // Internal structures
    // ------------------------------------------------------------------

    struct TypeData {
        godot::Object *config_obj = nullptr;

        // Config snapshot (read once at register_type)
        float  width            = 20.0f;
        float  length           = 1000.0f;
        godot::Ref<godot::Texture2D> texture;
        bool   tile             = true;
        godot::Ref<godot::Texture2D> base_texture;
        int    delay_frames     = 120;
        int    expand_frames    = 10;
        int    duration_frames  = 120;
        int    fade_frames      = 30;
        int    pool_size        = 100;
        int    collision_layer  = 1;
        int    collision_mask   = 2;

        // Physics area with pool_size pre-allocated rectangle shapes
        godot::RID area;
        std::vector<godot::RID> shapes;

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
    void _update_shape_transform(TypeData &td, LaserState &l);

    static void _area_monitor_callback(
            int status, godot::RID body_rid, int64_t body_iid,
            int body_shape, int local_shape,
            TamaServerLaserPool *self, godot::String type_key);

    friend class TamaServerLaser;

protected:
    static void _bind_methods();

public:
    TamaServerLaserPool()  = default;
    ~TamaServerLaserPool() override;

    void _ready()                        override;
    void _physics_process(double delta)  override;
    void _draw()                         override;
    void _exit_tree()                    override;

    void register_type(const godot::String &key, godot::Object *config);

    int get_active_count() const { return (int)_active.size(); }

    // Returns the TamaServerLaser wrapper, or null if the pool is full.
    godot::Object *spawn(const TamaBulletFireData &data, godot::Object *config,
                         float angle, godot::Vector2 position,
                         godot::Object *context);

    void recycle(godot::Object *laser_wrapper);
    void recycle_all();

    // laser_hit(laser: TamaServerLaser, body_instance_id: int)
};
