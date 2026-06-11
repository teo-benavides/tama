#pragma once
#include <vector>

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/list.hpp>

// Runtime animation frame — built once at registration time from TamaAnimatedTexture.
struct TamaAnimFrame {
    godot::Ref<godot::Texture2D> texture;
    float duration_sec = 0.016667f; // 1 frame at 60 fps
};

class TamaAnimatedTexture : public godot::Resource {
    GDCLASS(TamaAnimatedTexture, godot::Resource)

public:
    struct Frame {
        godot::Ref<godot::Texture2D> texture;
        int duration = 1; // in animation frames at the given fps
    };

private:
    float fps         = 60.0f;
    int   frame_count = 0;
    int   blend_mode  = 0;
    std::vector<Frame> _frames;

protected:
    static void _bind_methods();

public:
    // Mirrors CanvasItemMaterial::BlendMode
    enum BlendMode {
        BLEND_MODE_MIX           = 0,
        BLEND_MODE_ADD           = 1,
        BLEND_MODE_SUB           = 2,
        BLEND_MODE_MUL           = 3,
        BLEND_MODE_PREMULT_ALPHA = 4,
    };

    float get_fps()  const { return fps; }
    void  set_fps(float v) { fps = v; }

    int  get_blend_mode()  const { return blend_mode; }
    void set_blend_mode(int v)   { blend_mode = v; }

    int  get_frame_count() const { return frame_count; }
    void set_frame_count(int v);

    godot::Ref<godot::Texture2D> get_frame_texture(int i) const;
    void set_frame_texture(int i, godot::Ref<godot::Texture2D> v);
    int  get_frame_duration(int i) const;
    void set_frame_duration(int i, int v);

    // Returns the first frame's texture (used for auto_rect and static draw).
    godot::Ref<godot::Texture2D> get_first_texture() const;

    // Build runtime frames. Returns empty if no frames.
    // Returns a single-entry vector when fps <= 0 or only 1 frame (static).
    // Returns all frames with per-frame duration_sec when animated.
    std::vector<TamaAnimFrame> build_anim_frames() const;

    bool _get(const godot::StringName &p_name, godot::Variant &r_ret) const;
    bool _set(const godot::StringName &p_name, const godot::Variant &p_value);
    void _get_property_list(godot::List<godot::PropertyInfo> *p_list) const;
};
