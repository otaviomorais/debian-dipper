#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <sys/ioctl.h>
static int haptic_ioctl(int fd,unsigned long request,...);
#define ioctl haptic_ioctl
#include "haptics.c"
#undef ioctl
#include <assert.h>
static int supported=1,removed;
static int haptic_ioctl(int fd,unsigned long request,...)
{
    (void)fd;
    va_list args; va_start(args,request);
    if(request==EVIOCRMFF) { assert(va_arg(args,int)==2); removed++; va_end(args); return 0; }
    void *data=va_arg(args,void *); va_end(args);
    if(request==EVIOCGNAME(127)) { strcpy(data,"spmi_haptics"); return 13; }
    if(request==EVIOCGBIT(EV_FF,((FF_CNT+8*sizeof(unsigned long)-1)/(8*sizeof(unsigned long)))*sizeof(unsigned long))) {
        if(supported) ((unsigned long *)data)[FF_RUMBLE/(8*sizeof(unsigned long))]|=1UL<<(FF_RUMBLE%(8*sizeof(unsigned long)));
        return 0;
    }
    if(request==EVIOCSFF) {
        struct ff_effect *effect=data;
        assert(effect->type==FF_RUMBLE && effect->id==-1 && effect->replay.length==20 && !effect->replay.delay);
        assert(effect->u.rumble.strong_magnitude==0x3000 && !effect->u.rumble.weak_magnitude);
        effect->id=2; return 0;
    }
    assert(!"Unexpected haptic ioctl"); return -1;
}
int main(void)
{
    struct haptics h=HAPTICS_INITIALIZER;
    assert(!haptics_open(&h,NULL,0) && !haptics_pulse(&h,1000));
    assert(haptics_open(&h,NULL,51)<0 && errno==EINVAL);
    char path[]="/tmp/dipper-haptics-XXXXXX";
    int fd=mkstemp(path); assert(fd>=0); close(fd);
    assert(!haptics_open(&h,path,20));
    assert(haptics_pulse(&h,1000)==1);
    assert(haptics_pulse(&h,1010)==0);
    assert(haptics_pulse(&h,1020)==1);
    haptics_close(&h); assert(h.fd==-1 && removed==1);
    fd=open(path,O_RDONLY); assert(fd>=0);
    for(int i=0;i<3;i++) {
        struct input_event event;
        assert(read(fd,&event,sizeof(event))==(ssize_t)sizeof(event));
        assert(event.type==EV_FF && event.code==2 && event.value==(i==2?0:1));
    }
    close(fd);
    supported=0; assert(haptics_open(&h,path,20)<0 && h.fd<0);
    supported=1; assert(!haptics_open(&h,path,20));
    close(h.fd); assert(haptics_pulse(&h,2000)<0 && h.fd==-1);
    unlink(path);
    puts("haptics: FF capabilities, bounded 20 ms pulse, magnitude, rate limit, stop, off and failed device: OK");
    return 0;
}
