#include <assert.h>
#include <stdio.h>
#include "touch.h"

int main(void)
{
    struct touch_state s;
    touch_reset(&s);
    assert(touch_frame(&s,0,-1,-1,0,0)==TOUCH_NONE);
    assert(touch_frame(&s,1,10,12,0,10)==TOUCH_NONE);
    assert(touch_frame(&s,0,-1,-1,0,20)==TOUCH_TAP);
    assert(s.key==12);
    assert(touch_frame(&s,0,-1,-1,0,21)==TOUCH_NONE);
    /* Two quick taps at identical coordinates must both survive. */
    for(int i=0;i<2;i++) {
        assert(touch_frame(&s,1,11+i,12,0,30+i*10)==TOUCH_NONE);
        assert(touch_frame(&s,0,-1,-1,0,35+i*10)==TOUCH_TAP);
    }
    touch_reset(&s);
    touch_frame(&s,1,15,50,1,0);
    assert(touch_frame(&s,1,15,50,1,449)==TOUCH_NONE);
    assert(touch_frame(&s,1,15,50,1,450)==TOUCH_REPEAT);
    assert(touch_frame(&s,1,15,50,1,451)==TOUCH_NONE);
    assert(touch_frame(&s,1,15,50,1,540)==TOUCH_REPEAT);
    assert(touch_frame(&s,0,-1,-1,0,550)==TOUCH_NONE);
    touch_reset(&s);
    touch_frame(&s,1,20,10,0,0);
    touch_frame(&s,2,21,10,0,10);
    assert(s.blocked);
    assert(touch_frame(&s,1,21,10,0,20)==TOUCH_NONE);
    assert(touch_frame(&s,0,-1,-1,0,30)==TOUCH_NONE);
    assert(!s.blocked);
    touch_frame(&s,1,25,10,0,40);
    touch_frame(&s,1,26,10,0,50); /* slot reused: release was lost */
    assert(s.blocked);
    assert(touch_frame(&s,0,-1,-1,0,60)==TOUCH_NONE);
    touch_frame(&s,1,30,10,0,70);
    touch_frame(&s,1,30,-1,0,80); /* slid outside original button */
    assert(touch_frame(&s,0,-1,-1,0,90)==TOUCH_NONE);
    touch_frame(&s,1,40,10,0,100);
    touch_cancel(&s); /* SYN_DROPPED / VT change */
    assert(touch_frame(&s,1,40,10,0,110)==TOUCH_NONE);
    assert(touch_frame(&s,0,-1,-1,0,120)==TOUCH_NONE);
    touch_frame(&s,1,50,50,1,200);
    assert(touch_frame(&s,1,50,50,1,3200)==TOUCH_NONE);
    assert(s.blocked);
    assert(touch_frame(&s,0,-1,-1,0,3210)==TOUCH_NONE);
    puts("touch: tap, fast taps, repeats, multi-touch, tracking reuse, slide, sync loss, timeout: OK");
    return 0;
}
