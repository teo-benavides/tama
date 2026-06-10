#pragma once
#include "tama_server_object_config.h"
#include <godot_cpp/classes/texture2d.hpp>

class TamaServerStraightLaserConfig : public TamaServerObjectConfig {
    GDCLASS(TamaServerStraightLaserConfig, TamaServerObjectConfig)
protected:
    static void _bind_methods();
public:
    float width           = 20.0f;
    float length          = 1000.0f;
    godot::Ref<godot::Texture2D> texture;
    bool  tile            = true;
    godot::Ref<godot::Texture2D> base_texture;
    int   delay_frames    = 120;
    int   expand_frames   = 10;
    int   duration_frames = 120;
    int   fade_frames     = 30;

    float get_width()                   const { return width; }
    void  set_width(float v)                  { width = v; }
    float get_length()                  const { return length; }
    void  set_length(float v)                 { length = v; }
    godot::Ref<godot::Texture2D> get_texture()        const { return texture; }
    void  set_texture(godot::Ref<godot::Texture2D> v)       { texture = v; }
    bool  get_tile()                    const { return tile; }
    void  set_tile(bool v)                    { tile = v; }
    godot::Ref<godot::Texture2D> get_base_texture()   const { return base_texture; }
    void  set_base_texture(godot::Ref<godot::Texture2D> v)  { base_texture = v; }
    int   get_delay_frames()            const { return delay_frames; }
    void  set_delay_frames(int v)             { delay_frames = v; }
    int   get_expand_frames()           const { return expand_frames; }
    void  set_expand_frames(int v)            { expand_frames = v; }
    int   get_duration_frames()         const { return duration_frames; }
    void  set_duration_frames(int v)          { duration_frames = v; }
    int   get_fade_frames()             const { return fade_frames; }
    void  set_fade_frames(int v)              { fade_frames = v; }
};
