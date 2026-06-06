#pragma once
#include "tama_interpreter.h"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

class TamaEmitter : public godot::Node2D {
    GDCLASS(TamaEmitter, godot::Node2D)
protected:
    static void _bind_methods();

public:
    // -----------------------------------------------------------------------
    // @export properties
    // -----------------------------------------------------------------------
    godot::String get_script_filename() const { return _script_filename; }
    void          set_script_filename(const godot::String &v);
    bool get_excluded_from_group() const { return _excluded_from_group; }
    void set_excluded_from_group(bool v) { _excluded_from_group = v; }

    // -----------------------------------------------------------------------
    // Godot virtuals
    // -----------------------------------------------------------------------
    void _ready()   override;
    void _process(double delta) override;

    // Dynamic property overrides for tama_export_* inspector variables
    bool _get(const godot::StringName &p_name, godot::Variant &r_ret) const;
    bool _set(const godot::StringName &p_name, const godot::Variant &p_value);
    bool _property_can_revert(const godot::StringName &p_name) const;
    bool _property_get_revert(const godot::StringName &p_name, godot::Variant &r_ret) const;
    void _get_property_list(godot::List<godot::PropertyInfo> *p_list) const;

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------
    void start();
    void stop();
    void set_export(const godot::String &name, const godot::Variant &value) { _export_values[name] = value; }
    void set_export_num(const godot::String &name, float value)  { _export_values[name] = godot::Variant(value); }
    void set_export_str(const godot::String &name, const godot::String &value) { _export_values[name] = godot::Variant(value); }
    void set_export_bool(const godot::String &name, bool value)  { _export_values[name] = godot::Variant(value); }
    godot::Variant get_export(const godot::String &name) const { return _export_values.get(name, godot::Variant()); }

    // For spawner compatibility
    float _last_angle = 0.0f;
    float _last_speed = 0.0f;

private:
    struct ExportDef { std::string name, type; godot::Variant default_value; };

    godot::String _script_filename;
    bool          _excluded_from_group = false;
    _TamaInterpreter *_interpreter = nullptr;
    bool          _running        = false;
    godot::Dictionary _export_values;
    std::vector<ExportDef> _cached_export_defs;
    uint64_t _last_modified_time = 0;
    float    _poll_timer         = 0.0f;
    static constexpr float POLL_INTERVAL = 1.0f;

    void  _refresh_exports();
    void  _read_exports_from(_TamaASTNode *program);
    godot::String _exports_signature() const;
    godot::String _find_script_path() const;
    uint64_t      _get_script_mtime() const;
    void  _check_file_changed();
};
