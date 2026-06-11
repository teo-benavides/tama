#pragma once
#include "tama_animated_texture.h"
#include "tama_server_object_config.h"

class TamaServerCurvedLaserConfig : public TamaServerObjectConfig {
    GDCLASS(TamaServerCurvedLaserConfig, TamaServerObjectConfig)
protected:
    static void _bind_methods();
public:
    float width  = 20.0f;
    int   length = 30;
    godot::Ref<TamaAnimatedTexture> texture;

    float get_width()  const { return width; }
    void  set_width(float v) { width = v; }
    int   get_length() const { return length; }
    void  set_length(int v)  { length = (v < 2 ? 2 : v); }
    godot::Ref<TamaAnimatedTexture> get_texture()           const { return texture; }
    void  set_texture(godot::Ref<TamaAnimatedTexture> v)          { texture = v; }
};
