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
3. Create a `TamaBulletRegistry` resource and add your bullet to `entries` and `default_bullet`. When adding to `entries`, you need to give the bullet a type name.
4. Set Tama -> Scripts Path in Project Settings to the folder you'd like to save your TamaScript scripts in. It's `res://tamascripts` by default.
5. Write a TamaScript file and save it in the previously set folder.
6. Add a `TamaEmitter` to your scene and set its `script_filename` to the name of one of your TamaScript files.
7. Configure `TamaManager`, like so:
    ```gdscript
    # registry you created in Step 3
    TamaManager.registry = load("res://my_bullet_registry.tres")
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
| `repeatf` | Run the block once per physics frame (synchronous; terminal). |
| `while COND` | Loop while `COND` is non-zero. |
| `if COND` / `elif COND` / `else` | Conditional branching. |
| `var NAME EXPR` | Declare a local variable. |
| `NAME EXPR` | Reassign an existing variable (change propagates to parent scope). |
| `fire NAME` / `fire` *(inline)* | Spawn a bullet. |
| `act NAME` / `act` *(inline)* | Run an act (blocking). |
| `async act …` | Run an act without blocking. |
| `chdir` / `chspd` / `chpos` / `accel` | Send a transition command to this bullet. |
| `vanish` | Stop this bullet's act and destroy it. |

### `bullet` block statements

| Statement | Description |
|---|---|
| `type NAME` | Bullet scene type — looks up `NAME` in `TamaBulletRegistry`. Omit to use the registry default. |
| `emt NAME` / `emt` *(inline)* | Attach a firing emitter that runs in parallel with the bullet's `act`. |
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

```
chdir               ← change direction
    dir aim 0
    over 1.0        ← transition time in seconds

chspd
    spd abs 400
    over 2.0

chpos               ← move to position
    x abs 500
    y abs 300
    over 1.5        ← omit over for instant

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
- Optimization via RenderingServer and PhysicsServer2D, forgoing Nodes for bullets
- In-depth documentation
### TamaScript
- Strings, delimited by `"`
- Small example game
- Documentation comments
- Node graph script editor

## Special thanks
[@icons](https://github.com/Voxybuns/at-icons)
