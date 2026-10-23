/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FBKEYBOARD_TOUCH_H
#define FBKEYBOARD_TOUCH_H
#include <stdint.h>
struct touch_state {
    int id, key, blocked, repeated;
    uint64_t started, last;
};
enum { TOUCH_NONE = 0, TOUCH_TAP = 1, TOUCH_REPEAT = 2 };
void touch_reset(struct touch_state *s);
int touch_frame(struct touch_state *s, int count, int id, int key,
                int repeatable, uint64_t now);
void touch_cancel(struct touch_state *s);
#endif
