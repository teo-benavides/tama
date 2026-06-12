# Tama — Architecture & Orientation Guide

> Purpose: get a new Claude Code instance productive fast, without reading the
> whole tree. Read this first, then jump to the specific file you need.
> Pair it with `README.md` (user-facing API reference) and
> `TAMASCRIPT_SPEC.md` / `tamascript_grammar.bnf` (the language spec).

## What this project is

Tama is a **bullet hell framework for Godot 4.x**, distributed as a Godot addon.
Patterns are written in **TamaScript**, a custom indentation-based DSL inspired by
BulletML. The entire runtime (lexer, parser, interpreter, bullet/laser pools,
manager) is implemented as a **C++ GDExtension** — there is almost no GDScript in
the framework itself.

- Godot version target: 4.x (`compatibility_minimum = 4.4` in the gdextension).
- The CLAUDE.md says "Godot 4.6.2".
- Language of the DSL: TamaScript (`.tama` / `.tam` files).
- There is also a published **VSCode extension** for TamaScript (separate repo).

## Where everything lives

```
addons/tama/
├── tama.gdextension          # GDExtension manifest; entry_symbol = tama_library_init
├── plugin.cfg / plugin.gd    # EditorPlugin: registers project settings (scripts_path, etc.)
├── bin/                       # PREBUILT binaries (libtama.{windows,linux}.*) — committed
├── icons/                     # editor icons for the custom node types
├── native/                    # ← ALL C++ SOURCE LIVES HERE
│   ├── SConstruct             # build script (wraps godot-cpp/SConstruct)
│   ├── godot-cpp/             # vendored godot-cpp bindings — DO NOT EDIT, not our code
│   ├── doc_classes/*.xml      # in-editor class documentation (one per public class)
│   └── src/                   # ← the actual framework implementation
└── example/                   # demo project: example.tscn + tamascripts/ + bullets/
    ├── example.gd             # wires up TamaManager; live TextEdit runs pasted scripts
    ├── example_tama_context.gd# sample TamaContext (mid_x(), spiral_x(), f2s(), etc.)
    └── tamascripts/*.tama     # ~60 example patterns — best reference for DSL usage

# Repo root:
README.md / README.ja.md       # user-facing API docs (very detailed — keep in sync)
TAMASCRIPT_SPEC.md             # language spec prose
tamascript_grammar.bnf         # formal EBNF grammar for TamaScript
```

**The example scripts in `addons/tama/example/tamascripts/` are the single best way
to learn what the DSL can express.** `bowap_clean.tama` is a minimal canonical pattern.

## The C++ source map (`addons/tama/native/src/`)

Naming: classes prefixed with `_` (e.g. `_TamaInterpreter`, `_TamaSpawnManager`)
are **internal** — registered via `register_internal_class` and hidden from the
editor/docs. Unprefixed classes (`TamaManager`, `TamaEmitter`, `TamaBullet`, …)
are the public API surface.

### Language pipeline (source text → executable AST)
| File | Role |
|---|---|
| `tama_token.h` | `TamaToken` + `TT::` token-type enum. |
| `tama_lexer.{h,cpp}` | Indentation-aware lexer. Emits INDENT/DEDENT tokens. Keyword table lives here (`tama_lexer.cpp` top). |
| `tama_parser.{h,cpp}` | Recursive-descent parser → AST. One `parse_*()` per construct. Resolves `include` at parse time (rejects cycles). |
| `tama_ast_nodes.{h,cpp}` | `_TamaASTNode` (one fat struct for all node kinds, tagged by `type_id`), `TamaArgVal` (evaluated arg / first-class ref), `TamaRef` (named def + pre-bound args). |
| `tama_expr.{h,cpp}` | `_TamaExprRuntime` singleton — wraps Godot's `Expression` class to evaluate numeric `EXPR` strings, exposing built-ins like `time()`, `mid_x()`, plus context methods. |

