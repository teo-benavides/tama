#pragma once
#include "tama_animated_texture.h"
#include "tama_interpreter.h"
#include "tama_server_curved_laser.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

class TamaServerCurvedLaserPool : public godot::Node2D {
    GDCLASS(TamaServerCurvedLaserPool, godot::Node2D)

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
        int    collision_layer      = 1;
        int    collision_mask       = 2;

        // One shared area with pool_size*(length-1) pre-allocated rectangle shapes.
        // Shape index = global_slot*(length-1) + segment_index.
        godot::RID area;
        std::vector<godot::RID> shapes;

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

    // Per-texture accumulation buckets — cleared each frame, capacity retained
    struct DrawBucket {
        godot::RID               texture_rid;
        std::vector<godot::Vector2> points;
        std::vector<godot::Color>   colors;
        std::vector<godot::Vector2> uvs;
        std::vector<int32_t>        indices;
    };
    std::unordered_map<int64_t, DrawBucket> _draw_buckets;

    // Persistent Packed* buffers for the flush — sized up as needed, never shrunk
    godot::PackedInt32Array    _flush_idx;
    godot::PackedVector2Array  _flush_pts;
    godot::PackedColorArray    _flush_col;
    godot::PackedVector2Array  _flush_uv;

    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------

    void _recycle_internal(CurvedLaserState *l);
    godot::Rect2 _world_bounds() const;

    static void _area_monitor_callback(
        int status, godot::RID body_rid, int64_t body_iid,
        int body_shape, int local_shape,
        TamaServerCurvedLaserPool *self, godot::String type_key);

    friend class TamaServerCurvedLaser;

protected:
    static void _bind_methods();

public:
    TamaServerCurvedLaserPool()  = default;
    ~TamaServerCurvedLaserPool() override;

    void _ready()                        override;
    void _physics_process(double delta)  override;
    void _draw()                         override;
    void _exit_tree()                    override;

    void register_type(const godot::String &key, godot::Object *config);

    int get_active_count() const { return (int)_active.size(); }

    godot::Object *spawn(const TamaBulletFireData &data, godot::Object *config,
                         float angle, float speed, godot::Vector2 position,
                         godot::Object *context);

    void recycle(godot::Object *laser_wrapper);
    void recycle_all();
};
