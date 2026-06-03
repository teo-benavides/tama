#include "tama_server_bullet_pool.h"
#include "tama_interpreter.h"
#include "tama_manager.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/physics_server2d.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/world2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ---------------------------------------------------------------------------
// _TamaServerBullet binding
// ---------------------------------------------------------------------------

void _TamaServerBullet::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_position"), &_TamaServerBullet::get_position);
    ClassDB::bind_method(D_METHOD("get_angle"),    &_TamaServerBullet::get_angle);
    ClassDB::bind_method(D_METHOD("get_speed"),    &_TamaServerBullet::get_speed);
    ClassDB::bind_method(D_METHOD("get_active"),   &_TamaServerBullet::get_active);
    ClassDB::bind_method(D_METHOD("get_speed_x"),  &_TamaServerBullet::get_speed_x);
    ClassDB::bind_method(D_METHOD("get_speed_y"),  &_TamaServerBullet::get_speed_y);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "", "get_position");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "angle"),    "", "get_angle");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "speed"),    "", "get_speed");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,    "active"),   "", "get_active");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "speed_x"),  "", "get_speed_x");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "speed_y"),  "", "get_speed_y");
}

void _TamaServerBullet::_init_slot(_TamaServerBulletPool *pool, BulletState *state) {
    _pool  = pool;
    _state = state;
    state->wrapper = this;
}

Vector2 _TamaServerBullet::get_position() const { return _state ? _state->position    : Vector2{}; }
float   _TamaServerBullet::get_angle()    const { return _state ? _state->angle       : 0.0f; }
float   _TamaServerBullet::get_speed()    const { return _state ? _state->speed       : 0.0f; }
bool    _TamaServerBullet::get_active()   const { return _state ? _state->active      : false; }
float   _TamaServerBullet::get_speed_x()  const { return _state ? _state->speed_x     : 0.0f; }
float   _TamaServerBullet::get_speed_y()  const { return _state ? _state->speed_y     : 0.0f; }

// ---------------------------------------------------------------------------
// _TamaServerBulletPool — GDExtension binding
// ---------------------------------------------------------------------------

void _TamaServerBulletPool::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_type", "key", "config"),
                         &_TamaServerBulletPool::register_type);
    ClassDB::bind_method(D_METHOD("spawn", "data", "config", "angle", "speed", "position", "context"),
                         &_TamaServerBulletPool::spawn);
    ClassDB::bind_method(D_METHOD("recycle", "bullet_wrapper"),
                         &_TamaServerBulletPool::recycle);
    ClassDB::bind_method(D_METHOD("recycle_all"),
                         &_TamaServerBulletPool::recycle_all);

    ADD_SIGNAL(MethodInfo("bullet_hit",
        PropertyInfo(Variant::OBJECT, "bullet", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "_TamaServerBullet"),
        PropertyInfo(Variant::INT,    "body_instance_id")));
}

// ---------------------------------------------------------------------------
// Destructor — free all GPU resources
// ---------------------------------------------------------------------------

