#pragma once
#include "tama_animated_texture.h"
#include "tama_draw_job.h"
#include "tama_server_bullet.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2.hpp>

class _TamaDrawCoordinator;

class TamaServerBulletPool : public godot::Node2D {
    GDCLASS(TamaServerBulletPool, godot::Node2D)
    friend class _TamaDrawCoordinator;

    // ------------------------------------------------------------------
    // Internal structures
    // ------------------------------------------------------------------

    static constexpr int BATCH_CHUNK = 256;

    struct BatchData {
        godot::Ref<godot::MultiMesh> multimesh_res;
        godot::RID multimesh;
        int used         = 0;   // packed: slots [0, used) are all live (== active_count)
        int active_count = 0;
        int birth_frame  = -1;
        // Per-batch main animation state (independent from other batches of same type)
        float anim_time  = 0.0f;
        int   anim_frame = 0;
        godot::Ref<godot::Texture2D> current_texture;
        // Per-batch spawn animation state
        float spawn_anim_time  = 0.0f;
        int   spawn_anim_frame = 0;
        // local_slot -> BulletState*, so recycle can pull the last live slot into a
        // freed hole and keep the batch compact (used == active_count).
        std::vector<BulletState *> slot_bullets;
        // Number of bullets in this batch still in spawn animation (spawn_frames_remaining > 0).
        // Spawn animation state is per-bullet (BulletState), not per-batch.
        int spawn_active_count = 0;
        // Spawn multimesh — created lazily when first needed, reused across batch reuses.
        godot::Ref<godot::MultiMesh> spawn_multimesh_res;
        godot::RID spawn_multimesh;
    };

    struct TypeData {
        // Original config object — used for fallback lookup by pointer in spawn()
        godot::Object *config_obj = nullptr;

        // Config properties (read once at register_type)
        std::vector<TamaAnimFrame> anim_frames;       // empty = no texture; size>1 = animated
        godot::Ref<godot::Texture2D> first_texture;   // anim_frames[0].texture, for static/composite draws

        godot::Rect2    rect         = {-8.0f, -8.0f, 16.0f, 16.0f};
        godot::Vector2  texture_scale = {1.0f, 1.0f};
        float  shape_radius   = 6.0f;
        bool   rotates              = true;
        bool   face_velocity        = true;
        int    pool_size            = 1000;
        float  out_of_bounds_margin = 50.0f;

        // Spawn animation config (read once at register_type)
        int    spawn_delay             = 0;
        float  starting_spawn_scale    = 2.0f;
        float  starting_spawn_opacity  = 0.0f;
        godot::Ref<godot::Texture2D> spawn_texture;
        // QuadMesh sized to spawn_texture dimensions (created when spawn_delay > 0)
        godot::Ref<godot::QuadMesh> spawn_quad;

        // Blend mode (from animation / spawn_texture TamaAnimatedTexture)
        int blend_mode       = 0;
        int spawn_blend_mode = 0;

        // Spawn animation frames (from spawn_texture TamaAnimatedTexture)
        std::vector<TamaAnimFrame> spawn_anim_frames;

        // GPU resources
        godot::Ref<godot::QuadMesh> quad;

        // Bullet state flat array (indexed by global slot)
        std::vector<BulletState>        bullets;
        std::vector<TamaServerBullet *> wrappers;

        // FIFO ring buffer for slot allocation
        std::vector<int32_t> ring;
        int ring_r     = 0;
        int ring_w     = 0;
        int ring_count = 0;

        // Render batch management
        std::vector<BatchData *> active_batches; // birth-order (oldest first)
        std::vector<BatchData *> free_batches;
        BatchData *cur_batch  = nullptr;
        int64_t    cur_frame  = -1; // physics frame cur_batch was opened (animated types only)
    };

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    std::unordered_map<std::string, TypeData *> _types;

    // Flat list of all active bullets across all types (ptr into TypeData.bullets)
    std::vector<BulletState *> _active;

    // Persistent recycle list — cleared at the top of each _physics_process to avoid per-frame allocation
    std::vector<BulletState *> _to_recycle;

    // Pre-allocated buffer for spawn animation draw path (transform + color per instance)
    godot::PackedFloat32Array _spawn_buf;

    // Registrations queued before the node enters the scene tree
    struct PendingReg { godot::String key; godot::Object *config; };
    std::vector<PendingReg> _pending_regs;


    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------

    BatchData *_get_batch(TypeData &td);
    void       _recycle_batch(TypeData &td, BatchData *batch);
    void       _recycle_internal(BulletState *b);

    void _step_tweens(BulletState &b, float delta);
    // C++ event handlers — called directly by TamaServerBullet (friend)
    void _apply_chdir    (BulletState &b, const TamaChdirEvent    &e);
    void _apply_chspd    (BulletState &b, const TamaChspdEvent    &e);
    void _apply_chrotspd (BulletState &b, const TamaChrotspdEvent &e);
    void _apply_chpos    (BulletState &b, const TamaChposEvent    &e);
    void _apply_accel    (BulletState &b, const TamaAccelEvent    &e);

    friend class TamaServerBullet;

    float _dir_to_angle(const BulletState &b, int dir_type, float value) const;
    float _spd_to_value(const BulletState &b, int speed_type, float value) const;
    float _accel_axis_end(int axis_type, float value, float current, float over) const;

    godot::Rect2 _world_bounds() const;

protected:
    static void _bind_methods();

public:
    TamaServerBulletPool()  = default;
    ~TamaServerBulletPool() override;

    // Godot virtuals
    void _ready()                       override;
    void _physics_process(double delta) override;
    void _exit_tree()                   override;

    // ------------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------------

    void register_type(const godot::String &key, godot::Object *config);

    int get_active_count() const { return (int)_active.size(); }

    void collect_draw_jobs(std::vector<DrawJob> &out);

    // Returns the TamaServerBullet wrapper, or null if the pool is full.
    godot::Object *spawn(const TamaBulletFireData &data, godot::Object *config,
                         float angle, float speed, godot::Vector2 position,
                         godot::Object *context);

    void recycle(godot::Object *bullet_wrapper);
    void recycle_all();

    // ------------------------------------------------------------------
    // Properties
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // Signals
    // ------------------------------------------------------------------
    // bullet_hit(bullet: TamaServerBullet, body_instance_id: int)
};
