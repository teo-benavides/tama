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

## TamaAnimatedTexture

`TamaAnimatedTexture` is the texture type used by all server object configs. It stores animation frames, playback speed, and a blend mode that is applied to all objects of that type.

| Property | Type | Default | Description |
|---|---|---|---|
| `frame_count` | `int` | `0` | Number of frames. Set to `1` for a static (non-animated) texture. |
| `fps` | `float` | `60` | Animation playback speed. `0` or less disables animation. |
| `blend_mode` | `enum` | `Mix` | How the object type is composited: **Mix**, **Add**, **Subtract**, **Multiply**, **Premultiplied Alpha**. Applied per type. |
| `frame_N/texture` | `Texture2D` | — | Texture for frame N (shown in the inspector when `frame_count` > 0). |
| `frame_N/duration` | `int` | `1` | How many animation-frames this frame is shown before advancing. |

## Server Bullets

Server bullets use `RenderingServer` (MultiMesh) for rendering — no scene nodes are created per bullet. Collision is a CPU circle test against the player position each frame. This makes them suitable for dense patterns with hundreds to thousands of simultaneous bullets.

### Setup

1. In your `TamaBulletRegistry`, open the `objects` dictionary and add an entry whose key is the type name and whose value is a new `TamaServerBulletConfig`.
2. Configure the `TamaServerBulletConfig` (see properties below).
3. Optionally check "Default to Server Bullets" to use this type when no `bul` block is specified.

### TamaServerBulletConfig Properties

| Property | Type | Default | Description |
|---|---|---|---|
| `texture` | `TamaAnimatedTexture` | — | Animated texture for the bullet. Single-frame = static sprite. Blend mode set here. |
| `auto_rect` | `bool` | `true` | Auto-compute sprite rect from the first frame's pixel size. |
| `rect` | `Rect2` | `(-8,-8,16,16)` | Sprite draw rect (used when `auto_rect` is off). |
| `texture_scale` | `Vector2` | `(1,1)` | Scale applied to the sprite visual only. |
| `shape_radius` | `float` | `6` | Collision circle radius in pixels. |
| `rotates` | `bool` | `true` | Whether the sprite rotates with the bullet's angle. |
| `face_velocity` | `bool` | `true` | Sprite faces the direction of movement (requires `rotates`). |
| `pool_size` | `int` | `1000` | Maximum simultaneous bullets of this type. Excess spawns are silently dropped. |
| `out_of_bounds_margin` | `float` | `50` | Extra pixels past the screen edge before a bullet is recycled. |
| `spawn_delay` | `int` | `0` | Physics frames to freeze the bullet before it starts moving. Spawn animation plays during this period. Can be overridden per-fire with `delay` in TamaScript. |
| `spawn_texture` | `TamaAnimatedTexture` | — | Animated texture for the spawn effect (rendered above the bullet). Falls back to the first frame of `texture` when not set. |
| `starting_spawn_scale` | `float` | `2.0` | Scale of the spawn sprite at the first delay frame. Lerps to `1.0` by the end. |
| `starting_spawn_opacity` | `float` | `0.0` | Opacity of the spawn sprite at the first delay frame. Lerps to `1.0` by the end. |

### Collision Detection

Connect to `TamaManager.bullet_hit`. Collision fires when the bullet's `shape_radius` + `TamaManager.player_hitbox_radius` overlaps `TamaManager.player_position`:

```gdscript
func _ready():
    TamaManager.bullet_hit.connect(_on_bullet_hit)

func _on_bullet_hit(bullet: TamaServerBullet) -> void:
    TamaManager.destroy_server_bullet(bullet)
```

`TamaServerBullet` exposes: `position`, `angle`, `speed`, `speed_x`, `speed_y`, `active`.

### Limitations

The following TamaScript features are **not supported** for server bullets:

- **`emt`** — attaching a firing emitter requires a scene node and does not work with server bullets.

## Straight Lasers

Straight lasers are beam-shaped objects anchored at a fixed position and angle, passing through four phases: **delay** (warning line, no collision) → **expand** (beam grows to full width) → **active** (full width) → **fade** (beam disappears).

### Setup

Add a `TamaServerStraightLaserConfig` entry to the `objects` dictionary in your `TamaBulletRegistry`.

