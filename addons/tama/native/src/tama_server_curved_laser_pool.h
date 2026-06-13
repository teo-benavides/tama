#pragma once
#include "tama_animated_texture.h"
#include "tama_draw_job.h"
#include "tama_interpreter.h"
#include "tama_server_curved_laser.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

class _TamaDrawCoordinator;

class TamaServerCurvedLaserPool : public godot::Node2D {
    GDCLASS(TamaServerCurvedLaserPool, godot::Node2D)
    friend class _TamaDrawCoordinator;

    // ------------------------------------------------------------------
    // Internal structures
    // ------------------------------------------------------------------

    struct TypeData {
        godot::Object *config_obj = nullptr;

        float  width                = 20.0f;
        int    length               = 30;    // number of trail nodes
        std::vector<TamaAnimFrame> texture_frames;
        int    pool_size            = 100;
        float  out_of_bounds_margin = 50.0f;

        int blend_mode = 0;
        int z_index    = 0;

        std::vector<CurvedLaserState>        lasers;
        std::vector<TamaServerCurvedLaser *> wrappers;

        std::vector<int32_t> ring;
        int ring_r     = 0;
        int ring_w     = 0;
        int ring_count = 0;
    };

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    std::unordered_map<std::string, TypeData *> _types;
    std::vector<CurvedLaserState *> _active;
    std::vector<CurvedLaserState *> _to_recycle;

    struct PendingReg { godot::String key; godot::Object *config; };
    std::vector<PendingReg> _pending_regs;

    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------

    void _recycle_internal(CurvedLaserState *l);
    godot::Rect2 _world_bounds() const;

    friend class TamaServerCurvedLaser;

protected:
    static void _bind_methods();

public:
    TamaServerCurvedLaserPool()  = default;
    ~TamaServerCurvedLaserPool() override;

    void _ready()                        override;
    void _physics_process(double delta)  override;
    void _exit_tree()                    override;

    void register_type(const godot::String &key, godot::Object *config);

    int get_active_count() const { return (int)_active.size(); }

    void collect_draw_jobs(std::vector<DrawJob> &out);

    godot::Object *spawn(const TamaBulletFireData &data, godot::Object *config,
                         float angle, float speed, godot::Vector2 position,
                         godot::Object *context);

    void recycle(godot::Object *laser_wrapper);
    void recycle_all();
};
