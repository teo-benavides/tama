#include "tama_server_bullet_pool.h"
#include "tama_interpreter.h"

#include <algorithm>
#include <cmath>

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
// TamaServerBullet binding
// ---------------------------------------------------------------------------

void TamaServerBullet::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_position"), &TamaServerBullet::get_position);
    ClassDB::bind_method(D_METHOD("get_angle"),    &TamaServerBullet::get_angle);
    ClassDB::bind_method(D_METHOD("get_speed"),    &TamaServerBullet::get_speed);
    ClassDB::bind_method(D_METHOD("get_active"),   &TamaServerBullet::get_active);
    ClassDB::bind_method(D_METHOD("get_speed_x"),  &TamaServerBullet::get_speed_x);
    ClassDB::bind_method(D_METHOD("get_speed_y"),  &TamaServerBullet::get_speed_y);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "", "get_position");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "angle"),    "", "get_angle");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "speed"),    "", "get_speed");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,    "active"),   "", "get_active");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "speed_x"),  "", "get_speed_x");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,   "speed_y"),  "", "get_speed_y");
}

void TamaServerBullet::_init_slot(TamaServerBulletPool *pool, BulletState *state) {
    _pool  = pool;
    _state = state;
    state->wrapper = this;
}

Vector2 TamaServerBullet::get_position() const { return _state ? _state->position    : Vector2{}; }
float   TamaServerBullet::get_angle()    const { return _state ? _state->angle       : 0.0f; }
float   TamaServerBullet::get_speed()    const { return _state ? _state->speed       : 0.0f; }
bool    TamaServerBullet::get_active()   const { return _state ? _state->active      : false; }
float   TamaServerBullet::get_speed_x()  const { return _state ? _state->speed_x     : 0.0f; }
float   TamaServerBullet::get_speed_y()  const { return _state ? _state->speed_y     : 0.0f; }

// ---------------------------------------------------------------------------
// TamaServerBulletPool — GDExtension binding
// ---------------------------------------------------------------------------

void TamaServerBulletPool::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_type", "key", "config"),
                         &TamaServerBulletPool::register_type);
    ClassDB::bind_method(D_METHOD("spawn", "data", "config", "angle", "speed", "position", "context"),
                         &TamaServerBulletPool::spawn);
    ClassDB::bind_method(D_METHOD("recycle", "bullet_wrapper"),
                         &TamaServerBulletPool::recycle);
    ClassDB::bind_method(D_METHOD("recycle_all"),
                         &TamaServerBulletPool::recycle_all);

    ClassDB::bind_method(D_METHOD("set_bounds_margin", "v"), &TamaServerBulletPool::set_bounds_margin);
    ClassDB::bind_method(D_METHOD("get_bounds_margin"),       &TamaServerBulletPool::get_bounds_margin);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bounds_margin"), "set_bounds_margin", "get_bounds_margin");

    ADD_SIGNAL(MethodInfo("bullet_hit",
        PropertyInfo(Variant::OBJECT, "bullet", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "TamaServerBullet"),
        PropertyInfo(Variant::INT,    "body_instance_id")));
}

// ---------------------------------------------------------------------------
// Destructor — free all GPU resources
// ---------------------------------------------------------------------------

