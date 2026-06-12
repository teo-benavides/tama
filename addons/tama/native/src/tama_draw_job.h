#pragma once
#include <cstdint>

// Kind tag for DrawJob — determines how the coordinator dispatches each draw command.
enum DrawJobKind {
    DRAW_BULLET_BATCH,
    DRAW_BULLET_BATCH_SPAWN,
    DRAW_STRAIGHT_LASER,
    DRAW_CURVED_LASER
};

// One entry per batch (bullets) or per active laser (lasers/curved lasers).
// Collected each physics frame by the coordinator from all three pools,
// sorted by spawn_frame, then emitted into per-blend-mode canvas items in _draw().
struct DrawJob {
    int64_t     spawn_frame = 0;
    int         blend_mode  = 0;
    DrawJobKind kind        = DRAW_BULLET_BATCH;
    void       *data        = nullptr;  // BatchData*, LaserState*, or CurvedLaserState*
    void       *extra       = nullptr;  // TypeData* (bullet/laser/curved) for draw context
};
