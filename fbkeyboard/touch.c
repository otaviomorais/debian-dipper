/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "touch.h"

void touch_reset(struct touch_state *s)
{
    *s = (struct touch_state){.id = -1, .key = -1};
}
void touch_cancel(struct touch_state *s)
{
    int id = s->id;
    touch_reset(s);
    s->id = id;
    s->blocked = 1; /* ignore this contact, but accept the next tracking ID */
}
int touch_frame(struct touch_state *s, int count, int id, int key, uint64_t now)
{
    if (!count) {
        touch_reset(s);
        return TOUCH_NONE;
    }
    if (count != 1 || id < 0) {
        s->id = id;
        touch_cancel(s);
        s->started = now;
        return TOUCH_NONE;
    }
    if (s->id != id) {
        /* A new tracking ID is a new contact even if the previous up frame was
           missing. Type one complete down/up chord now, never on a timer. */
        touch_reset(s);
        s->id = id;
        s->started = now;
    }
    if (s->blocked || s->emitted)
        return TOUCH_NONE;
    if (now >= s->started && now - s->started > 200) {
        touch_cancel(s);
        return TOUCH_NONE;
    }
    /* A contact that starts between keys can acquire its first valid key.
       Once emitted, moving/holding can never type a second key. */
    if (key < 0)
        return TOUCH_NONE;
    s->key = key;
    s->emitted = 1;
    return TOUCH_TAP;
}