### TamaServerStraightLaserConfig Properties

| Property | Type | Default | Description |
|---|---|---|---|
| `width` | `float` | `20` | Full beam width in pixels at maximum expansion. |
| `length` | `float` | `1000` | Beam length in pixels from the spawn position. |
| `texture` | `TamaAnimatedTexture` | — | Animated texture stretched/tiled across the beam. Blend mode set here. |
| `tile_x` | `bool` | `false` | Tile the texture along the beam's length instead of stretching. |
| `tile_y` | `bool` | `false` | Tile the texture across the beam's width instead of stretching. |
| `base_texture` | `TamaAnimatedTexture` | — | Optional texture drawn centred at the spawn point (e.g. an emitter flash). |
| `delay_frames` | `int` | `120` | Frames of the 1-pixel warning line. No collision. |
| `expand_frames` | `int` | `10` | Frames to expand from 1px to full width. Collision active. |
| `duration_frames` | `int` | `120` | Frames at full width. Collision active. |
| `fade_frames` | `int` | `30` | Frames to fade out. No collision. |
| `pool_size` | `int` | `1000` | Maximum simultaneous lasers of this type. |

### Collision Detection

```gdscript
func _ready():
    TamaManager.straight_laser_hit.connect(_on_laser_hit)

func _on_laser_hit(laser: TamaServerLaser) -> void:
    pass  # handle player hit
```

## Curved Lasers

Curved lasers move through space each frame and render as a ribbon trail. The trail is a triangle strip that tapers at tip and tail; collision is tested against all trail segments.

### Setup

Add a `TamaServerCurvedLaserConfig` entry to the `objects` dictionary in your `TamaBulletRegistry`.

### TamaServerCurvedLaserConfig Properties

| Property | Type | Default | Description |
|---|---|---|---|
| `width` | `float` | `20` | Ribbon width in pixels. |
| `length` | `int` | `30` | Number of trail nodes retained. Higher = longer visible trail. |
| `texture` | `TamaAnimatedTexture` | — | Animated texture applied to the ribbon (UV 0→1 along trail length). Blend mode set here. |
| `pool_size` | `int` | `1000` | Maximum simultaneous lasers of this type. |
| `out_of_bounds_margin` | `float` | `50` | Pixels past the screen edge before the laser head triggers recycling. |

### Collision Detection

```gdscript
func _ready():
    TamaManager.curved_laser_hit.connect(_on_curved_laser_hit)

func _on_curved_laser_hit(laser: TamaServerCurvedLaser) -> void:
    pass  # handle player hit
```

## TamaManager Reference

`TamaManager` is a singleton. Key properties:

| Property / Signal | Type | Description |
|---|---|---|
| `registry` | `TamaBulletRegistry` | The active object registry. Set before `start()` is called on any emitter. |
| `player_position` | `Vector2` | Player world position. Update every frame for `dir aim` and collision. |
| `player_hitbox_radius` | `float` | Radius of the player hitbox in pixels used by CPU collision tests. Default `3`. |
| `spawn_parent` | `NodePath` | Where scene bullets are parented. Defaults to the scene root. |
| `context` | `TamaContext` | Custom context that exposes GDScript functions to TamaScript. |
| `global_out_of_bounds_margin` | `float` | When ≥ 0, overrides the per-config `out_of_bounds_margin` for all types. Default `-1` (disabled). |
| `world_rect` | `Rect2` | World bounds rectangle. When set (non-zero size): `pos abs` spawn coordinates are offset by `world_rect.position` (so `pos x abs 0 y abs 0` spawns at the top-left of the world rect, not the viewport origin); server bullets and curved lasers are recycled when they exit this rect; bullets with `bounces` reflect off the edges of this rect. Defaults to the viewport rect when not set. |
| `bullet_count` | `int` *(read-only)* | Total active server objects across all types. |
| `bullet_hit(bullet)` | Signal | Emitted when a server bullet overlaps the player hitbox. |
| `straight_laser_hit(laser)` | Signal | Emitted when a straight laser beam overlaps the player hitbox (expand/active phases only). |
| `curved_laser_hit(laser)` | Signal | Emitted when any segment of a curved laser trail overlaps the player hitbox. |
| `event_fired(name, args)` | Signal | Emitted by the `event` statement in TamaScript. `name` is a `String`; `args` is an `Array` of the evaluated arguments. Connect to react to script-driven events (SFX, score, etc.). |

