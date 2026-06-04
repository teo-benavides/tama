<p align="center">
  <img src="logo-small.png" />
</p>

# Tama for Godot 
[![Discord](https://img.shields.io/discord/1509716856489906337?style=flat&logo=discord&logoColor=white&label=discord&labelColor=5865F2)](https://discord.gg/7qs72jGrub)

[日本語版はこちらに](README.ja.md)  
[Get the VSCode extension for TamaScript](https://marketplace.visualstudio.com/items?itemName=teo-benavides.tamascript)

Tama is a simple framework for creating bullet hell games, powered by an original bullet pattern definition language, TamaScript.

Check out the `example` folder inside the addon to see example TamaScript scripts and learn what you can do with it. Running `example.tscn` you'll find a text field where you can paste TamaScript and it will be run automatically.

## Quick Start
1. Drag and drop the `addons` folder from this repo into your project and enable it through Project Settings -> Plugins.
2. Create a bullet scene from a `TamaBullet` node, or a node extending `TamaBullet`.
3. Create a `TamaBulletRegistry` resource and set `default_scene_bullet` to the scene you just created.
4. Set Tama -> Scripts Path in Project Settings to the folder you'd like to save your TamaScript scripts in. It's `res://tamascripts` by default.
5. Write a TamaScript file and save it in the previously set folder.
6. Add a `TamaEmitter` to your scene and set its `script_filename` to the name of one of your TamaScript files.
7. Configure `TamaManager`, like so:
    ```gdscript
    TamaManager.set_registry(load("res://my_bullet_registry.tres"))
    TamaManager.load_scripts()
    TamaManager.set_player_position($Player.global_position)  # update this every frame
    ```
8. Call `start()` on your `TamaEmitter` to make it start executing your TamaScript.
9. (Optional) Configure a custom `TamaContext` to expose your own GDScript functions to TamaScript. Like this:
    ```
    # example_tama_context.gd
    extends TamaContext
    class_name MyTamaContext

    func some_func()
        # return value here

    # main.gd
    TamaManager.context = MyTamaContext.new()

    # example.tama
    main
        fire
            dir abs some_func()
            spd 200
    ```

## Server Bullets

Server bullets are a high-performance bullet type that uses `RenderingServer` (MultiMesh) and `PhysicsServer2D` directly — no scene nodes are created per bullet. This makes them suitable for dense patterns with thousands of simultaneous bullets.

### Setup

1. In your `TamaBulletRegistry`, click on `Default Server Bullet` and create a new `TamaServerBulletConfig`.
2. Click the newly created `TamaServerBulletConfig` and modify its settings (see properties below).
3. Check "Default to Server Bullets".

### TamaServerBulletConfig Properties

| Property | Type | Default | Description |
|---|---|---|---|
| `frames` | `Array[Texture2D]` | `[]` | Animation frames. Single texture = static sprite. |
| `fps` | `float` | `0` | Animation playback speed. `0` = no animation. |
| `auto_rect` | `bool` | `true` | Auto-compute sprite rect from the first frame's size. Disable to set `rect` manually. |
| `rect` | `Rect2` | `(-8,-8,16,16)` | Sprite draw rect (used when `auto_rect` is off). |
| `texture_scale` | `Vector2` | `(1,1)` | Scale applied to the sprite. |
| `shape_radius` | `float` | `6` | Collision circle radius in pixels. |
| `collision_layer` | `int` | `1` | Physics collision layer. |
| `collision_mask` | `int` | `2` | Physics collision mask. |
| `rotates` | `bool` | `true` | Whether the sprite rotates with the bullet's angle. |
| `face_velocity` | `bool` | `true` | Sprite faces the direction of movement (requires `rotates`). |
| `pool_size` | `int` | `1000` | Maximum simultaneous bullets of this type. When the pool is full, new spawns are silently dropped. |
| `out_of_bounds_margin` | `float` | `50` | Extra pixels past the screen edge before a bullet is recycled. |

### Collision Detection

Connect to the `bullet_hit` signal on `TamaManager`:

```gdscript
TamaManager.bullet_hit.connect(_on_bullet_hit)

func _on_bullet_hit(bullet: TamaServerBullet, body_instance_id: int):
    if body_instance_id == get_instance_id():
        # handle hit...
    TamaManager.destroy_server_bullet(bullet)
```

`TamaServerBullet` exposes: `position`, `angle`, `speed`, `speed_x`, `speed_y`, `active`.

### Limitations

The following TamaScript features are **not supported** for server bullets:

- **`emt`** — attaching a firing emitter to a bullet requires a scene node and does not work with server bullets.

## TamaManager Reference

`TamaManager` is a singleton. Key properties:

| Property / Signal | Type | Description |
|---|---|---|
| `registry` | `TamaBulletRegistry` | The active bullet registry. Set before `start()` is called on any emitter. |
| `player_position` | `Vector2` | Player world position used for `dir aim` calculations. Update every frame. |
| `spawn_parent` | `NodePath` | Where scene bullets are parented. Defaults to the current scene root. |
| `context` | `TamaContext` | Custom context that exposes GDScript functions to TamaScript. |
| `global_out_of_bounds_margin` | `float` | When ≥ 0, overrides the per-config `out_of_bounds_margin` for all server bullets. Default `-1` (disabled). |
| `bullet_count` | `int` *(read-only)* | Total active bullets across all types. |
| `bullet_hit(bullet, body_id)` | Signal | Emitted when a server bullet's collision area overlaps a physics body. |

Key methods:

| Method | Description |
|---|---|
| `get_server_bullet_pool()` | Returns the `TamaServerBulletPool` node (for manual `recycle()` calls). |
| `register_bullet(type, scene)` | Imperative alternative to setting `scene_bullets` in the registry. |
| `register_server_bullet(type, config)` | Imperative alternative to setting `server_bullets` in the registry. |
| `load_scripts(path)` | Reload all TamaScript files from the given path (or from project settings if omitted). |

## TamaScript Syntax

TamaScript is indentation-based. `#` starts a comment.

### Structure

```
main              ← entry point; runs when the emitter starts
    repeat
        fire myfire(200)
        wait 0.3

fire myfire(spd_) ← named fire definition
    dir aim 0
    spd spd_

act circle(n)     ← named action sequence
    repeat n i
        fire
            dir abs (360/n)*i
            spd 200

bullet tracker    ← named bullet definition
    type enemy    ← looks up "enemy" in TamaBulletRegistry
    act
        repeat
            chdir
                dir aim 0
                over 0.5
            wait 0.5
```

### `fire` block properties

| Statement | Default | Description |
|---|---|---|
| `dir [aim\|abs\|rel\|seq] EXPR` | `aim` | Bullet direction (degrees). `aim` = toward player + offset; `abs` = world angle; `rel` = relative to spawner angle; `seq` = relative to last fired angle. |
| `speed [abs\|rel\|seq] EXPR` | `abs` | Bullet speed. `rel`/`seq` add to last fired speed. |
| `offset EXPR` | — | Spawn offset along bullet's local axis. |
| `offset` *(block)* | — | Per-axis spawn offset. Default qualifier `rel` (rotated by bullet angle); `abs`/`seq` add to spawner position in world space. |
| `pos` *(block)* | — | Set spawn position directly. Default qualifier `abs` (world coords); `rel` adds to spawner position. Takes priority over `offset`. |
| `bullet NAME` | registry default | Which bullet to spawn. |

```
fire
    dir abs 90
    speed 150
    pos
        x abs 500
        y rel 0
    bullet my_bullet(arg1)
```

### `act` block statements

| Statement | Description |
|---|---|
| `wait EXPR` | Pause N seconds. |
| `waitf EXPR` | Pause N physics frames. |
| `repeat [N] [i]` | Loop N times (omit N for infinite). `i` = 0-based index. |
| `repeatf [N] [i]` | Run the block once per physics frame (synchronous). Without `N`: infinite and terminal. With `N`: runs N times then continues; `i` = 0-based index. |
| `while COND` | Loop while `COND` is non-zero. |
| `if COND` / `elif COND` / `else` | Conditional branching. |
| `var NAME EXPR` | Declare a local variable. |
| `NAME EXPR` | Reassign an existing variable (change propagates to parent scope). |
| `fire NAME` / `fire` *(inline)* | Spawn a bullet. |
| `act NAME` / `act` *(inline)* | Run an act (blocking). |
| `async act …` | Run an act without blocking. |
| `chdir` / `chspd` / `chpos` / `accel` | Send a transition command to this bullet. Omit `over` (or set it to `0`) to apply instantly. |
| `vanish` | Stop this bullet's act and destroy it. |
| `break` | Exit the innermost enclosing `repeat`, `repeatf`, or `while`. |

### `bullet` block statements

| Statement | Description |
|---|---|
| `type NAME` | Bullet type — looks up `NAME` in `TamaBulletRegistry`. Omit to use the registry default. |
| `emt NAME` / `emt` *(inline)* | Attach a firing emitter that runs in parallel with the bullet's `act`. **Not supported for server bullets.** |
| `mvmt` *(block)* | Per-frame position expression re-evaluated every physics frame. `abs` = world coordinate; `rel` = offset from spawn position. |
| `act NAME` / `act` *(inline)* | Behaviour the bullet runs after spawning. |

### Variables and control flow

`var NAME EXPR` declares a variable scoped to the current block. A bare `NAME EXPR` reassigns an existing variable and the change propagates back to parent blocks. `true` and `false` are valid values (equal to `1.0` and `0.0`).

```
main
    var count 8
    var speed 200
    while count > 0
        if count > 4
            fire
                dir aim 0
                spd speed
        else
            fire
                dir aim 45
                spd speed * 0.5
        count count - 1    ← reassign (no var keyword)
        wait 0.1
```

### Passing definitions as arguments

`fire`, `act`, and `bullet` definitions can be passed as arguments and called by the receiver. You can also pre-bind arguments to whatever you're passing:

```
act x_way(n, f, spd_)
    repeat n i
        fire f          ← calls whatever fire def was passed as f
        wait 0.05

main
    act x_way(8, spread)              ← pass the fire def "spread" by name
    act x_way(8, spread(45), 200)    ← pre-bind 45 as spread's first arg
```

### Bullet direction/speed/position transitions (inside bullet `act`)

`over` is optional on all four statements and defaults to `0`. When `over` is `0` the value is applied instantly without tweening.

```
chdir               ← change direction (instant — no over)
    dir aim 0

chdir               ← change direction (tweened)
    dir abs 90
    over 1.0        ← transition time in seconds

chspd
    spd abs 400
    over 2.0

chpos               ← move to position
    x abs 500
    y abs 300
    over 1.5

accel               ← world-axis acceleration
    x 0
    y 50
    over 1.5
```

### `export` and `include`

```
export num speed 200     ← exposes a float field in the inspector
export str dir_mode aim  ← exposes a string field (use for aim/abs/rel/seq)
export bool enabled true ← exposes a bool field (checkbox in the inspector)

include builtin          ← merges fire/act/bullet defs from another .tama file
```

## Planned features
### TamaScript
- Strings, delimited by `"`
- Small example game
- Documentation comments
- Node graph script editor

## Special thanks
[@icons](https://github.com/Voxybuns/at-icons)
