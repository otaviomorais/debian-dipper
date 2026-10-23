/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "touch.h"

void touch_reset(struct touch_state *s)
{
    *s = (struct touch_state){.id = -1, .key = -1};
}
void touch_cancel(struct touch_state *s)
{
    touch_reset(s);
    s->blocked = 1; /* require all fingers up before accepting another gesture */
}
int touch_frame(struct touch_state *s, int count, int id, int key, uint64_t now)
{
    if (!count) {
        touch_reset(s);
        return TOUCH_NONE;
    }
    if (s->blocked)
        return TOUCH_NONE;
    if (count != 1 || id < 0) {
        touch_cancel(s);
        s->started = now;
        return TOUCH_NONE;
    }
    if (s->id != id) {
        /* A new tracking ID is a new contact even if the previous up frame was
           missing. Type one complete down/up chord now, never on a timer. */
        s->id = id;
        s->key = key;
        s->started = now;
        return key >= 0 ? TOUCH_TAP : TOUCH_NONE;
    } else if (key != s->key) {
        /* The original key was already emitted. Moving cannot type another. */
        s->key = -1;
    }
    return TOUCH_NONE;
}