Key methods:

| Method | Description |
|---|---|
| `get_server_bullet_pool()` | Returns the `TamaServerBulletPool` node. |
| `get_laser_pool()` | Returns the `TamaServerLaserPool` node for straight lasers. |
| `get_curved_laser_pool()` | Returns the `TamaServerCurvedLaserPool` node for curved lasers. |
| `register_bullet(type, scene)` | Imperative alternative to setting `scene_bullets` in the registry. |
| `register_server_bullet(type, config)` | Imperative alternative to adding to `objects` in the registry. |
| `destroy_server_bullet(bullet)` | Recycles a server bullet. Call from `bullet_hit`. |
| `recycle_all()` | Immediately recycles all active bullets and lasers. |
| `load_scripts(path)` | Load all TamaScript files from the given path (or project settings if omitted). |

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
| `rotspd [abs\|rel\|seq] EXPR` | `0` (no spin) | Initial rotation speed in degrees/sec. Bullet's angle accumulates at this rate each frame. `rel`/`seq` add to last fired rotation speed. |
| `offset EXPR` | — | Spawn offset along bullet's local axis. |
| `offset` *(block)* | — | Per-axis spawn offset. Default qualifier `rel` (rotated by bullet angle); `abs`/`seq` add to spawner position in world space. |
| `pos` *(block)* | — | Set spawn position directly. Default qualifier `abs` (world coords); `rel` adds to spawner position. Takes priority over `offset`. |
| `bullet NAME` | registry default | Which bullet to spawn. |
| `delay EXPR` | — | *(Server bullets only)* Overrides the bullet type's `spawn_delay` for this fire only. `EXPR` = physics frames to freeze while the spawn animation plays. |

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
| `var NAME = EXPR` | Declare a local variable. |
| `NAME = EXPR` | Reassign an existing variable (change propagates to parent scope). |
| `NAME(args...)` | Call a method on the TamaContext object (see Quick Start step 9). Return value is discarded. Arguments can be numeric expressions or quoted strings (e.g. `sfx("fire")`). |
| `event NAME(args...)` | Emit `TamaManager.event_fired(name, args)` — a fire-and-forget signal to game code. Args can be numeric expressions or quoted strings (e.g. `event sfx("shot.wav")`). Parentheses are optional when there are no args. Use this for SFX/score/screen-shake; unlike a context call it needs no `TamaContext`. |
| `fire NAME` / `fire` *(inline)* | Spawn a bullet. |
| `form NAME` / `form` *(inline)* | Spawn multiple bullets simultaneously in a spatial pattern (ring or fan). |
| `act NAME` / `act` *(inline)* | Run an act (blocking). |
| `async act …` | Run an act without blocking. |
| `chdir` / `chspd` / `chrotspd` / `chpos` / `accel` | Send a transition command to this bullet. Omit `over` (or set it to `0`) to apply instantly. |
| `vanish` | Stop this bullet's act and destroy it. |
| `break` | Exit the innermost enclosing `repeat`, `repeatf`, or `while`. |

### `bullet` block statements

| Statement | Description |
|---|---|
| `type NAME` | Bullet type — looks up `NAME` in `TamaBulletRegistry`. Omit to use the registry default. |
| `emt NAME` / `emt` *(inline)* | Attach a firing emitter that runs in parallel with the bullet's `act`. **Not supported for server bullets.** |
| `mvmt` *(block)* | Per-frame position expression re-evaluated every physics frame. `abs` = world coordinate; `rel` = offset from spawn position. |
| `act NAME` / `act` *(inline)* | Behaviour the bullet runs after spawning. |
| `bounces [N] [x\|y]` | Make the bullet reflect off the world bounds instead of despawning. Bounds are `TamaManager.world_rect` when set, otherwise the viewport. `N` = max number of bounces (omit or `-1` for infinite). `x` = left/right walls only; `y` = top/bottom walls only; omit axis for all borders. After the last bounce the bullet exits normally. |

