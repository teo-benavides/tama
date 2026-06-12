this project is a Godot 4.6.2 project where im creating a bullet hell framework called Tama, with a custom DSL called TamaScript inspired by BulletML, an XML-based language for describing bullet patterns in bullet hell games.
you can find the grammar for TamaScript in tamascript_grammar.bnf. you can see example scripts in the tamascripts folder in the root directory

READ ARCHITECTURE.md FIRST — it's an orientation guide to the codebase (C++ GDExtension layout, the source map, how a frame flows, build instructions, and current in-progress work) written so you don't have to analyze the whole tree before starting.

dont bother connecting to the godot LSP to try to fix errors, it doesnt work.
