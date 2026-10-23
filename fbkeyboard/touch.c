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
int touch_frame(struct touch_state *s, int count, int id, int key,
                int repeatable, uint64_t now)
{
    if (!count) {
        int tap = !s->blocked && s->id >= 0 && s->key >= 0 && !s->repeated;
        s->id = -1; /* keep key for the caller to dispatch */
        s->blocked = 0;
        s->repeated = 0;
        return tap ? TOUCH_TAP : TOUCH_NONE;
    }
    if (s->blocked)
        return TOUCH_NONE;
    if (count != 1 || (s->id >= 0 && s->id != id)) {
        touch_cancel(s);
        return TOUCH_NONE;
    }
    if (s->id < 0) {
        s->id = id;
        s->key = key;
        s->started = s->last = now;
        s->repeated = 0;
    } else if (key != s->key) {
        /* Sliding outside the original key cancels; don't type the neighbour. */
        touch_cancel(s);
        return TOUCH_NONE;
    }
    /* Bound damage even if a broken driver never reports a release. */
    if (now - s->started >= 3000) {
        touch_cancel(s);
        return TOUCH_NONE;
    }
    if (key >= 0 && repeatable && now - s->started >= 450 &&
        now - s->last >= 90) {
        s->last = now;
        s->repeated = 1;
        return TOUCH_REPEAT;
    }
    return TOUCH_NONE;
}
