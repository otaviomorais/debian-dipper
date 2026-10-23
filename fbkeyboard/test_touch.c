#include <assert.h>
#include <stdio.h>
#include "touch.h"

int main(void)
{
    struct touch_state s;
    touch_reset(&s);
    assert(touch_frame(&s,0,-1,-1,0)==TOUCH_NONE);
    assert(touch_frame(&s,1,10,12,10)==TOUCH_TAP);
    assert(s.key==12);
    assert(touch_frame(&s,0,-1,-1,20)==TOUCH_NONE);
    assert(touch_frame(&s,0,-1,-1,21)==TOUCH_NONE);
    /* Consecutive taps at the same coordinates, with or without an up frame. */
    for(int i=0;i<20;i++)
        assert(touch_frame(&s,1,11+i,12,30+i*10)==TOUCH_TAP);
    for(int i=0;i<20;i++) {
        assert(touch_frame(&s,1,100+i,12,300+i*10)==TOUCH_TAP);
        assert(touch_frame(&s,0,-1,-1,305+i*10)==TOUCH_NONE);
    }
    /* A motionless or noisy held contact cannot produce timer repeats. */
    touch_reset(&s);
    assert(touch_frame(&s,1,150,50,1000)==TOUCH_TAP);
    for(unsigned now=1001;now<31000;now+=30)
        assert(touch_frame(&s,1,150,50,now)==TOUCH_NONE);
    assert(touch_frame(&s,1,151,50,31001)==TOUCH_TAP);
    /* Sliding must not type a neighbouring key or latch future contacts. */
    assert(touch_frame(&s,1,151,51,31002)==TOUCH_NONE);
    assert(touch_frame(&s,1,151,50,31003)==TOUCH_NONE);
    assert(touch_frame(&s,1,152,51,31004)==TOUCH_TAP);
    touch_frame(&s,2,153,10,31005);
    assert(s.blocked);
    assert(touch_frame(&s,1,153,10,31006)==TOUCH_NONE);
    assert(touch_frame(&s,0,-1,-1,31007)==TOUCH_NONE);
    assert(touch_frame(&s,1,154,10,31008)==TOUCH_TAP);
    touch_cancel(&s); /* SYN_DROPPED / VT change: never invent a press. */
    assert(touch_frame(&s,1,154,10,31009)==TOUCH_NONE);
    assert(touch_frame(&s,0,-1,-1,31010)==TOUCH_NONE);
    assert(touch_frame(&s,1,155,10,31011)==TOUCH_TAP);
    touch_reset(&s);
    assert(touch_frame(&s,1,160,-1,32000)==TOUCH_NONE);
    assert(touch_frame(&s,1,160,12,32020)==TOUCH_TAP);
    assert(touch_frame(&s,1,160,13,32030)==TOUCH_NONE);
    touch_reset(&s);
    assert(touch_frame(&s,1,170,-1,33000)==TOUCH_NONE);
    assert(touch_frame(&s,1,170,12,33300)==TOUCH_NONE);
    assert(touch_frame(&s,1,171,12,33310)==TOUCH_TAP);
    puts("touch: press once, 40 rapid taps, missing up, no timer repeats, slide recovery, multi-touch and sync cancellation: OK");
    return 0;
}