### Execution
| File | Role |
|---|---|
| `tama_interpreter.{h,cpp}` | **The heart.** `_TamaInterpreter` is a plain C++ (non-Node) **state-machine** interpreter. Explicit `_exec_stack` of `ExecFrame`s (BODY / REPEAT_CTRL / WHILE_CTRL / REPEATF_CTRL) rather than native recursion, so execution can suspend on `wait`/`waitf` and resume across frames. `step(delta)` advances it. Async acts run as nested `_TamaInterpreter`s in `_async_children`. |
| `tama_emitter.{h,cpp}` | `TamaEmitter : Node2D` — the user-placed node. Owns a root `_TamaInterpreter`, runs `main`. Handles `export` inspector vars (dynamic `_get`/`_set`/`_get_property_list`) and hot-reloads the script when the file mtime changes (polls every `POLL_INTERVAL`s). |
| `tama_spawn_manager.{h,cpp}` | `_TamaSpawnManager : Node` — internal. Receives `TamaBulletFireData` from interpreters via the `_fire_cb` callback and turns it into actual bullets/lasers. Resolves `dir/speed/pos/rotspd` qualifiers (aim/abs/rel/seq) against the spawner. Routes to scene bullets vs. server pools. |
| `tama_manager.{h,cpp}` | `TamaManager : Object` — the **singleton** (registered as engine singleton `TamaManager`; NOT an autoload anymore — `plugin.gd` removes the legacy autoload). Public façade: `load_scripts`, registry/context/player_position, the `*_hit` signals, pool accessors. Lazily creates the spawn manager + pools as scene nodes. |
| `tama_script_repository.{h,cpp}` | `_TamaScriptRepository` — parses & caches `.tama` files by name; holds the ASTs. |
| `tama_context.{h,cpp}` | `TamaContext : RefCounted-ish` base. Users subclass in GDScript to expose functions to TamaScript (`mid_x()`, custom callbacks). Provides built-ins like `time()`. |

### Rendering & object types
There are **three kinds of fired objects**. Scene bullets are real nodes; the other
three are "server objects" rendered via `RenderingServer`/`MultiMesh` with CPU
circle/segment collision against `player_position` — built for thousands of objects.

| File(s) | Object type |
|---|---|
| `tama_bullet.{h,cpp}` | `TamaBullet : Node2D` — **scene bullet** (one node per bullet). Subclass for custom visuals. Supports `emt` (firing emitter), which server bullets do NOT. |
| `tama_server_bullet.h` + `tama_server_bullet_pool.{h,cpp}` + `tama_server_bullet_config.{h,cpp}` | **Server bullet** (MultiMesh, pooled). `TamaServerBullet` is a thin wrapper Object exposed via the `bullet_hit` signal. Pool batches by type, has spawn-animation/delay support. |
| `tama_server_laser*.{h,cpp}` (+ `_straight_laser_config`) | **Straight laser** — fixed beam, phases: delay→expand→active→fade. |
| `tama_server_curved_laser*.{h,cpp}` | **Curved laser** — moving ribbon trail, segment collision. |
| `tama_server_object_config.{h,cpp}` | Shared base config for the three server object types. |
| `tama_animated_texture.{h,cpp}` | `TamaAnimatedTexture` — frames + fps + blend mode; used by every server config. |
| `tama_bullet_registry.{h,cpp}` | `TamaBulletRegistry : Resource` — maps type-name → scene (`scene_bullets`) or server config (`objects`); holds `default_bullet`. |
| `register_types.cpp` | GDExtension init: registers all classes, creates the `TamaManager` and `_TamaExprRuntime` singletons, declares project settings. **Add new classes here.** |

## How a frame flows (mental model)

1. User places a `TamaEmitter`, sets `script_filename`, calls `start()`.
2. Emitter looks up the parsed AST via `TamaManager` → `_TamaScriptRepository`,
   seeds scope with `export` values, and runs `main` on its root `_TamaInterpreter`.
3. Each `_physics_process`, the emitter `step()`s its interpreter. The interpreter
   walks its `_exec_stack`, executing nodes until it yields (on `wait`/`waitf`) or
   finishes. `fire` statements build a `TamaBulletFireData` and invoke `_fire_cb`.
4. `_fire_cb` → `_TamaSpawnManager::_on_bullet_fired` resolves direction/speed/
   position qualifiers against the spawner and spawns the object (scene node or
   into the appropriate server pool).
5. Each spawned object that has an `act` runs its **own** `_TamaInterpreter`,
   stepped by the pool / the bullet node. `chdir`/`chspd`/`chpos`/`accel`/`chrotspd`
   inside a bullet's act are delivered as C++ events through `TamaBulletEventHandler`
   (no Godot signal overhead) and applied as tweens by the pool.
