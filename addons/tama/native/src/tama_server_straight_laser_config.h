#pragma once
#include "tama_animated_texture.h"
#include "tama_server_object_config.h"

class TamaServerStraightLaserConfig : public TamaServerObjectConfig {
    GDCLASS(TamaServerStraightLaserConfig, TamaServerObjectConfig)
protected:
    static void _bind_methods();
public:
    float width           = 20.0f;
    float length          = 1000.0f;
    godot::Ref<TamaAnimatedTexture> texture;
    bool  tile_x          = false;
    bool  tile_y          = false;
    godot::Ref<TamaAnimatedTexture> base_texture;
    int   delay_frames    = 120;
    int   expand_frames   = 10;
    int   duration_frames = 120;
    int   fade_frames     = 30;

    float get_width()                   const { return width; }
    void  set_width(float v)                  { width = v; }
    float get_length()                  const { return length; }
    void  set_length(float v)                 { length = v; }
    godot::Ref<TamaAnimatedTexture> get_texture()       const { return texture; }
    void  set_texture(godot::Ref<TamaAnimatedTexture> v)      { texture = v; }
    bool  get_tile_x()                  const { return tile_x; }
    void  set_tile_x(bool v)                  { tile_x = v; }
    bool  get_tile_y()                  const { return tile_y; }
    void  set_tile_y(bool v)                  { tile_y = v; }
    godot::Ref<TamaAnimatedTexture> get_base_texture()  const { return base_texture; }
    void  set_base_texture(godot::Ref<TamaAnimatedTexture> v) { base_texture = v; }
    int   get_delay_frames()            const { return delay_frames; }
    void  set_delay_frames(int v)             { delay_frames = v; }
    int   get_expand_frames()           const { return expand_frames; }
    void  set_expand_frames(int v)            { expand_frames = v; }
    int   get_duration_frames()         const { return duration_frames; }
    void  set_duration_frames(int v)          { duration_frames = v; }
    int   get_fade_frames()             const { return fade_frames; }
    void  set_fade_frames(int v)              { fade_frames = v; }
};
