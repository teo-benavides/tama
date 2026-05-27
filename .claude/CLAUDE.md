this project is a Godot 4.6.2 project where im creating a bullet hell framework called Tama, with a custom DSL called TamaScript inspired by BulletML, an XML-based language for describing bullet patterns in bullet hell games.
you can find the grammar for TamaScript in @tamascript_grammar.bnf , the reference for BulletML at @BulletML_Reference.htm . the "BulletML scripts" folder contains example BulletML scripts, and in the root directory you can find the tamascripts folder, which contains 2 TamaScript files, bowap.tam and enemies.tam, which have BulletML equivalents in BulletML scripts as bowap.xml and enemies.xml respectively.

remember to use the latest Godot 4 features such as typed Arrays and Dictionaries where necessary.

dont bother connecting to the godot LSP to try to fix errors, it doesnt work.