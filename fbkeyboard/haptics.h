/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FBKEYBOARD_HAPTICS_H
#define FBKEYBOARD_HAPTICS_H
#include <stdint.h>
struct haptics { int fd, effect; unsigned duration; uint64_t last; };
#define HAPTICS_INITIALIZER { .fd=-1, .effect=-1 }
int haptics_open(struct haptics *h, const char *device, unsigned duration);
int haptics_pulse(struct haptics *h, uint64_t now);
void haptics_close(struct haptics *h);
#endif