_TamaServerBulletPool::~_TamaServerBulletPool() {
    for (auto &[key, td] : _types) {
        for (auto *b : td->active_batches) delete b;
        for (auto *b : td->free_batches)   delete b;
        // RIDs are not freed in _exit_tree (see comment there); the servers own them.
        for (auto *w : td->wrappers) memdelete(w);
        delete td;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Godot virtuals
// ---------------------------------------------------------------------------

void _TamaServerBulletPool::_ready() {
    set_physics_process(true);
    _composite_threshold = (int)ProjectSettings::get_singleton()
        ->get_setting("tama/server_bullet_composite_threshold", 1000);
    for (auto &pr : _pending_regs)
        register_type(pr.key, pr.config);
    _pending_regs.clear();
}

void _TamaServerBulletPool::_exit_tree() {
    TamaManager *mgr = TamaManager::get_instance();
    if (mgr) mgr->_on_scene_nodes_freed();

    // The pool is a persistent root child — _exit_tree only fires on game exit.
    // The physics/rendering servers destroy all their own state on shutdown, so
    // freeing every shape RID individually here (each a cross-thread call into
    // PhysicsServer2D) is unnecessary and is the main cause of the frozen-window
    // delay when closing via the OS title-bar button.
}

// ---------------------------------------------------------------------------
// Type registration
// ---------------------------------------------------------------------------

void _TamaServerBulletPool::register_type(const String &p_key, Object *p_config) {
    if (!is_inside_tree()) {
        _pending_regs.push_back({p_key, p_config});
        return;
    }
    std::string key = p_key.utf8().get_data();
    if (_types.count(key)) {
        UtilityFunctions::push_warning("_TamaServerBulletPool: type '" + p_key + "' already registered.");
        return;
    }

    TypeData *td = new TypeData();
    td->config_obj     = p_config;

    td->frames         = (TypedArray<Texture2D>)p_config->get("frames");
    td->fps            = (float)p_config->get("fps");
    td->first_texture  = td->frames.size() > 0
        ? Ref<Texture2D>(Object::cast_to<Texture2D>(td->frames[0].operator Object *()))
        : Ref<Texture2D>();
    td->rect           = (Rect2)p_config->get("rect");
    td->texture_scale  = (Vector2)p_config->get("texture_scale");
    td->shape_radius   = (float)p_config->get("shape_radius");
    td->collision_layer = (int)p_config->get("collision_layer");
    td->collision_mask  = (int)p_config->get("collision_mask");
    td->rotates              = (bool)p_config->get("rotates");
    td->face_velocity        = (bool)p_config->get("face_velocity");
    td->pool_size            = (int)p_config->get("pool_size");
    td->out_of_bounds_margin = (float)p_config->get("out_of_bounds_margin");

    int n = td->pool_size;
    _types[key] = td;

    // QuadMesh shared by all batches for this type.
    td->quad.instantiate();
    td->quad->set_size(td->rect.size);

    // Composite multimesh
    td->composite_res.instantiate();
    td->composite_res->set_transform_format(MultiMesh::TRANSFORM_2D);
    td->composite_res->set_mesh(td->quad);
    td->composite = td->composite_res->get_rid();

    // Physics area with N pre-allocated shapes
    RID space = get_world_2d()->get_space();
    td->area = PhysicsServer2D::get_singleton()->area_create();
    PhysicsServer2D::get_singleton()->area_set_space(td->area, space);
    PhysicsServer2D::get_singleton()->area_set_collision_layer(td->area, td->collision_layer);
    PhysicsServer2D::get_singleton()->area_set_collision_mask(td->area, td->collision_mask);
    PhysicsServer2D::get_singleton()->area_set_monitorable(td->area, false);

    td->shapes.reserve(n);
    for (int i = 0; i < n; ++i) {
        RID shape = PhysicsServer2D::get_singleton()->circle_shape_create();
        PhysicsServer2D::get_singleton()->shape_set_data(shape, td->shape_radius);
        PhysicsServer2D::get_singleton()->area_add_shape(td->area, shape);
        PhysicsServer2D::get_singleton()->area_set_shape_disabled(td->area, i, true);
        td->shapes.push_back(shape);
    }

    // Monitor callback — local_shape is the global slot index
    Callable cb = callable_mp_static(_TamaServerBulletPool::_area_monitor_callback)
                      .bind(this, p_key);
    PhysicsServer2D::get_singleton()->area_set_monitor_callback(td->area, cb);

    // Pre-allocate bullet states and wrapper objects
    td->bullets.resize(n);
    td->wrappers.resize(n);
    for (int i = 0; i < n; ++i) {
        td->bullets[i].global_slot = i;
        td->bullets[i].area_rid    = td->area;
        _TamaServerBullet *w = memnew(_TamaServerBullet);
        w->_init_slot(this, &td->bullets[i]);
        td->wrappers[i] = w;
    }

    // FIFO ring: all slots free initially
    td->ring.resize(n);
    for (int i = 0; i < n; ++i) td->ring[i] = i;
    td->ring_r     = 0;
    td->ring_w     = 0;
    td->ring_count = n;
}

// ---------------------------------------------------------------------------
// Area collision callback (static)
// ---------------------------------------------------------------------------

void _TamaServerBulletPool::_area_monitor_callback(
        int status, RID /*body_rid*/, int64_t body_iid,
        int /*body_shape*/, int local_shape,
        _TamaServerBulletPool *self, String type_key)
{
    if (status != PhysicsServer2D::AREA_BODY_ADDED) return;
    std::string key = type_key.utf8().get_data();
    auto it = self->_types.find(key);
    if (it == self->_types.end()) return;
    TypeData *td = it->second;
    if (local_shape < 0 || local_shape >= (int)td->bullets.size()) return;
    BulletState &b = td->bullets[local_shape];
    if (!b.active) return;
    self->emit_signal("bullet_hit", b.wrapper, body_iid);
}

// ---------------------------------------------------------------------------
// Batch helpers
// ---------------------------------------------------------------------------

_TamaServerBulletPool::BatchData *_TamaServerBulletPool::_get_batch(TypeData &td) {
    BatchData *b;
    if (!td.free_batches.empty()) {
        b = td.free_batches.back();
        td.free_batches.pop_back();
    } else {
        b = new BatchData();
        b->multimesh_res.instantiate();
        b->multimesh_res->set_transform_format(MultiMesh::TRANSFORM_2D);
        b->multimesh_res->set_mesh(td.quad);
        b->multimesh_res->set_instance_count(BATCH_CHUNK);
        b->multimesh = b->multimesh_res->get_rid();
        Transform2D offscreen(0.0f, Vector2(-100000.0f, -100000.0f));
        for (int i = 0; i < BATCH_CHUNK; ++i)
            RenderingServer::get_singleton()->multimesh_instance_set_transform_2d(b->multimesh, i, offscreen);
    }
    // Each batch starts at frame 0 of the animation, independent of other batches
    b->anim_time      = 0.0f;
    b->anim_frame     = 0;
    b->current_texture = td.first_texture;
    return b;
}

void _TamaServerBulletPool::_recycle_batch(TypeData &td, BatchData *batch) {
    if (td.cur_batch == batch) td.cur_batch = nullptr;
    Transform2D offscreen(0.0f, Vector2(-100000.0f, -100000.0f));
    for (int i = 0; i < batch->used; ++i)
        RenderingServer::get_singleton()->multimesh_instance_set_transform_2d(batch->multimesh, i, offscreen);
    batch->used         = 0;
    batch->active_count = 0;
    batch->birth_frame  = -1;
    auto &v = td.active_batches;
    v.erase(std::find(v.begin(), v.end(), batch));
    td.free_batches.push_back(batch);
}

// ---------------------------------------------------------------------------
// Spawn
// ---------------------------------------------------------------------------

Object *_TamaServerBulletPool::spawn(
        Object *p_data, Object *p_config,
        float angle, float speed, Vector2 position,
        Object *p_context)
{
    if (!p_data || !p_config) return nullptr;

    // Read bullet_type from BulletFireData
    String bullet_type_str = (String)p_data->get("bullet_type");
    std::string key        = bullet_type_str.utf8().get_data();

    auto it = _types.find(key);
    if (it == _types.end()) {
        // Key not found — fall back to matching by config pointer (used when spawning via default_server_bullet)
        for (auto &[k, td_ptr] : _types) {
            if (td_ptr->config_obj == p_config) { it = _types.find(k); break; }
        }
        if (it == _types.end()) return nullptr;
    }
    TypeData &td = *it->second;
    if (td.ring_count == 0) return nullptr;

    // Allocate global slot from FIFO ring
    int  n           = (int)td.bullets.size();
    int  global_slot = td.ring[td.ring_r];
    td.ring_r        = (td.ring_r + 1) % n;
    --td.ring_count;

    // Get or start this frame's batch
    int64_t frame = Engine::get_singleton()->get_physics_frames();
    if (!td.cur_batch || td.cur_frame != frame || td.cur_batch->used >= BATCH_CHUNK) {
        td.cur_batch             = _get_batch(td);
        td.cur_batch->birth_frame = (int)frame;
        td.cur_frame             = frame;
        td.active_batches.push_back(td.cur_batch);
    }
    BatchData *batch  = td.cur_batch;
    int local_slot    = batch->used;
    ++batch->used;
    ++batch->active_count;

    // Init bullet state
    BulletState &b   = td.bullets[global_slot];
    b.active         = true;
    b.active_idx     = (int32_t)_active.size();
    _active.push_back(&b);

    b.multimesh_rid     = batch->multimesh;
    b.batch_ptr         = batch;   // direct pointer — immune to active_batches reordering
    b.local_slot        = local_slot;

    b.position          = position;
    b.initial_position  = position;
    b.angle             = angle;
    b.speed             = speed;
    b.speed_x           = 0.0f;
    b.speed_y           = 0.0f;
    b.rotates              = td.rotates;
    b.face_velocity        = td.face_velocity;
    b.out_of_bounds_margin = td.out_of_bounds_margin;
    b.texture_scale     = Vector2(td.texture_scale.x, -td.texture_scale.y); // negate Y for QuadMesh UV convention
    b.last_angle        = angle;
    b.last_speed        = speed;
    b.angle_tween.active = false;
    b.speed_tween.active = false;
    b.pos_tween.active   = false;
    b.sx_tween.active    = false;
    b.sy_tween.active    = false;
    b.vel_dirty          = true;

    // mvmt
    b.mvmt_x_set  = (bool)p_data->get("mvmt_x_set");
    b.mvmt_y_set  = (bool)p_data->get("mvmt_y_set");
    b.mvmt_x_type = (int)p_data->get("mvmt_x_type");
    b.mvmt_y_type = (int)p_data->get("mvmt_y_type");
    b.mvmt_x_chunk = nullptr;
    b.mvmt_y_chunk = nullptr;

    if (b.mvmt_x_set || b.mvmt_y_set) {
        // Build mvmt scope from bullet_params/args + spawn_x/y
        Array params = (Array)p_data->get("bullet_params");
        Array args   = (Array)p_data->get("bullet_args");
        int   n_args = std::min(params.size(), args.size());

        std::vector<std::string> var_names;
        b.mvmt_values.clear();
        var_names.reserve(n_args + 2);
        b.mvmt_values.reserve(n_args + 2);

        for (int i = 0; i < n_args; ++i) {
            var_names.push_back(((String)params[i]).utf8().get_data());
            b.mvmt_values.push_back((double)args[i]);
        }
        var_names.push_back("spawn_x");
        b.mvmt_values.push_back((double)position.x);
        var_names.push_back("spawn_y");
        b.mvmt_values.push_back((double)position.y);

        // Build cache key
        auto make_key = [&](const std::string &expr) {
            std::string k = expr + "|";
            for (size_t i = 0; i < var_names.size(); ++i) {
                if (i > 0) k += ',';
                k += var_names[i];
            }
            return k;
        };

        _TamaExprRuntime *er = _TamaExprRuntime::get_singleton();
        if (er) {
            if (b.mvmt_x_set) {
                std::string expr = ((String)p_data->get("mvmt_x_expr")).utf8().get_data();
                b.mvmt_x_chunk   = er->get_chunk(expr, var_names, make_key(expr));
            }
            if (b.mvmt_y_set) {
                std::string expr = ((String)p_data->get("mvmt_y_expr")).utf8().get_data();
                b.mvmt_y_chunk   = er->get_chunk(expr, var_names, make_key(expr));
            }
        }
    }

    // Initial render transform
    float rot = b.rotates ? b.angle : 0.0f;
    RenderingServer::get_singleton()->multimesh_instance_set_transform_2d(
        batch->multimesh, local_slot,
        Transform2D(rot, b.texture_scale, 0.0f, position));

    // Physics transform
    PhysicsServer2D::get_singleton()->area_set_shape_transform(
        td.area, global_slot, Transform2D(0.0f, position));
    PhysicsServer2D::get_singleton()->area_set_shape_disabled(td.area, global_slot, false);

    // bullet_act runner — C++ _TamaInterpreter (stepped in _physics_process)
    b.runner = nullptr;
    Variant bullet_act_v = p_data->get("bullet_act");
    bool has_act = bullet_act_v.get_type() != Variant::NIL &&
                   bullet_act_v.get_type() != Variant::OBJECT ||
                   (bullet_act_v.get_type() == Variant::OBJECT &&
                    bullet_act_v.operator Object *() != nullptr);
    // Simplify: just check for non-null Object
    Object *bullet_act_obj = nullptr;
    if (bullet_act_v.get_type() == Variant::OBJECT)
        bullet_act_obj = bullet_act_v.operator Object *();
    has_act = (bullet_act_obj != nullptr);

    if (has_act) {
        _TamaInterpreter *runner = memnew(_TamaInterpreter);
        runner->set_context(p_context);
        b.runner = runner;

        // Build act scope
        Dictionary act_scope;
        Array params = (Array)p_data->get("bullet_params");
        Array args   = (Array)p_data->get("bullet_args");
        int   na     = std::min(params.size(), args.size());
        for (int i = 0; i < na; ++i)
            act_scope[(String)params[i]] = args[i];
        act_scope["spawn_x"] = position.x;
        act_scope["spawn_y"] = position.y;

        // Connect runner signals — bind wrapper Object* for state resolution
        Object *wrapper_obj = b.wrapper;
        runner->connect("changed_direction",
            callable_mp(this, &_TamaServerBulletPool::_on_changed_direction).bind(wrapper_obj));
        runner->connect("changed_speed",
            callable_mp(this, &_TamaServerBulletPool::_on_changed_speed).bind(wrapper_obj));
        runner->connect("changed_position",
            callable_mp(this, &_TamaServerBulletPool::_on_changed_position).bind(wrapper_obj));
        runner->connect("accelerated",
            callable_mp(this, &_TamaServerBulletPool::_on_accelerated).bind(wrapper_obj));
        runner->connect("vanished",
            callable_mp(this, &_TamaServerBulletPool::recycle).bind(wrapper_obj));

        // Allow server bullets to fire child bullets by connecting bullet_fired to the spawn manager.
        TamaManager *mgr = TamaManager::get_instance();
        _TamaSpawnManager *sm = mgr ? mgr->_get_spawn_manager() : nullptr;
        if (sm) sm->connect_interpreter(runner, b.wrapper);

        Object *source_program = Object::cast_to<Object>(p_data->get("source_program"));
        runner->start_act(source_program, bullet_act_obj, act_scope);
    }

    return b.wrapper;
}

// ---------------------------------------------------------------------------
// Recycle
// ---------------------------------------------------------------------------

void _TamaServerBulletPool::recycle(Object *p_wrapper) {
    if (!p_wrapper) return;
    _TamaServerBullet *w = Object::cast_to<_TamaServerBullet>(p_wrapper);
    if (!w || !w->_state) return;
    _recycle_internal(w->_state);
}

void _TamaServerBulletPool::recycle_all() {
    std::vector<BulletState *> copy = _active;
    for (auto *b : copy) _recycle_internal(b);
    // Force one more _draw() so Godot clears the persistent canvas draw commands
    // from the last frame. Without this, the multimesh draw calls linger on screen
    // because _physics_process only calls queue_redraw() when _active is non-empty.
    queue_redraw();
}

void _TamaServerBulletPool::_recycle_internal(BulletState *b) {
    if (!b || !b->active) return;
    b->active = false;

    // Hide in multimesh
    Transform2D offscreen(0.0f, Vector2(-100000.0f, -100000.0f));
    RenderingServer::get_singleton()->multimesh_instance_set_transform_2d(
        b->multimesh_rid, b->local_slot, offscreen);

    // Find which TypeData owns this slot (search by area_rid)
    TypeData *td_owner = nullptr;
    std::string td_key;
    for (auto &[k, td] : _types) {
        if (td->area == b->area_rid) { td_owner = td; td_key = k; break; }
    }
    if (td_owner) {
        // Decrement batch counter; recycle batch if empty.
        // Use the direct BatchData* pointer — not an index, which would break when
        // other batches are removed from active_batches and shift the indices.
        if (b->batch_ptr) {
            BatchData *batch = static_cast<BatchData *>(b->batch_ptr);
            --batch->active_count;
            if (batch->active_count == 0) _recycle_batch(*td_owner, batch);
            b->batch_ptr = nullptr;
        }
        // Return global slot to ring
        int ring_n = (int)td_owner->ring.size();
        td_owner->ring[td_owner->ring_w] = b->global_slot;
        td_owner->ring_w = (td_owner->ring_w + 1) % ring_n;
        ++td_owner->ring_count;
    }

    PhysicsServer2D::get_singleton()->area_set_shape_disabled(b->area_rid, b->global_slot, true);

    // Clean up runner
    if (b->runner) {
        _TamaInterpreter *ti = Object::cast_to<_TamaInterpreter>(b->runner);
        if (ti) {
            ti->stop();
            ti->queue_free();
        }
        b->runner = nullptr;
    }

    // O(1) swap-erase from _active
    int32_t idx  = b->active_idx;
    BulletState *last = _active.back();
    _active[idx]     = last;
    last->active_idx = idx;
    _active.pop_back();
    b->active_idx = -1;
}

// ---------------------------------------------------------------------------
// Physics process
// ---------------------------------------------------------------------------

void _TamaServerBulletPool::_physics_process(double p_delta) {
    float delta = (float)p_delta;
    Rect2 world = _world_bounds();
    TamaManager *mgr = TamaManager::get_instance();
    float global_margin = mgr ? mgr->get_global_out_of_bounds_margin() : -1.0f;
    godot::Object *ctx  = mgr ? mgr->_get_context() : nullptr;
    _TamaExprRuntime *er = _TamaExprRuntime::get_singleton();

    _to_recycle.clear();

    // Snapshot the size before the loop. spawn() called from a runner's bullet_fired signal
    // appends to _active (may reallocate), and _recycle_internal() from a vanished signal
    // does swap-erase. An index-based loop with a size snapshot handles both safely:
    // new spawns are appended past n (processed next frame), recycled bullets are skipped
    // via the b->active guard below.
    const int n = (int)_active.size();
    for (int i = 0; i < n; ++i) {
        BulletState *b = _active[i];
        // Step runner before this bullet's position update so script changes
        // take effect in the same frame they are issued.
        if (b->runner) {
            _TamaInterpreter *ti = Object::cast_to<_TamaInterpreter>(b->runner);
            if (ti && ti->is_running()) ti->step(delta);
        }

        // The runner may have recycled this bullet (e.g. vanish command). Skip it.
        if (!b->active) continue;

        // Mark velocity dirty when any relevant tween is active this frame.
        bool tween_was_active = b->angle_tween.active || b->speed_tween.active
                             || b->sx_tween.active    || b->sy_tween.active;
        _step_tweens(*b, delta);
        if (tween_was_active) b->vel_dirty = true;

        Vector2 pos_before = b->position;

        if (b->mvmt_x_set || b->mvmt_y_set) {
            const double *vals = b->mvmt_values.data();
            size_t        cnt  = b->mvmt_values.size();
            if (b->mvmt_x_set && b->mvmt_x_chunk && er) {
                double vx = er->eval_chunk(*b->mvmt_x_chunk, vals, cnt, ctx);
                b->position.x = (b->mvmt_x_type == 0) // ABS
                    ? (float)vx
                    : b->initial_position.x + (float)vx;
            }
            if (b->mvmt_y_set && b->mvmt_y_chunk && er) {
                double vy = er->eval_chunk(*b->mvmt_y_chunk, vals, cnt, ctx);
                b->position.y = (b->mvmt_y_type == 0) // ABS
                    ? (float)vy
                    : b->initial_position.y + (float)vy;
            }
        } else {
            // Recompute cached velocity only when dirty (angle/speed/speed_x/speed_y changed).
            if (b->vel_dirty) {
                b->cached_vx = std::cos(b->angle) * b->speed + b->speed_x;
                b->cached_vy = std::sin(b->angle) * b->speed + b->speed_y;
                b->vel_dirty = false;
            }
            b->position.x += b->cached_vx * delta;
            b->position.y += b->cached_vy * delta;
        }

        float rot = 0.0f;
        if (b->rotates) {
            if (b->face_velocity) {
                if (b->mvmt_x_set || b->mvmt_y_set) {
                    Vector2 dp = b->position - pos_before;
                    rot = dp.length_squared() > 1e-8f ? std::atan2(dp.y, dp.x) : b->angle;
                } else {
                    // Reuse cached velocity — no extra trig needed.
                    rot = (b->cached_vx != 0.0f || b->cached_vy != 0.0f)
                        ? std::atan2(b->cached_vy, b->cached_vx) : b->angle;
                }
            } else {
                rot = b->angle;
            }
        }
        RenderingServer::get_singleton()->multimesh_instance_set_transform_2d(
            b->multimesh_rid, b->local_slot,
            Transform2D(rot, b->texture_scale, 0.0f, b->position));
        PhysicsServer2D::get_singleton()->area_set_shape_transform(
            b->area_rid, b->global_slot, Transform2D(0.0f, b->position));

        float m = (global_margin >= 0.0f) ? global_margin : b->out_of_bounds_margin;
        if (b->position.x < world.position.x - m ||
            b->position.y < world.position.y - m ||
            b->position.x > world.get_end().x   + m ||
            b->position.y > world.get_end().y   + m)
            _to_recycle.push_back(b);
    }

    for (BulletState *b : _to_recycle)
        _recycle_internal(b);

    // Advance per-batch sprite animations (each batch has its own independent frame offset)
    for (auto &[key, td] : _types) {
        if (td->frames.size() <= 1 || td->fps <= 0.0f) continue;
        float frame_dur = 1.0f / td->fps;
        for (BatchData *batch : td->active_batches) {
            batch->anim_time += delta;
            while (batch->anim_time >= frame_dur) {
                batch->anim_time -= frame_dur;
                batch->anim_frame = (batch->anim_frame + 1) % (int)td->frames.size();
                batch->current_texture = Ref<Texture2D>(
                    Object::cast_to<Texture2D>(td->frames[batch->anim_frame].operator Object *()));
            }
        }
    }

    if (!_active.empty())
        queue_redraw();
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void _TamaServerBulletPool::_draw() {
    for (auto &[key, td] : _types) {
        if (td->active_batches.empty()) continue;
        bool animated = td->fps > 0.0f && td->frames.size() > 1;
        if (animated || (int)_active.size() <= _composite_threshold) {
            // Animated types always draw per-batch (each batch has a different texture).
            // Static types use per-batch draws when below the composite threshold.
            for (BatchData *batch : td->active_batches)
                draw_multimesh(batch->multimesh_res, batch->current_texture);
        } else {
            // Composite: concatenate all batch transforms into one draw call.
            // Only reached for static (non-animated) types above the threshold.
            // _composite_buf is a persistent member — resize never reallocates downward,
            // so it reaches steady-state capacity quickly and stays there.
            int total_floats = 0;
            for (BatchData *batch : td->active_batches)
                total_floats += batch->used * 8;
            if (total_floats == 0) continue;

            _composite_buf.resize(total_floats);
            float *dst = _composite_buf.ptrw();
            int offset = 0;
            for (BatchData *batch : td->active_batches) {
                if (batch->used <= 0) continue;
                PackedFloat32Array src = RenderingServer::get_singleton()
                    ->multimesh_get_buffer(batch->multimesh);
                int count = batch->used * 8;
                if (src.size() >= count) {
                    std::memcpy(dst + offset, src.ptr(), count * sizeof(float));
                    offset += count;
                }
            }
            if (offset == 0) continue;
            td->composite_res->set_instance_count(offset / 8);
            RenderingServer::get_singleton()->multimesh_set_buffer(td->composite, _composite_buf);
            draw_multimesh(td->composite_res, td->first_texture);
        }
    }
}

// ---------------------------------------------------------------------------
// Tween stepping
// ---------------------------------------------------------------------------

void _TamaServerBulletPool::_step_tweens(BulletState &b, float delta) {
    if (b.angle_tween.active)  b.angle   = b.angle_tween.step(delta);
    if (b.speed_tween.active)  b.speed   = b.speed_tween.step(delta);
    if (b.pos_tween.active)    b.position = b.pos_tween.step(delta);
    if (b.sx_tween.active)     b.speed_x = b.sx_tween.step(delta);
    if (b.sy_tween.active)     b.speed_y = b.sy_tween.step(delta);
}

// ---------------------------------------------------------------------------
// Signal handlers
// ---------------------------------------------------------------------------

// DirType: AIM=0, ABS=1, REL=2, SEQ=3 — ValueType: ABS=0, REL=1, SEQ=2

float _TamaServerBulletPool::_dir_to_angle(const BulletState &b, int dir_type, float value) const {
    // value is in degrees (interpreter emits raw expression results, no pre-conversion)
    static const float DEG2RAD = 3.14159265f / 180.0f;
    float rad = value * DEG2RAD;
    switch (dir_type) {
    case 0: { // AIM — rad is an additive offset to the aim angle
        TamaManager *mgr = TamaManager::get_instance();
        if (mgr) {
            return (mgr->get_player_position() - b.position).angle() + rad;
        }
        return rad;
    }
    case 1: return rad;                    // ABS
    case 2: return b.angle + rad;          // REL
    case 3: return b.last_angle + rad;     // SEQ
    }
    return rad;
}

float _TamaServerBulletPool::_spd_to_value(const BulletState &b, int speed_type, float value) const {
    switch (speed_type) {
    case 0: return value;                  // ABS
    case 1: return b.speed + value;        // REL
    case 2: return b.last_speed + value;   // SEQ
    }
    return value;
}

float _TamaServerBulletPool::_accel_axis_end(int axis_type, float value, float current, float over) const {
    switch (axis_type) {
    case 0: return value;                          // ABS
    case 1: return (value - current) * over;       // REL
    case 2: return value;                          // SEQ
    }
    return value;
}

// NOTE: signal handlers take (signal_data, wrapper_object) because Callable::bind()
// appends the bound args after the signal args.
// We resolve BulletState via the wrapper's public _state pointer.

#define RESOLVE_BULLET(wrapper_param)                                    \
    _TamaServerBullet *w_ = Object::cast_to<_TamaServerBullet>(wrapper_param); \
    if (!w_ || !w_->_state) return;                                      \
    BulletState &b = *w_->_state;

void _TamaServerBulletPool::_on_changed_direction(Object *p_data, Object *p_wrapper) {
    RESOLVE_BULLET(p_wrapper)
    int   dir_type  = (int)p_data->get("dir_type");
    float dir_value = (float)p_data->get("dir_value");
    float over      = (float)p_data->get("over");
    float target    = _dir_to_angle(b, dir_type, dir_value);
    b.last_angle    = b.angle;
    if (over <= 0.0f) {
        b.angle = target;
        b.angle_tween.active = false;
        b.vel_dirty = true;
    } else {
        b.angle_tween = { true, b.angle, target, 0.0f, over };
    }
}

void _TamaServerBulletPool::_on_changed_speed(Object *p_data, Object *p_wrapper) {
    RESOLVE_BULLET(p_wrapper)
    int   speed_type  = (int)p_data->get("speed_type");
    float speed_value = (float)p_data->get("speed_value");
    float over        = (float)p_data->get("over");
    float target      = _spd_to_value(b, speed_type, speed_value);
    b.last_speed      = b.speed;
    if (over <= 0.0f) {
        b.speed = target;
        b.speed_tween.active = false;
        b.vel_dirty = true;
    } else {
        b.speed_tween = { true, b.speed, target, 0.0f, over };
    }
}

void _TamaServerBulletPool::_on_changed_position(Object *p_data, Object *p_wrapper) {
    RESOLVE_BULLET(p_wrapper)
    bool    has_x  = (bool)p_data->get("has_x");
    bool    has_y  = (bool)p_data->get("has_y");
    float   over   = (float)p_data->get("over");
    Vector2 target = b.position;
    if (has_x) {
        int   x_type = (int)p_data->get("x_type");
        float x      = (float)p_data->get("x");
        switch (x_type) {
        case 0: case 2: target.x  = x; break;   // ABS, SEQ
        case 1:         target.x += x; break;    // REL
        }
    }
    if (has_y) {
        int   y_type = (int)p_data->get("y_type");
        float y      = (float)p_data->get("y");
        switch (y_type) {
        case 0: case 2: target.y  = y; break;
        case 1:         target.y += y; break;
        }
    }
    if (over <= 0.0f) {
        b.position = target;
        b.pos_tween.active = false;
    } else {
        b.pos_tween = { true, b.position, target, 0.0f, over };
    }
}

void _TamaServerBulletPool::_on_accelerated(Object *p_data, Object *p_wrapper) {
    RESOLVE_BULLET(p_wrapper)
    bool  has_x = (bool)p_data->get("has_x");
    bool  has_y = (bool)p_data->get("has_y");
    float over  = (float)p_data->get("over");
    if (over <= 0.0f) {
        if (has_x) {
            int   x_type = (int)p_data->get("x_type");
            float x      = (float)p_data->get("x");
            if (x_type == 1) b.speed_x += x; else b.speed_x = x;
        }
        if (has_y) {
            int   y_type = (int)p_data->get("y_type");
            float y      = (float)p_data->get("y");
            if (y_type == 1) b.speed_y += y; else b.speed_y = y;
        }
        b.vel_dirty = true;
    } else {
        if (has_x) {
            float end_x = _accel_axis_end((int)p_data->get("x_type"), (float)p_data->get("x"), b.speed_x, over);
            b.sx_tween = { true, b.speed_x, end_x, 0.0f, over };
        }
        if (has_y) {
            float end_y = _accel_axis_end((int)p_data->get("y_type"), (float)p_data->get("y"), b.speed_y, over);
            b.sy_tween = { true, b.speed_y, end_y, 0.0f, over };
        }
    }
}

// ---------------------------------------------------------------------------
// World bounds
// ---------------------------------------------------------------------------

Rect2 _TamaServerBulletPool::_world_bounds() const {
    Viewport *vp  = get_viewport();
    if (!vp) return Rect2(-10000, -10000, 20000, 20000);
    Transform2D inv = vp->get_canvas_transform().affine_inverse();
    Rect2 vp_rect   = vp->get_visible_rect();
    Vector2 tl = inv.xform(vp_rect.position);
    Vector2 br = inv.xform(vp_rect.get_end());
    return Rect2(
        std::min(tl.x, br.x), std::min(tl.y, br.y),
        std::abs(br.x - tl.x), std::abs(br.y - tl.y));
}
