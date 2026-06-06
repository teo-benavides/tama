# TamaScript Language Specification

TamaScript is an indentation-based DSL for describing bullet-hell patterns, inspired by BulletML. It runs inside a Godot 4 framework called Tama. This document is intended as a complete reference for building tooling (e.g. a VSCode language plugin).

---

## Table of Contents

1. [File and Encoding](#1-file-and-encoding)
2. [Lexer](#2-lexer)
3. [Top-Level Structure](#3-top-level-structure)
4. [Definitions](#4-definitions)
5. [Blocks](#5-blocks)
6. [Action Block Statements](#6-action-block-statements)
7. [Fire Block Statements](#7-fire-block-statements)
8. [Bullet Block Statements](#8-bullet-block-statements)
9. [Shared Sub-Statements](#9-shared-sub-statements) (`dir`, `speed`, `offset`, `pos`, `over`)
10. [Expressions](#10-expressions)
11. [Qualifiers](#11-qualifiers)
12. [First-Class Definitions](#12-first-class-definitions)
13. [Include System](#13-include-system)
14. [Export Declarations](#14-export-declarations)
15. [Runtime Semantics](#15-runtime-semantics)
16. [Parser Behaviour Notes](#16-parser-behaviour-notes)
17. [Complete Keyword List](#17-complete-keyword-list)
18. [Full Grammar (EBNF)](#18-full-grammar-ebnf)

---

## 1. File and Encoding

- Extension: `.tama` or `.tam`
- Encoding: UTF-8
- Line endings: `\n`, `\r\n`, or `\r` (normalised to `\n`)
- Scripts are loaded from `res://tamascripts/` in the Godot project

---

## 2. Lexer

### 2.1 Whitespace and indentation

Indentation is **significant**. The lexer emits `INDENT` and `DEDENT` tokens when the leading whitespace of a line increases or decreases relative to the previous non-blank line.

- Spaces: 1 space = 1 unit
- Tabs: 1 tab = 4 units
- Blank lines and comment-only lines are **skipped entirely** (no NEWLINE token emitted for them)
- A `NEWLINE` token is emitted at the end of every non-blank, non-comment line
- Indentation mismatch (dedenting to a level that was never opened) is a lexer error

### 2.2 Comments

```
# this is a comment
wait 1.0  # inline comment
```

`#` starts a comment that runs to end of line. Valid anywhere whitespace is allowed. The lexer stops tokenising the line at `#`.

### 2.3 Token types

| Token | Pattern | Notes |
|---|---|---|
| `NUMBER` | `\d+(\.\d+)?` | Float literal; floats matched before ints |
| `WORD` | `[a-zA-Z_][a-zA-Z0-9_]*` | Identifier or keyword |
| `LPAREN` | `(` | |
| `RPAREN` | `)` | |
| `OP` | `==`, `!=`, `<=`, `>=`, `&&`, `\|\|`, `*`, `/`, `+`, `-`, `<`, `>`, `!`, `&`, `\|`, `=`, `%` | Two-char operators matched before single-char |
| `COMMA` | `,` | |
| `NEWLINE` | *(end of non-blank line)* | |
| `INDENT` | *(indent increase)* | |
| `DEDENT` | *(indent decrease)* | |
| `EOF` | *(end of token stream)* | |
| `ERROR` | *(unrecognised character)* | |

`WORD` tokens whose value matches a keyword entry are **reclassified** to their keyword type at lex time.

### 2.4 Keywords

The following identifiers are reserved and cannot be used as variable or definition names:

```
main    fire    act     bullet  bul
repeat  repeatf wait    waitf   vanish  break
chdir   chspd   chrotspd  chpos   accel   over
dir     speed   spd     rotspd  offset  pos     mvmt
aim     abs     rel     seq
x       y       type    bounces
emitter emt     async
if      elif    else    while
var     true    false
export  include
```

`bul` is an alias for `bullet`; `spd` is an alias for `speed`; `emt` is an alias for `emitter`.

Note: `num`, `str`, and `bool` are **not** keywords — they are parsed as plain `WORD` tokens in specific positions (after `export`).

---

## 3. Top-Level Structure

```
program = { include_decl }
          { export_decl }
          [ main_def ]
          { top_level_def }
          EOF
```

A valid executable script must have a `main` block. Library scripts (intended only for `include`) may omit `main`.

Top-level definitions may appear in any order after `main`, though `include` and `export` conventionally appear first. The parser accepts them intermixed.

### 3.1 Program layout example

```
include builtin

export num speed 200
export num count 5

main
    repeat
        act x_way(count, 0, speed)
        wait 0.5

fire myfire
    dir aim 0
    spd 300
```

---

## 4. Definitions

### 4.1 `main`

```
main_def = "main" NEWLINE action_block
```

The entry point. Executed when the emitter starts. There is exactly one `main` per executable script.

```
main
    repeat
        fire
            dir aim 0
            spd 200
        wait 0.3
```

### 4.2 `fire`

```
fire_def = "fire" IDENT [ param_list ] NEWLINE fire_block
```

A named fire pattern. Called with `fire <name>` or `fire <name>(args...)` from an action block.

```
fire spread(angle, spd_)
    dir aim angle
    spd spd_
```

### 4.3 `act`

```
act_def = "act" IDENT [ param_list ] NEWLINE action_block
```

A named action sequence. Called with `act <name>` or `act <name>(args...)`.

```
act circle(n, spd_)
    repeat n i
        fire
            dir abs (360 / n) * i
            spd spd_
```

### 4.4 `bullet`

```
bullet_def = ( "bullet" | "bul" ) IDENT [ param_list ] NEWLINE bullet_block
```

A named bullet definition. Referenced inside fire blocks with `bullet <name>` or `bul <name>`.

```
bullet tracker
    type enemy
    act
        repeat
            chdir
                dir aim 0
                over 0.5
            wait 0.5
```

### 4.5 Parameter lists

```
param_list = "(" IDENT { "," IDENT } ")"
```

Parameters are untyped identifiers. They are bound to positional arguments when the definition is called and placed in scope as `float` or `String` values (or first-class refs — see §12).

---

## 5. Blocks

All blocks are **indented regions**. The lexer emits `INDENT` at the start and `DEDENT` at the end. Each block must contain at least one statement.

```
action_block = INDENT { action_stmt NEWLINE } DEDENT
fire_block   = INDENT { fire_stmt   NEWLINE } DEDENT
bullet_block = INDENT { bullet_stmt NEWLINE } DEDENT
```

`chdir`, `chspd`, `chrotspd`, `chpos`, `accel`, `offset`, and `pos` have their own inner block forms (see §9).

---

## 6. Action Block Statements

Valid inside `main`, `act`, and any nested action block (`repeat`, `while`, `if`, inline `act`, etc.).

### 6.1 `wait`

```
wait EXPR
```

Pause execution for `EXPR` seconds (wall-clock time, unscaled).

```
wait 0.5
wait interval * 2
```

### 6.2 `waitf`

```
waitf EXPR
```

Pause for `EXPR` **physics frames**.

```
waitf 1
waitf frames
```

### 6.3 `vanish`

```
vanish
```

Stops execution of the current interpreter and emits a `vanished` signal to the bullet. Used inside bullet `act` blocks to make a bullet destroy itself.

### 6.4 `break`

```
break
```

Exits the innermost enclosing `repeat`, `repeatf`, or `while` loop. Only exits one level — `break` inside a nested loop only affects that loop, not any outer ones.

```
repeat
    if some_flag
        break       ← exits the repeat
    wait 0.1
```

### 6.5 `repeat`

```
repeat_stmt = "repeat" [ EXPR [ IDENT ] ] NEWLINE action_block
```

Loop. Forms:

| Form | Meaning |
|---|---|
| `repeat` | Infinite loop |
| `repeat EXPR` | Loop `EXPR` times |
| `repeat EXPR i` | Loop `EXPR` times; `i` holds the **0-based** iteration index |

The index variable is placed in scope for the duration of each iteration body and is **0-based** (first iteration: `i = 0`, last: `i = count - 1`). May be nested arbitrarily.

Variables declared with `var` inside the body are scoped to each iteration. Reassignments to outer-scope variables propagate back to the parent scope (see §15.2).

```
repeat
    fire
        dir aim 0
    wait 0.1

repeat 8 i
    fire
        dir abs 45 * i   # i = 0..7 → angles 0, 45, 90, ..., 315
        spd 200
```

**Note:** `EXPR` and the optional `IDENT` are distinguished by position: if the last token before the newline is a plain `WORD`, it is the index variable; everything before it is `EXPR`.

**Edge cases:**
- `repeat` → infinite loop, no count, no index
- `repeat 5` → count is `5`, no index
- `repeat n` → `n` is treated as count expression (no index), since there is nothing before it
- `repeat 5 i` → count `5`, index `i`
- `repeat count + 1 i` → count `count + 1`, index `i`

### 6.6 `repeatf`

```
repeatf_stmt = "repeatf" [ EXPR [ IDENT ] ] NEWLINE action_block
```

The body is executed synchronously (unlike `repeat`, no coroutine is spawned per frame). `wait`/`waitf` inside a `repeatf` body are silently ignored.

**Without N (infinite form):** Registers the body in `TamaManager` to run once per physics frame forever. This form is **terminal** — nothing after it in the same block executes.

**With N (finite form):** Runs the body N times, once per physics frame, then execution continues to the next statement. `IDENT` names the 0-based loop index, matching `repeat` semantics.

```
repeatf                              ← infinite, terminal
    chpos
        x abs spawn_x + radius * cos(time())
        y abs spawn_y + radius * sin(time())

repeatf 60 i                         ← finite: 60 frames, then continues
    chspd
        spd abs 100 + i * 5
wait 1.0                             ← runs after the 60 frames complete
```

### 6.7 `while`

```
while_stmt = "while" EXPR NEWLINE action_block
```

Evaluates `EXPR` before each iteration. If non-zero (truthy), executes the body and repeats. Exits when `EXPR` evaluates to zero.

`true` and `false` are valid literals (evaluating to `1.0` and `0.0` respectively). Variables declared with `var` inside the body are scoped to each iteration. Reassignments to outer-scope variables propagate back.

```
var spd_ 10
while spd_ < 500
    fire
        dir aim 0
        spd spd_
    spd_ spd_ + 10
    wait 0.2
```

### 6.8 `if` / `elif` / `else`

```
if_stmt = "if" EXPR NEWLINE action_block
          { "elif" EXPR NEWLINE action_block }
          [ "else" NEWLINE action_block ]
```

Conditional branching. `EXPR` is evaluated as a float; non-zero is truthy. At most one branch executes. Variables declared with `var` inside a branch are scoped to that branch. Reassignments to outer-scope variables propagate back.

```
if count >= 10
    fire
        dir abs 0
        spd 300
elif count >= 5
    fire
        dir aim 0
        spd 200
else
    wait 0.1
```

### 6.9 `var` / assignment

```
var_stmt    = "var" IDENT EXPR
assign_stmt = IDENT EXPR
```

**`var`** declares a new variable in the current scope. The variable is visible in all nested blocks (repeat, while, if/elif/else, inline acts, fire calls) but is erased when its enclosing block exits.

**Assignment** (`IDENT EXPR` with no keyword) reassigns a variable that is already in scope. The assignment writes to the shared scope dictionary, so it propagates back to parent blocks (see §15.2 for full scoping rules).

`IDENT` must not be a keyword. `EXPR` may reference any in-scope variable, loop index, or context function. Bare qualifier keywords (`aim`, `abs`, `rel`, `seq`) and bool literals (`true`, `false`) are valid values.

```
var count 8
var dir_type aim
var active true

count count + 1          # reassignment
dir_type abs             # store a qualifier string
```

### 6.10 `dir`

```
dir [ DIR_QUALIFIER ] EXPR
```

Sets the direction for the **next** fire statement in this scope. See §9.1.

### 6.11 `speed` / `spd`

```
( "speed" | "spd" ) [ VALUE_QUALIFIER ] EXPR
```

Sets the speed for the next fire. See §9.2.

### 6.12 `offset`

Sets the spawn position offset. See §9.3.

### 6.13 `chdir`

```
chdir_stmt = "chdir" NEWLINE chdir_block
chdir_block = INDENT { ( dir_stmt | over_stmt ) NEWLINE } DEDENT
```

Emits a direction-change command to the bullet. `dir` is required; `over` is optional and defaults to `0`. When `over` is `0` the direction is set instantly without tweening.

```
chdir           ← instant
    dir aim 0

chdir           ← tweened
    dir abs 90
    over 1.0    ← transition time in seconds
```

At runtime this emits a `changed_direction` signal with the target direction and transition duration.

### 6.14 `chspd`

```
chspd_stmt = "chspd" NEWLINE chspd_block
chspd_block = INDENT { ( speed_stmt | over_stmt ) NEWLINE } DEDENT
```

Emits a speed-change command. `speed`/`spd` is required; `over` is optional and defaults to `0`. When `over` is `0` the speed is set instantly without tweening.

```
chspd           ← instant
    spd abs 400

chspd           ← tweened
    spd abs 400
    over 2.0
```

### 6.15 `chrotspd`

```
chrotspd_stmt = "chrotspd" NEWLINE chrotspd_block
chrotspd_block = INDENT { ( speed_stmt | over_stmt ) NEWLINE } DEDENT
```

Emits a rotation-speed change command to the bullet. `speed`/`spd` is required; `over` is optional and defaults to `0`. When `over` is `0` the rotation speed is set instantly without tweening. Rotation speed is measured in **degrees per second** and is applied each frame as `angle += rot_speed × (π/180) × delta`.

| Speed qualifier | Meaning |
|---|---|
| `abs` (default) | Set rotation speed directly. |
| `rel` | Add to the bullet's current rotation speed. |
| `seq` | Add to the bullet's last rotation speed (before this command). |

```
chrotspd        ← instant
    spd abs 90  ← 90°/sec clockwise

chrotspd        ← tweened
    spd abs 90
    over 0.5    ← ramp up over half a second

chrotspd        ← stop spinning
    spd abs 0
```

### 6.16 `chpos`

```
chpos_stmt  = "chpos" NEWLINE chpos_block
chpos_block = INDENT { ( axis_stmt | over_stmt ) NEWLINE } DEDENT
axis_stmt   = ( "x" | "y" ) [ VALUE_QUALIFIER ] EXPR
```

Emits a position-change command to the bullet. At least one of `x`/`y` is required. `over` is optional and defaults to `0`. When `over` is `0` the bullet is moved instantly without tweening.

| Qualifier | Meaning |
|---|---|
| `abs` (default) | World coordinate |
| `rel` | Offset from the bullet's current position |

```
chpos
    x abs mid_x()
    y abs mid_y()
    over 1.5
```

At runtime this emits a `changed_position` signal. The bullet node is responsible for implementing the movement.

### 6.17 `accel`

```
accel_stmt = "accel" NEWLINE accel_block
accel_block = INDENT { ( axis_stmt | over_stmt ) NEWLINE } DEDENT
axis_stmt   = ( "x" | "y" ) [ VALUE_QUALIFIER ] EXPR
```

Emits an acceleration command on world axes. At least one of `x`/`y` is required; `over` is optional and defaults to `0`. When `over` is `0` the axis velocity is set instantly without tweening. Default qualifier for each axis is `abs`.

```
accel
    x 50
    y -100
    over 1.5
```

### 6.18 `fire` (inline)

```
inline_fire = "fire" NEWLINE fire_block
```

An anonymous fire block. Equivalent to a named fire def but written inline.

```
fire
    dir aim 0
    spd 200
```

### 6.19 `fire <name>` (named call)

```
fire_call = "fire" IDENT [ arg_list ]
```

Calls a named fire definition.

```
fire spread
fire spread(45, 300)
```

### 6.20 `act` (inline)

```
inline_act = "act" NEWLINE action_block
```

An anonymous action block, executed inline (or async — see §6.20).

```
act
    wait 1.0
    fire
        dir aim 0
        spd 200
```

### 6.21 `act <name>` (named call)

```
act_call = "act" IDENT [ arg_list ]
```

Calls a named act definition.

```
act circle
act circle(8, 200)
```

### 6.22 `async`

```
async_stmt = "async" ( inline_act | act_call )
```

Fires an act without blocking. Execution of the current block continues immediately on the next statement. The async act runs in parallel.

```
async act spiral(12, 0.1, 200)
async act
    repeat
        fire
            dir seq 30
            spd 150
        wait 0.2
```

The interpreter tracks async act count and waits for all async acts to finish before `start()` resolves (so the emitter doesn't stop prematurely).

---

## 7. Fire Block Statements

Valid inside `fire` definitions and inline `fire` blocks.

```
fire_stmt = dir_stmt | speed_stmt | rotspd_stmt | offset_stmt | pos_stmt | bullet_call | inline_bullet
```

All properties are optional. A fire with no `bullet` uses the registry's default bullet. When both `offset` and `pos` are present, `pos` takes priority.

### 7.1 `bullet <name>` (named call)

```
bullet_call = ( "bullet" | "bul" ) IDENT [ arg_list ] NEWLINE
```

References a named bullet definition, optionally passing arguments.

### 7.2 `bullet` (inline)

```
inline_bullet = ( "bullet" | "bul" ) NEWLINE bullet_block
```

An anonymous inline bullet definition (see §8).

### 7.3 `rotspd`

```
rotspd_stmt = "rotspd" [ VALUE_QUALIFIER | IDENT ] EXPR
```

Sets the bullet's initial rotation speed in degrees per second. The bullet's `angle` accumulates at this rate each frame (`angle += rot_speed × (π/180) × delta`), causing it to curve continuously.

**Default qualifier:** `abs`

| Qualifier | Meaning |
|---|---|
| `abs` | Set rotation speed directly. |
| `rel` | Add to the spawner's last fired rotation speed. |
| `seq` | Same as `rel`. |

Like `dir` and `speed`, the qualifier may be a scope variable holding `"abs"`, `"rel"`, or `"seq"`.

```
fire
    dir abs 0
    spd 200
    rotspd 90       ← 90°/sec clockwise; bullet curves downward

fire
    dir aim 0
    spd 150
    rotspd rel 30   ← 30°/sec added to last fired rotation speed
```

### 7.4 Order

Properties can appear in any order inside a fire block. Only one `bullet` call is permitted per fire block.

---

## 8. Bullet Block Statements

Valid inside `bullet` definitions and inline `bullet` blocks.

```
bullet_stmt = type_stmt | emitter_stmt | bounces_stmt | mvmt_stmt | act_call | inline_act
```

### 8.1 `type`

```
type_stmt = "type" IDENT NEWLINE
```

Sets the bullet's gameplay scene type. The IDENT is looked up in the `TamaBulletRegistry`. If omitted, the registry default is used.

```
type enemy
type spawner
```

### 8.2 `emitter` / `emt`

```
emitter_stmt = ( "emitter" | "emt" ) ( IDENT [ arg_list ] NEWLINE | NEWLINE action_block )
```

Attaches a firing emitter to the bullet. Two forms:

- **Named act**: `emt my_act` — references a top-level `act` definition by name (optionally with args). That act is run as the bullet's spawner emitter.
- **Inline**: `emt` followed by an indented action block — an anonymous emitter body written inline.

The emitter interpreter runs in parallel with the bullet's `act` if both are present (separate interpreter instances).

```
bullet spawner_bul
    type spawner
    emt
        repeat
            fire
                dir aim 0
                spd 150
            wait 0.3
```

### 8.3 `mvmt`

```
mvmt_stmt      = "mvmt" NEWLINE mvmt_block
mvmt_block     = INDENT { mvmt_axis_stmt NEWLINE } DEDENT
mvmt_axis_stmt = ( "x" | "y" ) [ VALUE_QUALIFIER ] EXPR
```

Defines a per-frame position expression that is re-evaluated every physics frame on the bullet. Default qualifier is `abs`.

| Qualifier | Meaning |
|---|---|
| `abs` (default) | World coordinate — sets the bullet's global position directly |
| `rel` | Offset from the bullet's spawn position |

The expression has access to the bullet's scope (params and exports) plus context functions like `time()` and `spawn_x`/`spawn_y`.

```
bullet orbiter(radius, phase)
    mvmt
        x abs mid_x() + radius * cos(time() + phase)
        y abs mid_y() + radius * sin(time() + phase)
```

### 8.4 `act` (inline or named call)

```
act_stmt = inline_act | act_call
```

Runs an action sequence on the bullet after it spawns. Used for movement, direction changes, self-destruction (`vanish`), etc.

```
bullet homing
    type enemy
    act
        repeat
            chdir
                dir aim 0
                over 0.3
            wait 0.3
```

### 8.5 `bounces`

```
bounces_stmt = "bounces" [ EXPR ] [ axis ]
axis         = "x" | "y"
```

Declares that the bullet reflects off screen borders instead of despawning when it reaches them. All parts are optional:

| Form | Meaning |
|---|---|
| `bounces` | Infinite bounces off all four borders |
| `bounces N` | Up to `N` bounces off all four borders, then exit normally |
| `bounces x` | Infinite bounces off left/right walls only |
| `bounces y` | Infinite bounces off top/bottom walls only |
| `bounces N x` | Up to `N` bounces off left/right walls only |
| `bounces N y` | Up to `N` bounces off top/bottom walls only |
| `bounces -1` | Explicit infinite — same as bare `bounces` |

`EXPR` is evaluated at fire time using the emitter's scope (exports, `var` variables, etc.) and rounded to an integer. Use `-1` to express infinite bounces explicitly in an expression.

After the last allowed bounce the bullet continues in its reflected direction and despawns via the normal out-of-bounds check. The bullet is **not** destroyed at the moment of the final bounce.

**Reflection math:** the bullet's angle is stored as a float in radians internally. Hitting a left/right wall negates the x-component of velocity (`angle = π − angle`); hitting a top/bottom wall negates the y-component (`angle = −angle`). Any independent `speed_x`/`speed_y` acceleration is reflected the same way.

`bounces` has no effect on bullets using `mvmt` expressions — position is controlled by the expression and border reflection cannot be applied.

```
bullet wall_bouncer
    bounces 3           ← 3 bounces off any border, then exits

bullet pinball
    bounces             ← infinite bounces off all borders

bullet side_only
    bounces x           ← infinite bounces off left/right walls only

bullet top_bottom
    bounces 2 y         ← 2 bounces off top/bottom, then exits

bullet param_bounces(n)
    bounces n           ← count from caller
```

---

## 9. Shared Sub-Statements

### 9.1 `dir`

```
dir_stmt = "dir" [ DIR_QUALIFIER ] EXPR NEWLINE
```

**Default qualifier:** `aim`

| Qualifier | Meaning |
|---|---|
| `aim` | Angle relative to the direction toward the player. `dir aim 0` points directly at the player; `dir aim 45` is 45° offset from aim. |
| `abs` | Absolute angle in degrees (0° = right, increases clockwise). |
| `rel` | Relative to the spawner's current rotation (in degrees). |
| `seq` | Relative to the last fired bullet's angle (in degrees). Accumulates per-spawner. |

The qualifier may also be a **scope variable** holding a string (`"aim"`, `"abs"`, `"rel"`, or `"seq"`), enabling dynamic dispatch:

```
export str dir_mode aim

fire
    dir dir_mode 45
```

### 9.2 `speed` / `spd`

```
speed_stmt = ( "speed" | "spd" ) [ VALUE_QUALIFIER ] EXPR NEWLINE
```

**Default qualifier:** `abs`

| Qualifier | Meaning |
|---|---|
| `abs` | Absolute speed value. |
| `rel` | Relative to the spawner's last fired speed. |
| `seq` | Same as `rel` — adds to last speed. |

Like `dir`, the qualifier may be a scope variable holding `"abs"`, `"rel"`, or `"seq"`.

### 9.3 `offset`

Two forms:

**Inline:**
```
offset EXPR NEWLINE
```
Offsets the spawn position along the bullet's local axis (rotated by the bullet's angle). Positive values push in the local-forward direction; negative values push backward.

**Block:**
```
offset NEWLINE
    x [ VALUE_QUALIFIER ] EXPR
    y [ VALUE_QUALIFIER ] EXPR
```
Offsets along world X and Y axes independently. Default qualifier for each axis is `rel`.

| Qualifier | Meaning |
|---|---|
| `abs` / `seq` | World-space offset added to the spawner's global position (`spawner.global_position + offset`). |
| `rel` | Local-axis offset: the full offset vector is rotated by the bullet's angle before being added to the spawner's position. Use this to displace a bullet sideways or forward relative to its travel direction. |

Per-axis types are independent; when mixing `abs` and `rel` on different axes, each contributes to its respective world/local component and they are combined after rotation.

### 9.4 `pos`

```
pos NEWLINE
    x [ VALUE_QUALIFIER ] EXPR
    y [ VALUE_QUALIFIER ] EXPR
```

Sets the bullet's spawn position directly. Default qualifier for each axis is `abs`. When `pos` is present it takes priority over `offset`.

| Qualifier | Meaning |
|---|---|
| `abs` / `seq` | Sets global position directly (world coordinates). |
| `rel` | Adds to the spawner's global position (`spawner.global_position + value`). |

Both axes are optional; unspecified axes inherit the spawner's position for that axis.

```
fire
    dir abs 0
    speed 130
    pos
        x abs spawn_x
        y abs spawn_y
```

### 9.5 `over`

```
over_stmt = "over" EXPR NEWLINE
```

Transition duration in seconds. Used inside `chdir`, `chspd`, `chrotspd`, `chpos`, and `accel` blocks. Optional in all — omitting it is equivalent to `over 0`. When the resolved value is `0`, the change is applied instantly (no tween created). Evaluated as a float.

---

## 10. Expressions

Expressions (`EXPR`) are **raw token sequences** collected to end-of-line (or to a matching `)` inside argument lists). They are evaluated at runtime using Godot's built-in `Expression` class.

### 10.1 What's available

- Numeric literals: `200`, `3.14`
- Boolean literals: `true` (evaluates to `1.0`), `false` (evaluates to `0.0`)
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Parentheses: `(expr)`
- Scope variables: any identifier that is a parameter, export, or `var`-declared variable in scope
- Godot built-in functions available via `Expression`: `sin`, `cos`, `abs`, `max`, `min`, `sqrt`, `floor`, `ceil`, `round`, `pow`, `PI`, etc.
- Context methods: `time()` — returns elapsed time in seconds (provided by `TamaContext`)

### 10.2 Scope

The expression evaluator receives only **numeric** (float/int/bool) values from the current scope. Non-numeric scope values (strings, TamaRefs, inline nodes) are filtered out before evaluation. To use a string value as a qualifier, reference it by name directly in the qualifier position rather than in an expression.

### 10.3 Examples

```
dir aim (360 / count) * i
spd abs 100 + speed_bonus
wait sin(time()) * 0.5 + 0.5
repeat 2 * waves

var active true
if active
    fire
        dir aim 0
```

---

## 11. Qualifiers

Several statements accept an optional qualifier keyword that changes interpretation.

### Dir qualifiers

| Token | Value | Usage |
|---|---|---|
| `aim` | default | Aim toward player + offset |
| `abs` | explicit | Absolute world angle |
| `rel` | explicit | Relative to spawner rotation |
| `seq` | explicit | Relative to last fired angle |

### Value qualifiers (speed, offset axes, pos axes, accel axes, chpos axes)

| Token | Value | Usage |
|---|---|---|
| `abs` | default for speed/pos/accel/chpos | Absolute value |
| `rel` | default for offset axes | Relative / additive |
| `seq` | explicit | Same as `rel` for speed; same as `rel` for offset |

### Dynamic qualifiers

If the parser sees a `WORD` token (not a qualifier keyword) before the expression in a `dir` or `speed` statement, and there is no newline immediately after it, it is treated as a **variable name** whose runtime string value supplies the qualifier. This allows exporting the qualifier as a `str` export.

**Parser rule:** after the `dir`/`speed`/`spd` keyword, if the next token is `WORD` and the token after that is not `NEWLINE`, treat the word as a variable qualifier rather than the start of the expression.

---

## 12. First-Class Definitions

Acts, fires, and bullets can be passed as **first-class values** in argument lists. This lets you parameterise patterns.

### 12.1 Passing a definition as an argument

In a call site, a `WORD` that matches a top-level definition name (pre-scanned during parsing) and appears immediately before `,` or `)` is parsed as a `RefCallArg` rather than an expression string.

```
act x_way(8, spread_fire, 200)
```

Here, if `spread_fire` is a known fire definition, it is passed by reference.

You can also partially apply arguments:

```
act x_way(8, spread_fire(30), 200)
```

This binds `30` as the first argument to `spread_fire`, creating a partial application.

### 12.2 Receiving first-class values

Parameters holding refs are in scope as `TamaRef` objects. They are resolved transparently when called:

```
act x_way(n, f, spd_)
    repeat n i
        fire f           # if f is a fire ref, fires it
        # or: act f      # if f is an act ref
```

### 12.3 Inline blocks as arguments

Inline `act`, `fire`, `bullet`, and `emitter` blocks can be passed as arguments:

```
act x_way(8,
    fire
        dir aim 0
        spd 200
, 300)
```

### 12.4 Parser disambiguation

The parser pre-scans all top-level definition names before parsing bodies. At each argument position, the parser checks:

1. If the next token is `WORD` followed by `,` or `)` and the word is a known definition → `RefCallArg`
2. If the next token is `WORD` followed by `(` and the word is a known definition → `RefCallArg` with sub-args
3. If the next token is a block keyword (`act`, `fire`, `bullet`, `emt`) followed by `NEWLINE` → inline block node
4. If the qualifier keywords `aim`, `abs`, `rel`, `seq` appear alone before `,` or `)` → literal string
5. Otherwise → raw expression string collected to `,` or `)`

---

## 13. Include System

```
include_decl = "include" IDENT NEWLINE
```

Includes another TamaScript file by name (without extension). The included script is looked up in `res://tamascripts/` with extensions `.tama` and `.tam` tried in order.

```
include builtin
include utils
```

**Semantics:**
- All top-level `fire`, `act`, and `bullet` definitions from the included file are merged into the including file's program.
- Exports and `main` from the included file are **not** imported.
- Includes are resolved at **parse time** — the included program is fully parsed before the including file's reference resolution runs, so included names are available for first-class ref detection.
- Circular includes are detected and rejected with a warning.
- Nested includes work: an included file may itself include other files.
- Library files (e.g. `builtin.tama`) typically have no `main` block.

---

## 14. Export Declarations

```
export_decl = "export" ( "num" | "str" | "bool" ) IDENT [ DEFAULT_EXPR ] NEWLINE
```

Declares a named variable that is exposed as an editable field in the game engine's inspector (TamaEmitter). Must appear before `main`.

| Type | Maps to | Inspector widget | Usage |
|---|---|---|---|
| `num` | `float` | Number field | Numeric parameters (speed, count, angle, etc.) |
| `str` | `String` | Text field | Qualifier parameters (e.g. `"aim"`, `"abs"`, `"rel"`, `"seq"`) |
| `bool` | `bool` | Checkbox | Boolean flags |

The optional `DEFAULT_EXPR` is the default value used when no override has been set. For `num` it is parsed as a float literal; for `str` it is the remainder of the line as a string; for `bool` it must be `true` or `false`.

```
export num speed 200
export num count 8
export str dir_mode aim
export bool fire_enabled true
```

At runtime, exported values are injected into the initial scope before `main` executes. They behave exactly like parameters.

**Note:** `num`, `str`, and `bool` are parsed as plain identifiers, not keywords. They are only meaningful in the `export` position.

---

## 15. Runtime Semantics

### 15.1 Execution model

- The interpreter is a GDScript coroutine (`async/await`).
- `wait` and `waitf` yield the coroutine, allowing the game loop to run.
- Multiple interpreters can run concurrently (e.g. async acts, bullet emitters).
- `stop()` sets a `_running` flag; all `await` points check this flag and exit early.

### 15.2 Scope

Scope is a `Dictionary` mapping name → value. The model distinguishes two kinds of block entry:

**Shared scope (anonymous nested blocks):** `repeat` bodies, `while` bodies, `if`/`elif`/`else` branches, and inline `act` blocks all execute with the **same** dictionary as their parent. There is no copy on entry. This means:
- `var` declarations inside these blocks are tracked via a pre-keys snapshot taken before the block runs. Any keys added during the block that were not in the snapshot are erased when the block exits, preventing them from leaking to the outer scope.
- Bare `IDENT EXPR` reassignments write directly to the shared dict. These changes are **visible to the parent** after the block returns (they propagate outward).

**Copied scope (named act calls):** `act <name>(args...)` calls create a new dictionary: the outer scope is duplicated and parameters are overlaid on the copy. Assignments inside the called act do not propagate back to the caller. This is function-call semantics.

**Async inline acts:** Receive `scope.duplicate()` at the time of spawning so that the async coroutine does not race against the parent scope.

**Scope values** can be `float`, `int`, `bool`, `String`, `TamaRef`, or inline AST nodes. Only `float`/`int`/`bool` values are passed to Godot's `Expression` evaluator; non-numeric values are filtered out and must be accessed via qualifier or first-class ref mechanisms.

**Export values** seed the initial scope before `main` executes. Parameters shadow outer scope variables by the same name.

### 15.3 Direction resolution

Angles use **radians** internally. `dir` expressions are in **degrees**. The resolver converts:

| Qualifier | Formula |
|---|---|
| `aim` | `angle_to_player + deg_to_rad(expr)` |
| `abs` | `deg_to_rad(expr)` |
| `rel` | `spawner.rotation + deg_to_rad(expr)` |
| `seq` | `last_fired_angle + deg_to_rad(expr)` |

### 15.4 Speed resolution

| Qualifier | Formula |
|---|---|
| `abs` | `expr` |
| `rel` | `last_fired_speed + expr` |
| `seq` | `last_fired_speed + expr` |

`last_fired_angle` and `last_fired_speed` are stored on the spawner node and updated after each fire.

### 15.5 Bullet lifecycle

When `fire` executes:

1. `bullet_fired` signal is emitted with a `BulletFireData` payload.
2. The spawn manager receives the signal and instantiates the bullet scene.
3. The bullet's `act` (if any) is started via `start_act()` after the bullet enters the scene tree.
4. The bullet's `emitter` (if any) is started in parallel (separate interpreter instance).

### 15.6 `vanish`

Sets `_running = false` on the interpreter and emits `vanished`. The game-side bullet node listens to `vanished` to destroy itself.

### 15.7 `chdir` / `chspd` / `chrotspd` / `chpos` / `accel`

These are **fire-and-forget** signals to the bullet. The interpreter emits the signal and immediately continues — it does not wait for the transition to complete. The bullet node is responsible for implementing the transition. When `over` is `0` (or omitted), the value is applied immediately without creating a tween; when `over` > `0`, a linear tween runs for that many seconds.

`chrotspd` changes the bullet's rotation speed (degrees/sec). The rotation speed is applied every physics frame as `angle += rot_speed × (π/180) × delta`, causing the bullet to curve. `rotspd` in a `fire` block sets the initial rotation speed at spawn time.

### 15.8 `repeatf`

**Infinite form (no N):** The body is registered with `TamaManager` as a per-frame callback. On each physics frame, `TamaManager` calls the body synchronously. The interpreter sets `_running = false` after registering and does not resume.

**Finite form (with N):** The interpreter loops N times, calling `_exec_body_sync` each iteration and then awaiting `get_tree().physics_frame` between iterations (not after the last). After N iterations, execution continues normally.

### 15.9 Expression evaluation

Expressions are evaluated with Godot's `Expression` class. Only `float`/`int`/`bool` scope values are passed as named bindings (`bool` is converted to `float` before passing). Non-numeric values (strings, refs) are filtered out. The `TamaContext` object provides callable methods like `time()` as the expression's base object.

---

## 16. Parser Behaviour Notes

These notes are specifically useful for implementing a language server, linter, or formatter.

### 16.1 Two-pass parsing

The parser does a **pre-scan** before full parsing:

1. Scan all tokens at depth 0 to collect defined names into `_defined_fires`, `_defined_acts`, `_defined_bullets`.
2. Resolve `include` directives encountered during the pre-scan (running a recursive parse on the included file via the resolver callable).
3. Pre-populate definition maps from included programs.

This pre-scan is what enables first-class ref detection in argument positions during the main parse pass.

### 16.2 Error recovery

The parser records errors as diagnostics `(line, col, length, message)` but continues parsing. Multiple errors can be returned from a single parse. A valid program node is always returned even if there were errors.

### 16.3 Reference resolution (post-parse)

After the main parse loop, the parser resolves all recorded references (`fire`, `act`, `bullet` calls) against the definition maps. Unknown references produce additional diagnostics. This happens after all definitions have been collected.

### 16.4 Qualifier vs variable name ambiguity

After `dir`/`speed`/`spd`, if the next token is a `WORD` (not a qualifier keyword) and the token after it is **not** `NEWLINE`, the word is treated as a variable name for the qualifier (dynamic qualifier). If it is a qualifier keyword (`aim`, `abs`, `rel`, `seq`), it is consumed as the qualifier. Otherwise, the qualifier defaults and the expression starts immediately.

**Edge case:** `dir aim 0` — `aim` is a keyword, consumed as qualifier; `0` is the expression.  
**Edge case:** `dir myvar 0` — `myvar` is a word not followed by NEWLINE; treated as a dynamic qualifier variable; `0` is the expression.  
**Edge case:** `dir 45` — no qualifier word; default qualifier (`aim`) used; `45` is the expression.

### 16.5 `repeat` index variable

The last token on the `repeat` line, if it is a plain `WORD`, is the index variable. Everything before it is the count expression. This is resolved by scanning to end-of-line, checking if the last token is `WORD`, and splitting accordingly.

### 16.6 Inline block detection

All block-start keywords (`act`, `fire`, `bullet`, `emt`/`emitter`) are "inline" when followed immediately by `NEWLINE`. When followed by anything else they are a call with an identifier and optional args.

### 16.7 Include resolution timing

Includes are resolved in the **pre-scan**, not the main parse loop. By the time `_parse_include` runs in the main loop, the resolved program is already in the cache and is simply merged into the program node arrays.

### 16.8 `num` / `str` / `bool` are not keywords

The parser reads them as `WORD` tokens and validates their value explicitly. A typo like `export int foo` is caught at the parser level, not the lexer level.

### 16.9 `main` is optional

If `main` is absent (library file), `program.main` is null. The interpreter checks for null and reports an error only if `start()` is called. Library files are safe to parse.

### 16.10 `var` vs assignment disambiguation

`var IDENT EXPR` declares a new variable. A bare `IDENT EXPR` (no keyword) reassigns an existing one. Both produce the same AST node (`VarDeclNode`) — the distinction is purely textual. At runtime, `var` and assignment are executed identically: `scope[name] = eval(expr)`. The semantic difference (declaration vs reassignment) is enforced by the pre-keys scoping mechanism, not the interpreter.

---

## 17. Complete Keyword List

| Keyword | Aliases | Context |
|---|---|---|
| `main` | — | Top-level def |
| `fire` | — | Top-level def, action stmt, fire block |
| `act` | — | Top-level def, action stmt, bullet stmt |
| `bullet` | `bul` | Top-level def, fire block stmt |
| `emitter` | `emt` | Bullet stmt — references or inlines an act as the bullet's spawner |
| `repeat` | — | Action stmt |
| `repeatf` | — | Action stmt |
| `wait` | — | Action stmt |
| `waitf` | — | Action stmt |
| `vanish` | — | Action stmt |
| `break` | — | Action stmt |
| `while` | — | Action stmt |
| `if` | — | Action stmt |
| `elif` | — | Action stmt (if branch) |
| `else` | — | Action stmt (if branch) |
| `var` | — | Action stmt |
| `true` | — | Boolean literal (1.0) |
| `false` | — | Boolean literal (0.0) |
| `async` | — | Action stmt modifier |
| `dir` | — | Fire/action/chdir stmt |
| `speed` | `spd` | Fire/action/chspd stmt |
| `offset` | — | Fire/action stmt |
| `pos` | — | Fire stmt |
| `mvmt` | — | Bullet stmt |
| `bounces` | — | Bullet stmt — border reflection declaration |
| `chdir` | — | Action stmt |
| `chspd` | — | Action stmt |
| `chrotspd` | — | Action stmt — change bullet rotation speed (degrees/sec) |
| `chpos` | — | Action stmt |
| `accel` | — | Action stmt |
| `over` | — | chdir/chspd/chrotspd/chpos/accel inner stmt |
| `rotspd` | — | Fire stmt — set initial bullet rotation speed (degrees/sec) |
| `type` | — | Bullet stmt |
| `aim` | — | Dir qualifier |
| `abs` | — | Dir/value qualifier |
| `rel` | — | Value qualifier |
| `seq` | — | Dir/value qualifier |
| `x` | — | Offset/pos/accel/chpos axis |
| `y` | — | Offset/pos/accel/chpos axis |
| `export` | — | Top-level directive |
| `include` | — | Top-level directive |

---

## 18. Full Grammar (EBNF)

```ebnf
(* Notation: A B = sequence, A | B = alternation, [A] = optional,
   {A} = zero or more, (A) = grouping, "x" = literal, UPPER = token *)

program       = { include_decl }
                { export_decl }
                [ main_def ]
                { top_level_def }
                EOF ;

include_decl  = "include" IDENT NEWLINE ;

export_decl   = "export" ( "num" | "str" | "bool" ) IDENT [ EXPR ] NEWLINE ;

top_level_def = fire_def | act_def | bullet_def ;

main_def      = "main" NEWLINE action_block ;

fire_def      = "fire" IDENT [ param_list ] NEWLINE fire_block ;

act_def       = "act"  IDENT [ param_list ] NEWLINE action_block ;

bullet_def    = ( "bullet" | "bul" ) IDENT [ param_list ] NEWLINE bullet_block ;

param_list    = "(" IDENT { "," IDENT } ")" ;
arg_list      = "(" arg { "," arg } ")" ;
arg           = inline_act | inline_fire | inline_bullet | inline_emitter
              | QUALIFIER_WORD               (* aim/abs/rel/seq alone *)
              | ref_call_arg                 (* known def name, optional pre-args *)
              | EXPR ;                       (* raw expression to "," or ")" *)
ref_call_arg  = IDENT [ "(" arg { "," arg } ")" ] ;


(* ---- Blocks ---- *)

action_block  = INDENT { action_stmt NEWLINE } DEDENT ;
fire_block    = INDENT { fire_stmt   NEWLINE } DEDENT ;
bullet_block  = INDENT { bullet_stmt NEWLINE } DEDENT ;


(* ---- Action statements ---- *)

action_stmt   = while_stmt
              | if_stmt
              | var_stmt
              | assign_stmt
              | repeat_stmt
              | repeatf_stmt
              | wait_stmt
              | waitf_stmt
              | vanish_stmt
              | break_stmt
              | dir_stmt
              | speed_stmt
              | offset_stmt
              | chdir_stmt
              | chspd_stmt
              | chrotspd_stmt
              | chpos_stmt
              | accel_stmt
              | inline_act
              | inline_fire
              | act_call
              | fire_call
              | async_stmt ;

while_stmt    = "while" EXPR NEWLINE action_block ;
                (* EXPR evaluated before each iteration; non-zero is truthy.
                   var declarations inside are scoped per iteration;
                   reassignments propagate outward.                          *)

if_stmt       = "if" EXPR NEWLINE action_block
                { "elif" EXPR NEWLINE action_block }
                [ "else" NEWLINE action_block ] ;
                (* var declarations inside a branch are scoped to that branch;
                   reassignments propagate outward.                           *)

var_stmt      = "var" IDENT EXPR ;
                (* declares a new variable; visible in nested blocks but erased
                   when the enclosing block exits.                            *)

assign_stmt   = IDENT EXPR ;
                (* reassigns a variable already in scope; propagates to parent
                   blocks. named act calls are excluded (function-call semantics). *)

repeat_stmt   = "repeat" [ EXPR [ IDENT ] ] NEWLINE action_block ;
                (* IDENT is the 0-based loop index; omitting EXPR = infinite loop.
                   var declarations inside are scoped per iteration;
                   reassignments propagate outward.                          *)

repeatf_stmt  = "repeatf" [ EXPR [ IDENT ] ] NEWLINE action_block ;
                (* no N: infinite, terminal; with N: runs N frames then continues.
                   IDENT is the 0-based loop index.                           *)

wait_stmt     = "wait"  EXPR ;
waitf_stmt    = "waitf" EXPR ;
vanish_stmt   = "vanish" ;
break_stmt    = "break" ;
async_stmt    = "async" ( inline_act | act_call ) ;
fire_call     = "fire"  IDENT [ arg_list ] ;
act_call      = "act"   IDENT [ arg_list ] ;
inline_fire   = "fire"  NEWLINE fire_block ;
inline_act    = "act"   NEWLINE action_block ;


(* ---- Fire statements ---- *)

fire_stmt     = dir_stmt | speed_stmt | rotspd_stmt | offset_stmt | pos_stmt
              | bullet_call | inline_bullet ;
bullet_call   = ( "bullet" | "bul" ) IDENT [ arg_list ] ;
inline_bullet = ( "bullet" | "bul" ) NEWLINE bullet_block ;


(* ---- Bullet statements ---- *)

bullet_stmt   = type_stmt | emitter_stmt | bounces_stmt | mvmt_stmt | act_call | inline_act ;
type_stmt     = "type" IDENT ;
emitter_stmt  = ( "emitter" | "emt" ) ( IDENT [ arg_list ] | NEWLINE action_block ) ;
bounces_stmt  = "bounces" [ EXPR ] [ "x" | "y" ] ;
                (* reflect off screen borders instead of despawning.
                   EXPR: max bounces; omit or -1 for infinite.
                   "x": left/right walls only; "y": top/bottom walls only; omit = all.
                   trailing "x"/"y" is detected by checking the last token before NEWLINE. *) 
mvmt_stmt     = "mvmt" NEWLINE mvmt_block ;
mvmt_block    = INDENT { ( "x" | "y" ) [ VALUE_QUALIFIER | IDENT ] EXPR NEWLINE } DEDENT ;
                (* default VALUE_QUALIFIER = abs;
                   abs: sets world position each frame;
                   rel: offset from spawn position each frame.              *)


(* ---- Shared property statements ---- *)

dir_stmt    = "dir"   [ DIR_QUALIFIER | IDENT ] EXPR ;
speed_stmt  = ( "speed" | "spd" ) [ VALUE_QUALIFIER | IDENT ] EXPR ;

DIR_QUALIFIER   = "aim" | "abs" | "rel" | "seq" ;
VALUE_QUALIFIER = "abs" | "rel" | "seq" ;

offset_stmt  = "offset" ( NEWLINE offset_block | EXPR ) ;
offset_block = INDENT
                   { ( "x" | "y" ) [ VALUE_QUALIFIER | IDENT ] EXPR NEWLINE }
               DEDENT ;
               (* default VALUE_QUALIFIER = rel                              *)

pos_stmt    = "pos" NEWLINE pos_block ;
pos_block   = INDENT
                  { ( "x" | "y" ) [ VALUE_QUALIFIER | IDENT ] EXPR NEWLINE }
              DEDENT ;
              (* default VALUE_QUALIFIER = abs;
                 takes priority over offset when both present                *)

chdir_stmt  = "chdir" NEWLINE chdir_block ;
chdir_block = INDENT { ( dir_stmt | over_stmt ) NEWLINE } DEDENT ;
              (* dir required; over optional — defaults to 0 (instant)      *)

chspd_stmt  = "chspd" NEWLINE chspd_block ;
chspd_block = INDENT { ( speed_stmt | over_stmt ) NEWLINE } DEDENT ;
              (* speed required; over optional — defaults to 0 (instant)    *)

chrotspd_stmt  = "chrotspd" NEWLINE chrotspd_block ;
chrotspd_block = INDENT { ( speed_stmt | over_stmt ) NEWLINE } DEDENT ;
                 (* speed required (degrees/sec); over optional — defaults to 0 (instant)
                    abs: set directly; rel: add to current; seq: add to last  *)

rotspd_stmt  = "rotspd" [ VALUE_QUALIFIER | IDENT ] EXPR ;
               (* default VALUE_QUALIFIER = abs;
                  initial rotation speed in degrees/sec applied at spawn.
                  rel/seq: add to spawner's last fired rotation speed.        *)

chpos_stmt  = "chpos" NEWLINE chpos_block ;
chpos_block = INDENT { ( axis_stmt | over_stmt ) NEWLINE } DEDENT ;
              (* at least one axis required; over optional — defaults to 0 (instant)
                 abs: world coordinate; rel: offset from current position    *)

accel_stmt  = "accel" NEWLINE accel_block ;
accel_block = INDENT { ( axis_stmt | over_stmt ) NEWLINE } DEDENT ;
              (* at least one axis required; over optional — defaults to 0 (instant) *)

axis_stmt   = ( "x" | "y" ) [ VALUE_QUALIFIER | IDENT ] EXPR ;

over_stmt   = "over" EXPR ;
              (* transition duration in seconds; used in chdir/chspd/chrotspd/chpos/accel.
                 optional in all — omitting is equivalent to over 0.
                 when 0: value applied instantly; when > 0: linear tween.   *)
```

---

## Appendix: Example Scripts

### aimed_shots.tama

```
include builtin

main
    act fire_every(0.5, f(0, 200))

fire f(direction, spd_)
    dir aim direction
    spd spd_
```

### builtin.tama (library, no main)

```
act x_way(x_, spin, spd_)
    fire
        dir abs -90 + spin
        spd spd_
    repeat x_-1 i
        fire
            dir seq 360/x_
            spd spd_

act spiral(steps, interval, spd_)
    repeat
        fire
            dir seq 360/steps
            spd spd_
        wait interval

act aimed_shots(amount, spread, spd_)
    repeat amount i
        fire
            dir aim (2*i + 1 - amount) * spread / (2 * max(amount - 1, 1))
            spd spd_

act act_every(seconds, act_)
    repeat
        act act_
        wait seconds

act fire_every(seconds, fire_)
    repeat
        fire fire_
        wait seconds
```

### Exports and variables example

```
export num amount 5
export num spread 90
export num speed_ 200
export str dir_mode aim
export bool enabled true

main
    while enabled
        act shots
        wait 0.5

act shots
    repeat amount i
        fire
            dir dir_mode (2*i + 1 - amount) * spread / (2 * max(amount - 1, 1))
            spd speed_
```
