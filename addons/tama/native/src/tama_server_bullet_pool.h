#pragma once
#include "tama_server_bullet.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

class TamaServerBulletPool : public godot::Node2D {
    GDCLASS(TamaServerBulletPool, godot::Node2D)

    // ------------------------------------------------------------------
    // Internal structures
    // ------------------------------------------------------------------

    static constexpr int BATCH_CHUNK = 256;

    struct BatchData {
        godot::Ref<godot::MultiMesh> multimesh_res;
        godot::RID multimesh;
        int used         = 0;
        int active_count = 0;
        int birth_frame  = -1;
        // Per-batch animation state (independent from other batches of same type)
        float anim_time  = 0.0f;
        int   anim_frame = 0;
        godot::Ref<godot::Texture2D> current_texture;
    };

    struct TypeData {
        // Original config object — used for fallback lookup by pointer in spawn()
        godot::Object *config_obj = nullptr;

        // Config properties (read once at register_type)
        godot::TypedArray<godot::Texture2D> frames;
        float  fps             = 0.0f;
        godot::Ref<godot::Texture2D> first_texture; // frames[0], used for static/composite draws

        godot::Rect2    rect         = {-8.0f, -8.0f, 16.0f, 16.0f};
        godot::Vector2  texture_scale = {1.0f, 1.0f};
        float  shape_radius   = 6.0f;
        int    collision_layer = 1;
        int    collision_mask  = 2;
        bool   rotates              = true;
        bool   face_velocity        = true;
        int    pool_size            = 1000;
        float  out_of_bounds_margin = 50.0f;

        // GPU resources
        godot::Ref<godot::QuadMesh> quad;
        godot::Ref<godot::MultiMesh> composite_res;
        godot::RID composite;
        godot::RID area;
        std::vector<godot::RID> shapes;

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
        int64_t    cur_frame  = -1;
    };

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    std::unordered_map<std::string, TypeData *> _types;

    // Flat list of all active bullets across all types (ptr into TypeData.bullets)
    std::vector<BulletState *> _active;

    // Persistent recycle list — cleared at the top of each _physics_process to avoid per-frame allocation
    std::vector<BulletState *> _to_recycle;

    // Pre-allocated buffer for the composite draw path — reused across frames and types
    godot::PackedFloat32Array _composite_buf;

    // Registrations queued before the node enters the scene tree
    struct PendingReg { godot::String key; godot::Object *config; };
    std::vector<PendingReg> _pending_regs;

    // Cached threshold (read from ProjectSettings in _ready)
    int _composite_threshold = 1000;


    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------

    BatchData *_get_batch(TypeData &td);
    void       _recycle_batch(TypeData &td, BatchData *batch);
    void       _recycle_internal(BulletState *b);

    void _step_tweens(BulletState &b, float delta);
    // Signal handlers: (signal_data, wrapper_object) — wrapper is bound via Callable::bind()
    // Using Object* as the bound arg because BulletState* is not Variant-compatible.
    void _on_changed_direction(godot::Object *data, godot::Object *wrapper);
    void _on_changed_speed(godot::Object *data, godot::Object *wrapper);
    void _on_changed_position(godot::Object *data, godot::Object *wrapper);
    void _on_accelerated(godot::Object *data, godot::Object *wrapper);

    float _dir_to_angle(const BulletState &b, int dir_type, float value) const;
    float _spd_to_value(const BulletState &b, int speed_type, float value) const;
    float _accel_axis_end(int axis_type, float value, float current, float over) const;

    godot::Rect2 _world_bounds() const;

    static void _area_monitor_callback(
            int status, godot::RID body_rid, int64_t body_iid,
            int body_shape, int local_shape,
            TamaServerBulletPool *self, godot::String type_key);

protected:
    static void _bind_methods();

public:
    TamaServerBulletPool()  = default;
    ~TamaServerBulletPool() override;

    // Godot virtuals
    void _ready()                   override;
    void _physics_process(double delta) override;
    void _draw()                    override;
    void _exit_tree()               override;

    // ------------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------------

    void register_type(const godot::String &key, godot::Object *config);

    int get_active_count() const { return (int)_active.size(); }

    // Returns the TamaServerBullet wrapper, or null if the pool is full.
    godot::Object *spawn(godot::Object *data, godot::Object *config,
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
