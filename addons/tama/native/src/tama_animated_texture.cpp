#include "tama_animated_texture.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

void TamaAnimatedTexture::set_frame_count(int v) {
    if (v < 0) v = 0;
    frame_count = v;
    _frames.resize((size_t)v); // default-initializes new frames (duration=1, texture=null)
    notify_property_list_changed();
}

Ref<Texture2D> TamaAnimatedTexture::get_frame_texture(int i) const {
    if (i < 0 || i >= (int)_frames.size()) return {};
    return _frames[i].texture;
}
void TamaAnimatedTexture::set_frame_texture(int i, Ref<Texture2D> v) {
    if (i < 0 || i >= (int)_frames.size()) return;
    _frames[i].texture = v;
}
int TamaAnimatedTexture::get_frame_duration(int i) const {
    if (i < 0 || i >= (int)_frames.size()) return 1;
    return _frames[i].duration;
}
void TamaAnimatedTexture::set_frame_duration(int i, int v) {
    if (i < 0 || i >= (int)_frames.size()) return;
    _frames[i].duration = v < 1 ? 1 : v;
}

Ref<Texture2D> TamaAnimatedTexture::get_first_texture() const {
    if (_frames.empty()) return {};
    return _frames[0].texture;
}

std::vector<TamaAnimFrame> TamaAnimatedTexture::build_anim_frames() const {
    if (_frames.empty()) return {};
    std::vector<TamaAnimFrame> result;
    // Single-frame or fps<=0: treat as static — return only the first frame.
    if (fps <= 0.0f || (int)_frames.size() == 1) {
        result.push_back({ _frames[0].texture, 0.0f });
        return result;
    }
    result.reserve(_frames.size());
    for (const Frame &f : _frames)
        result.push_back({ f.texture, (float)f.duration / fps });
    return result;
}

bool TamaAnimatedTexture::_get(const StringName &p_name, Variant &r_ret) const {
    String name = p_name;
    if (!name.begins_with("frame_")) return false;
    int slash = name.find("/");
    if (slash < 0) return false;
    int idx = name.substr(6, slash - 6).to_int();
    if (idx < 0 || idx >= (int)_frames.size()) return false;
    String prop = name.substr(slash + 1);
    if (prop == "texture")  { r_ret = _frames[idx].texture;  return true; }
    if (prop == "duration") { r_ret = _frames[idx].duration; return true; }
    return false;
}

bool TamaAnimatedTexture::_set(const StringName &p_name, const Variant &p_value) {
    String name = p_name;
    if (!name.begins_with("frame_")) return false;
    int slash = name.find("/");
    if (slash < 0) return false;
    int idx = name.substr(6, slash - 6).to_int();
    if (idx < 0 || idx >= (int)_frames.size()) return false;
    String prop = name.substr(slash + 1);
    if (prop == "texture")  { _frames[idx].texture  = p_value;        return true; }
    if (prop == "duration") { _frames[idx].duration = (int)p_value;   return true; }
    return false;
}

void TamaAnimatedTexture::_get_property_list(List<PropertyInfo> *p_list) const {
    for (int i = 0; i < (int)_frames.size(); ++i) {
        String prefix = "frame_" + String::num_int64(i) + "/";
        p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "texture",
            PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", PROPERTY_USAGE_DEFAULT));
        p_list->push_back(PropertyInfo(Variant::INT, prefix + "duration",
            PROPERTY_HINT_RANGE, "1,100,1,or_greater", PROPERTY_USAGE_DEFAULT));
    }
}

void TamaAnimatedTexture::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_fps"),           &TamaAnimatedTexture::get_fps);
    ClassDB::bind_method(D_METHOD("set_fps", "v"),      &TamaAnimatedTexture::set_fps);
    ClassDB::bind_method(D_METHOD("get_frame_count"),   &TamaAnimatedTexture::get_frame_count);
    ClassDB::bind_method(D_METHOD("set_frame_count", "v"), &TamaAnimatedTexture::set_frame_count);
    ClassDB::bind_method(D_METHOD("get_frame_texture", "i"), &TamaAnimatedTexture::get_frame_texture);
    ClassDB::bind_method(D_METHOD("set_frame_texture", "i", "v"), &TamaAnimatedTexture::set_frame_texture);
    ClassDB::bind_method(D_METHOD("get_frame_duration", "i"), &TamaAnimatedTexture::get_frame_duration);
    ClassDB::bind_method(D_METHOD("set_frame_duration", "i", "v"), &TamaAnimatedTexture::set_frame_duration);
    ClassDB::bind_method(D_METHOD("get_first_texture"), &TamaAnimatedTexture::get_first_texture);

    ClassDB::bind_method(D_METHOD("get_blend_mode"),      &TamaAnimatedTexture::get_blend_mode);
    ClassDB::bind_method(D_METHOD("set_blend_mode", "v"), &TamaAnimatedTexture::set_blend_mode);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_mode", PROPERTY_HINT_ENUM,
                              "Mix,Add,Subtract,Multiply,Premultiplied Alpha"),
                 "set_blend_mode", "get_blend_mode");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fps", PROPERTY_HINT_RANGE, "0,240,1,or_greater"),
                 "set_fps", "get_fps");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "frame_count", PROPERTY_HINT_RANGE, "0,64,1,or_greater"),
                 "set_frame_count", "get_frame_count");
    // Per-frame properties (frame_N/texture, frame_N/duration) are dynamic via _get_property_list.
}
