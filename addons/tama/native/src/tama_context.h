#pragma once
#include <godot_cpp/classes/ref_counted.hpp>

class TamaContext : public godot::RefCounted {
    GDCLASS(TamaContext, godot::RefCounted)
protected:
    static void _bind_methods();
public:
    virtual float time()          const;
    virtual int   get_1_in(int n) const;
};