TamaServerBulletPool::~TamaServerBulletPool() {
    for (auto &[key, td] : _types) {
        for (auto *b : td->active_batches) delete b;
        for (auto *b : td->free_batches)   delete b;
        // RIDs are freed in _exit_tree
        for (auto *w : td->wrappers) memdelete(w);
        delete td;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// GDScript autoloads are scene-tree nodes, not Engine singletons.
// Engine::get_singleton("TamaManager") prints an error and returns null for them.
static Node *_get_tama_manager(SceneTree *tree) {
    if (!tree || !tree->get_root()) return nullptr;
    return tree->get_root()->get_node_or_null(NodePath("/root/TamaManager"));
}

// ---------------------------------------------------------------------------
// Godot virtuals
// ---------------------------------------------------------------------------

void TamaServerBulletPool::_ready() {
    set_physics_process(true);
    _composite_threshold = (int)ProjectSettings::get_singleton()
        ->get_setting("tama/server_bullet_composite_threshold", 1000);
}

void TamaServerBulletPool::_exit_tree() {
    for (auto &[key, td] : _types) {
        for (auto rid : td->shapes)
            PhysicsServer2D::get_singleton()->free_rid(rid);
        PhysicsServer2D::get_singleton()->free_rid(td->area);
    }
}

// ---------------------------------------------------------------------------
// Type registration
// ---------------------------------------------------------------------------

void TamaServerBulletPool::register_type(const String &p_key, Object *p_config) {
    std::string key = p_key.utf8().get_data();
    if (_types.count(key)) {
        UtilityFunctions::push_warning("TamaServerBulletPool: type '" + p_key + "' already registered.");
        return;
    }

    TypeData *td = new TypeData();

    // Read config properties from GDScript Resource via Object::get()
    td->texture        = Ref<Texture2D>(Object::cast_to<Texture2D>(p_config->get("texture")));
    td->rect           = (Rect2)p_config->get("rect");
    td->texture_scale  = (Vector2)p_config->get("texture_scale");
    td->shape_radius   = (float)p_config->get("shape_radius");
    td->collision_layer = (int)p_config->get("collision_layer");
    td->collision_mask  = (int)p_config->get("collision_mask");
    td->rotates        = (bool)p_config->get("rotates");
    td->pool_size      = (int)p_config->get("pool_size");

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
    Callable cb = callable_mp_static(TamaServerBulletPool::_area_monitor_callback)
                      .bind(this, p_key);
    PhysicsServer2D::get_singleton()->area_set_monitor_callback(td->area, cb);

    // Pre-allocate bullet states and wrapper objects
    td->bullets.resize(n);
    td->wrappers.resize(n);
    for (int i = 0; i < n; ++i) {
        td->bullets[i].global_slot = i;
        td->bullets[i].area_rid    = td->area;
        TamaServerBullet *w = memnew(TamaServerBullet);
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

void TamaServerBulletPool::_area_monitor_callback(
        int status, RID /*body_rid*/, int64_t body_iid,
        int /*body_shape*/, int local_shape,
        TamaServerBulletPool *self, String type_key)
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

TamaServerBulletPool::BatchData *TamaServerBulletPool::_get_batch(TypeData &td) {
    if (!td.free_batches.empty()) {
        BatchData *b = td.free_batches.back();
        td.free_batches.pop_back();
        return b;
    }
    BatchData *b = new BatchData();
    b->multimesh_res.instantiate();
    b->multimesh_res->set_transform_format(MultiMesh::TRANSFORM_2D);
    b->multimesh_res->set_mesh(td.quad);
    b->multimesh_res->set_instance_count(BATCH_CHUNK);
    b->multimesh = b->multimesh_res->get_rid();
    // Place all instances off-screen initially
    Transform2D offscreen(0.0f, Vector2(-100000.0f, -100000.0f));
    for (int i = 0; i < BATCH_CHUNK; ++i)
        RenderingServer::get_singleton()->multimesh_instance_set_transform_2d(b->multimesh, i, offscreen);
    return b;
}

void TamaServerBulletPool::_recycle_batch(TypeData &td, BatchData *batch) {
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

Object *TamaServerBulletPool::spawn(
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
        UtilityFunctions::push_warning("TamaServerBulletPool: type '" + bullet_type_str + "' not registered.");
        return nullptr;
    }
    TypeData &td = *it->second;
    if (td.ring_count == 0) {
        UtilityFunctions::push_warning("TamaServerBulletPool: pool full for type '" + bullet_type_str + "'.");
        return nullptr;
    }

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
    b.rotates           = td.rotates;
    // Negate Y for QuadMesh UV convention (matches GDScript version)
    b.texture_scale     = Vector2(td.texture_scale.x, -td.texture_scale.y);
    b.last_angle        = angle;
    b.last_speed        = speed;
    b.angle_tween.active = false;
    b.speed_tween.active = false;
    b.pos_tween.active   = false;
    b.sx_tween.active    = false;
    b.sy_tween.active    = false;

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

        TamaExprRuntime *er = TamaExprRuntime::get_singleton();
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

    // bullet_act runner — C++ TamaInterpreter (stepped in _physics_process)
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
        TamaInterpreter *runner = memnew(TamaInterpreter);
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
            callable_mp(this, &TamaServerBulletPool::_on_changed_direction).bind(wrapper_obj));
        runner->connect("changed_speed",
            callable_mp(this, &TamaServerBulletPool::_on_changed_speed).bind(wrapper_obj));
        runner->connect("changed_position",
            callable_mp(this, &TamaServerBulletPool::_on_changed_position).bind(wrapper_obj));
        runner->connect("accelerated",
            callable_mp(this, &TamaServerBulletPool::_on_accelerated).bind(wrapper_obj));
        runner->connect("vanished",
            callable_mp(this, &TamaServerBulletPool::recycle).bind(wrapper_obj));

        Object *source_program = Object::cast_to<Object>(p_data->get("source_program"));
        runner->start_act(source_program, bullet_act_obj, act_scope);
    }

    return b.wrapper;
}

// ---------------------------------------------------------------------------
// Recycle
// ---------------------------------------------------------------------------

void TamaServerBulletPool::recycle(Object *p_wrapper) {
    if (!p_wrapper) return;
    TamaServerBullet *w = Object::cast_to<TamaServerBullet>(p_wrapper);
    if (!w || !w->_state) return;
    _recycle_internal(w->_state);
}

void TamaServerBulletPool::recycle_all() {
    // Copy to avoid modifying while iterating
    std::vector<BulletState *> copy = _active;
    for (auto *b : copy) _recycle_internal(b);
}

void TamaServerBulletPool::_recycle_internal(BulletState *b) {
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
        TamaInterpreter *ti = Object::cast_to<TamaInterpreter>(b->runner);
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

void TamaServerBulletPool::_physics_process(double p_delta) {
    float delta   = (float)p_delta;
    Rect2 bounds  = _world_bounds().grow(_bounds_margin);
    TamaExprRuntime *er = TamaExprRuntime::get_singleton();

    // Step all bullet act runners before updating positions
    for (BulletState *b : _active) {
        if (b->runner) {
            TamaInterpreter *ti = Object::cast_to<TamaInterpreter>(b->runner);
            if (ti && ti->is_running()) ti->step(delta);
        }
    }

    std::vector<BulletState *> to_recycle;

    for (BulletState *b : _active) {
        _step_tweens(*b, delta);

        if (b->mvmt_x_set || b->mvmt_y_set) {
            const double *vals = b->mvmt_values.data();
            size_t        cnt  = b->mvmt_values.size();
            if (b->mvmt_x_set && b->mvmt_x_chunk && er) {
                double vx = er->eval_chunk(*b->mvmt_x_chunk, vals, cnt);
                b->position.x = (b->mvmt_x_type == 0) // ABS
                    ? (float)vx
                    : b->initial_position.x + (float)vx;
            }
            if (b->mvmt_y_set && b->mvmt_y_chunk && er) {
                double vy = er->eval_chunk(*b->mvmt_y_chunk, vals, cnt);
                b->position.y = (b->mvmt_y_type == 0) // ABS
                    ? (float)vy
                    : b->initial_position.y + (float)vy;
            }
        } else {
            float cx = std::cos(b->angle), cy = std::sin(b->angle);
            b->position.x += (cx * b->speed + b->speed_x) * delta;
            b->position.y += (cy * b->speed + b->speed_y) * delta;
        }

        float rot = b->rotates ? b->angle : 0.0f;
        RenderingServer::get_singleton()->multimesh_instance_set_transform_2d(
            b->multimesh_rid, b->local_slot,
            Transform2D(rot, b->texture_scale, 0.0f, b->position));
        PhysicsServer2D::get_singleton()->area_set_shape_transform(
            b->area_rid, b->global_slot, Transform2D(0.0f, b->position));

        if (!bounds.has_point(b->position))
            to_recycle.push_back(b);
    }

    for (BulletState *b : to_recycle)
        _recycle_internal(b);

    if (!_active.empty())
        queue_redraw();
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void TamaServerBulletPool::_draw() {
    for (auto &[key, td] : _types) {
        if (td->active_batches.empty()) continue;
        if ((int)_active.size() <= _composite_threshold) {
            for (BatchData *batch : td->active_batches)
                draw_multimesh(batch->multimesh_res, td->texture);
        } else {
            // Composite: concatenate all batch buffers
            PackedFloat32Array buf;
            for (BatchData *batch : td->active_batches) {
                if (batch->used <= 0) continue;
                PackedFloat32Array src = RenderingServer::get_singleton()
                    ->multimesh_get_buffer(batch->multimesh);
                // Each instance: 8 floats for TRANSFORM_2D
                int byte_count = batch->used * 8;
                if (src.size() >= byte_count) {
                    for (int i = 0; i < byte_count; ++i)
                        buf.push_back(src[i]);
                }
            }
            if (buf.is_empty()) continue;
            td->composite_res->set_instance_count(buf.size() / 8);
            RenderingServer::get_singleton()->multimesh_set_buffer(td->composite, buf);
            draw_multimesh(td->composite_res, td->texture);
        }
    }
}

// ---------------------------------------------------------------------------
// Tween stepping
// ---------------------------------------------------------------------------

void TamaServerBulletPool::_step_tweens(BulletState &b, float delta) {
    if (b.angle_tween.active)  b.angle   = b.angle_tween.step(delta);
    if (b.speed_tween.active)  b.speed   = b.speed_tween.step(delta);
    if (b.pos_tween.active)    b.position = b.pos_tween.step(delta);
    if (b.sx_tween.active)     b.speed_x = b.sx_tween.step(delta);
    if (b.sy_tween.active)     b.speed_y = b.sy_tween.step(delta);
}

// ---------------------------------------------------------------------------
// Signal handlers from GDScript runners
// ---------------------------------------------------------------------------

// DirType enum values from tama_ast.gd: AIM=0, ABS=1, REL=2, SEQ=3
// ValueType: ABS=0, REL=1, SEQ=2

float TamaServerBulletPool::_dir_to_angle(const BulletState &b, int dir_type, float value) const {
    // AIM=0: player_pos - bullet_pos angle + offset
    // ABS=1: value in radians (already converted by interpreter)
    // REL=2: current angle + offset
    // SEQ=3: last_angle + offset
    // Note: value is already in radians (interpreter calls deg_to_rad)
    switch (dir_type) {
    case 0: { // AIM
        Object *mgr = _get_tama_manager(get_tree());
        if (mgr) {
            Vector2 pp = (Vector2)mgr->get("player_position");
            return (pp - b.position).angle() + value;
        }
        return value;
    }
    case 1: return value;          // ABS
    case 2: return b.angle + value; // REL
    case 3: return b.last_angle + value; // SEQ
    }
    return value;
}

float TamaServerBulletPool::_spd_to_value(const BulletState &b, int speed_type, float value) const {
    switch (speed_type) {
    case 0: return value;                  // ABS
    case 1: return b.speed + value;        // REL
    case 2: return b.last_speed + value;   // SEQ
    }
    return value;
}

float TamaServerBulletPool::_accel_axis_end(int axis_type, float value, float current, float over) const {
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
    TamaServerBullet *w_ = Object::cast_to<TamaServerBullet>(wrapper_param); \
    if (!w_ || !w_->_state) return;                                      \
    BulletState &b = *w_->_state;

void TamaServerBulletPool::_on_changed_direction(Object *p_data, Object *p_wrapper) {
    RESOLVE_BULLET(p_wrapper)
    int   dir_type  = (int)p_data->get("dir_type");
    float dir_value = (float)p_data->get("dir_value");
    float over      = (float)p_data->get("over");
    float target    = _dir_to_angle(b, dir_type, dir_value);
    b.last_angle    = b.angle;
    if (over <= 0.0f) {
        b.angle = target;
        b.angle_tween.active = false;
    } else {
        b.angle_tween = { true, b.angle, target, 0.0f, over };
    }
}

void TamaServerBulletPool::_on_changed_speed(Object *p_data, Object *p_wrapper) {
    RESOLVE_BULLET(p_wrapper)
    int   speed_type  = (int)p_data->get("speed_type");
    float speed_value = (float)p_data->get("speed_value");
    float over        = (float)p_data->get("over");
    float target      = _spd_to_value(b, speed_type, speed_value);
    b.last_speed      = b.speed;
    if (over <= 0.0f) {
        b.speed = target;
        b.speed_tween.active = false;
    } else {
        b.speed_tween = { true, b.speed, target, 0.0f, over };
    }
}

void TamaServerBulletPool::_on_changed_position(Object *p_data, Object *p_wrapper) {
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

void TamaServerBulletPool::_on_accelerated(Object *p_data, Object *p_wrapper) {
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

Rect2 TamaServerBulletPool::_world_bounds() const {
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