6. Pools run CPU collision against `TamaManager.player_position` and emit
   `bullet_hit` / `straight_laser_hit` / `curved_laser_hit`. The game connects
   these and calls `destroy_server_bullet` etc.

## Key design points / gotchas

- **Performance-first C++.** The interpreter deliberately avoids Godot machinery on
  the hot path: scope values are a native tagged union (`TamaScopeVal`, no Variant
  boxing for floats), fire data is a stack struct, bullet transitions use C++
  virtual callbacks instead of signals. Preserve this when editing — don't introduce
  per-bullet/per-frame Variant allocations or signal emissions.
- **Two AST ownership styles.** `_TamaASTNode`s parsed from a program are kept alive
  by `shared_ptr` in the AST tree; body execution uses raw pointers into that tree.
  Inline parser-created nodes passed as args carry their own `shared_ptr` owner in
  `TamaArgVal::_owner`. Watch lifetimes when adding new arg-passing paths.
- **Qualifiers (aim/abs/rel/seq) can be runtime values.** `dir_type_var` /
  `speed_type_var` / `axis_type_var` on a node mean the qualifier came from a
  variable (e.g. an exported `str`), resolved at exec time via `_get_dir_type` etc.
- **Scopes:** `var` declares in the current block; bare `NAME = EXPR` reassigns and
  propagates to parent blocks (shared scope dict). Named `act`/`fire` calls get a
  scope *copy* (function-call semantics) — assignments inside don't leak out.
- **Hot reload:** `TamaEmitter` polls the script file mtime and reloads on change;
  the example's TextEdit uses `load_script_from_source("temp", ...)`.
- **Singleton, not autoload:** access `TamaManager` directly (it's an engine
  singleton). `plugin.gd` strips the old `autoload/TamaManager` if present.
- **`bin/` is committed.** The prebuilt `.dll`/`.so` are checked in, so the addon
  works without building. If you change C++, you must rebuild and the new binary is
  what ships.

## Building the GDExtension

C++ changes require recompiling with **SCons** (godot-cpp toolchain). From
`addons/tama/native/`:

```
scons platform=windows target=template_debug      # editor/debug .dll
scons platform=windows target=template_release     # release .dll
# platform=linux for the .so variants
```

Output lands in `addons/tama/bin/` with the names the `.gdextension` expects.
`target=editor`/`template_debug` also compiles the doc XML into the binary.
`godot-cpp/` must be initialized (it's vendored/committed here).

> Note: the dev environment is Windows + PowerShell. The Godot editor LSP is
> unreliable here — **do not rely on it to surface C++/GDScript errors** (per CLAUDE.md).
> Validate by building with SCons and/or running `example.tscn`.

## The `event` statement

`event NAME(args...)` is a fire-and-forget hook that surfaces a named event from a
running script to game code via a Godot signal. Use it for side effects the
framework shouldn't own — SFX, screen shake, score, etc. `bowap_clean.tama` uses
`event sfx("shot.wav")`.

Full path: lexer `KW_EVENT` → parser `parse_event` → AST `NT::EVENT` → interpreter
`_emit_event` (evaluates each arg; quoted strings pass through as `String`, numeric
EXPRs are evaluated to floats; node/ref args are dropped) → `_event_cb`. The callback
is installed on every interpreter in `_TamaSpawnManager::connect_interpreter` (and
propagated to child/async interpreters), and emits on the singleton:

```
TamaManager.event_fired(name: String, args: Array)
```

Connect it in game code and dispatch on `name`:

```gdscript
TamaManager.event_fired.connect(func(name, args):
    if name == "sfx":
        play_sfx(args[0]))
```

**`event` vs. `context_call`:** `event foo(...)` emits a decoupled signal (no
TamaContext needed; one central handler). `foo(...)` (a bare `context_call`) instead
invokes method `foo` directly on the configured `TamaContext`, discarding the return.
Use `event` for game-side reactions, `context_call` for context-defined behavior.

## Keeping docs in sync

When you change DSL syntax or public API, update **all** of: the relevant
`parse_*`/lexer code, `tamascript_grammar.bnf`, `TAMASCRIPT_SPEC.md`, all 4 READMEs and the matching `native/doc_classes/*.xml`.
