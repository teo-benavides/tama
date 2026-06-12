#pragma once
#include "tama_draw_job.h"
#include "tama_server_bullet_pool.h"
#include "tama_server_laser_pool.h"
#include "tama_server_curved_laser_pool.h"

#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rid.hpp>

// Internal draw coordinator: one canvas item per blend mode, draw commands emitted
// in global spawn_frame order so temporal ordering is preserved across all object types.
class _TamaDrawCoordinator : public godot::Node2D {
    GDCLASS(_TamaDrawCoordinator, godot::Node2D)

    // Per-blend-mode canvas items (children of this node's canvas item).
    // z_index = blend_mode value, so normal (0) renders below additive (1), etc.
    std::unordered_map<int, godot::RID> _blend_cis;
    std::unordered_map<int, godot::Ref<godot::CanvasItemMaterial>> _blend_mats;

    // Draw jobs collected from all pools in physics_process, sorted by spawn_frame.
    std::vector<DrawJob> _jobs;

    TamaServerBulletPool      *_bullet_pool = nullptr;
    TamaServerLaserPool       *_laser_pool  = nullptr;
    TamaServerCurvedLaserPool *_curved_pool = nullptr;

    // Reusable Packed* arrays for the final RenderingServer call (curved laser batching).
    godot::PackedInt32Array   _flush_idx;
    godot::PackedVector2Array _flush_pts;
    godot::PackedColorArray   _flush_col;
    godot::PackedVector2Array _flush_uv;

    // Accumulation buffers for same-frame curved laser batching.
    // Grown as needed, never shrunk. Cleared between batches.
    std::vector<int32_t>          _idx_buf;
    std::vector<godot::Vector2>   _acc_pts;
    std::vector<godot::Color>     _acc_col;
    std::vector<godot::Vector2>   _acc_uv;

    godot::RID _get_or_create_blend_ci(int blend_mode);

    void _draw_bullet_batch(
        TamaServerBulletPool::TypeData *td,
        TamaServerBulletPool::BatchData *batch,
        godot::RID ci,
        godot::RenderingServer *rs);

    void _draw_bullet_batch_spawn(
        TamaServerBulletPool::TypeData *td,
        TamaServerBulletPool::BatchData *batch,
        godot::RID ci,
        godot::RenderingServer *rs);

    void _draw_straight_laser(
        TamaServerLaserPool::TypeData *td,
        LaserState *l,
        godot::RID ci,
        godot::RenderingServer *rs);

protected:
    static void _bind_methods();

public:
    _TamaDrawCoordinator()           = default;
    ~_TamaDrawCoordinator() override = default;

    void _physics_process(double delta) override;
    void _draw()                        override;
    void _exit_tree()                   override;

    void set_pools(
        TamaServerBulletPool      *bullet,
        TamaServerLaserPool       *laser,
        TamaServerCurvedLaserPool *curved)
    {
        _bullet_pool = bullet;
        _laser_pool  = laser;
        _curved_pool = curved;
    }
};