```
bullet wall_bouncer
    bounces 3       ← bounce up to 3 times off all borders, then exit

bullet side_bouncer
    bounces x       ← infinite bounces off left/right walls only

bullet finite_y
    bounces 2 y     ← bounce twice off top/bottom walls, then exit normally
```

### `form` block statements

| Sub-statement | Description |
|---|---|
| `type ring` \| `type fan` | Layout algorithm. `ring`: bullets evenly distributed over 360°. `fan`: bullets centered on `dir`. Required. |
| `amt EXPR` | Number of bullets to fire. |
| `dir [QUALIFIER] EXPR` | Center direction. Accepts the same qualifiers as `fire`'s `dir` (`aim`, `abs`, `rel`, `seq`, `away`), including a variable qualifier (non-keyword identifier before the expression, resolved from scope). `away` is ring-only. |
| `spd [QUALIFIER] EXPR` | Bullet speed (same qualifiers as `fire`'s `speed`). |
| `spr EXPR` | *(Fan only)* Total angular spread in degrees; step between adjacent bullets = `spr / (amt-1)`. |
| `step EXPR` | *(Fan only)* Fixed angular step between adjacent bullets in degrees. Takes priority over `spr` when both are present. Use when spacing must stay constant as `amt` changes across calls. |
| `rad EXPR` | *(Ring only)* Offset distance along each bullet's travel direction. |
| `rotspd [QUALIFIER] EXPR` | Initial rotation speed (degrees/sec) applied to each bullet at spawn. |
| `bul IDENT` \| `bul` *(block)* | Bullet type — same as `fire`'s `bullet`. |

```
# Ring of 8 bullets avoiding the player
form
    type ring
    amt 8
    dir away
    spd 200
    bul type enemy

# Fan of 5 aimed bullets spread 90°
form
    type fan
    amt 5
    spr 90
    dir aim 0
    spd 150

# Fan with fixed step — spacing stays the same as amt changes
form myfan(n, angle, spd_)
    type fan
    amt n
    step 10           ← 10° between adjacent bullets regardless of n
    dir abs angle
    spd spd_
```

### Built-in scope variables

The following float variables are automatically available in every bullet/laser `act` body — no declaration needed.

**Spawn-time** (set once at spawn, constant for the bullet's lifetime):

| Variable | Description |
|---|---|
| `spawn_x` | World X position at the moment the bullet spawned. |
| `spawn_y` | World Y position at the moment the bullet spawned. |
| `spawn_angle` | Angle in radians at spawn (the direction the bullet was fired). |
| `spawn_speed` | Speed in pixels/sec at spawn. `0` for straight lasers. |

**Per-frame** (updated every physics frame before the act runs):

| Variable | Description |
|---|---|
| `current_x` | Current X position relative to `world_rect.position`. |
| `current_y` | Current Y position relative to `world_rect.position`. |
| `current_angle` | Current angle in radians of the bullet's direction of travel. |
| `current_speed` | Current speed in pixels/sec. `0` for straight lasers. |

`current_x`/`current_y` use the world-rect coordinate system: `(0, 0)` = top-left of `TamaManager.world_rect`. `spawn_x`/`spawn_y`/`spawn_angle`/`spawn_speed` are also available in `mvmt` expressions.

### Variables and control flow

`var NAME = EXPR` declares a variable scoped to the current block. `NAME = EXPR` (no keyword) reassigns an existing variable and the change propagates back to parent blocks. `true` and `false` are valid values (equal to `1.0` and `0.0`).

```
main
    var count = 8
    var speed = 200
    while count > 0
        if count > 4
            fire
                dir aim 0
                spd speed
        else
            fire
                dir aim 45
                spd speed * 0.5
        count = count - 1    ← reassign (no var keyword)
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

`over` is optional on all these statements and defaults to `0`. When `over` is `0` the value is applied instantly without tweening.

```
chdir               ← change direction (instant — no over)
    dir aim 0

chdir               ← change direction (tweened)
    dir abs 90
    over 1.0        ← transition time in seconds

chspd
    spd abs 400
    over 2.0

chrotspd            ← change rotation speed
    rotspd abs 90   ← 90°/sec clockwise
    over 0.5

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
