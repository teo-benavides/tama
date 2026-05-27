this project is a Godot 4.6.2 project where im creating a bullet hell framework called Tama, with a custom DSL called TamaScript inspired by BulletML, an XML-based language for describing bullet patterns in bullet hell games.
you can find the grammar for TamaScript in @tamascript_grammar.bnf , the reference for BulletML at @BulletML_Reference.htm . the "BulletML scripts" folder contains example BulletML scripts, and in the root directory you can find the tamascripts folder, which contains 2 TamaScript files, bowap.tam and enemies.tam, which have BulletML equivalents in BulletML scripts as bowap.xml and enemies.xml respectively.
take in mind the example BulletML scripts are written in a modified version of BulletML implemented by myself, so there are differences with the BulletML reference. you can find the differences below
remember to use the latest Godot 4 features such as typed Arrays and Dictionaries where necessary.
TamaScript has various differences from BulletML, which you can inspect by checking out the grammar and comparing it to BulletML.

dont bother connecting to the godot LSP to try to fix errors, it doesnt work.

## Changes from standard BulletML
- No `<bulletml>` node.
- `NUMBER` values can be any valid GDScript expression, meaning you can use built-in functions like `lerp()`, `randf_range()`, etc.
    - Functions defined in a future context class can be used within these expressions.
    - `time()` is one such function.
    - `$rand` and `$rank` cannot be used. `$rand` would be redundant, `$rank` you could implement yourself depending on your game's requirements, for example by using an AutoLoad with a `rank` property or by modifying the context class and adding a function that returns rank.
- Script execution starts at the main block in TamaScript, `<action>` with `label="top"` in my BulletML implementation.
- `<bullet>`s can have a `type` attribute, which indicates the bullet type, a string that is used for fetching the bullet scene from a registry.
- `<bullet>`s can have a `shooter` (or `spawner` in TamaScript) attribute, a string that corresponds to a BulletML script filename (just the filename, not the whole path). Upon being spawned, the bullet will execute the script pointed to by `shooter`. Example: `<bullet type="example" shooter="example.xml">`
- `<wait>` and `<term>` use seconds instead of frames.
- Added `<offset>` node, which can contain `<horizontal>` and `<vertical>` nodes. `<offset>` can go inside `<fire>` and determines a spawn offset for the fired bullet. If no `<horizontal>` nor `<vertical>` is provided, the offset is relative to the bullet's local (accounting for its angle) up vector, and the expression directly inside `<offset>` is taken as the offset value.  
As with `<accel>`, `<horizontal>` and `<vertical>` can take a `type` attribute. If `"absolute"` or `"sequence"`, instead of working as an offset, the value sets the global position on that axis. If `"relative"` or if `type` is omitted, it's an actual offset off of the spawner's position.  
    Examples:
    ```xml
        <fire>
            <direction type="aim">0</direction>
            <speed type="absolute">250</speed>
            <offset>15</offset>
            <bullet type="example"/>
        </fire>

        <fire>
            <direction type="aim">0</direction>
            <speed type="absolute">250</speed>
            <offset>
                <horizontal>10</horizontal>
                <vertical>20</vertical>
            </offset>
            <bullet type="example"/>
        </fire>

        <fire>
            <direction type="aim">0</direction>
            <speed type="absolute">250</speed>
            <offset>
                <!-- Bullet will be spawned at the top left corner of the screen -->
                <horizontal type="absolute">0</horizontal>
                <vertical type="absolute">0</vertical>
            </offset>
            <bullet type="example"/>
        </fire>
    ```
- If a `<repeat>`'s `<times>` is set to `-1` or omitted, the `<action>` will repeat forever.
