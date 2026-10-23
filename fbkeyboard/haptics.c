/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "haptics.h"
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int attach(struct haptics *h,const char *path,unsigned duration,int explicit)
{
    int fd=open(path,O_RDWR|O_NONBLOCK|O_CLOEXEC);
    if(fd<0) return -1;
    char name[128]={0};
    unsigned long bits[(FF_CNT+8*sizeof(unsigned long)-1)/(8*sizeof(unsigned long))]={0};
    if(ioctl(fd,EVIOCGNAME(sizeof(name)-1),name)<0 ||
       (!explicit && !strcasestr(name,"haptic") && !strcasestr(name,"vibra")) ||
       ioctl(fd,EVIOCGBIT(EV_FF,sizeof(bits)),bits)<0 ||
       !(bits[FF_RUMBLE/(8*sizeof(unsigned long))]&(1UL<<(FF_RUMBLE%(8*sizeof(unsigned long)))))) {
        close(fd); return -1;
    }
    struct ff_effect effect={.type=FF_RUMBLE,.id=-1,.replay={.length=(unsigned short)duration}};
    /* PMI8998 driver scales the high byte as a percentage. 0x30 is a gentle
       pulse, below the motor's vendor vmax (3100 mV), with kernel-timed stop. */
    effect.u.rumble.strong_magnitude=0x3000;
    if(ioctl(fd,EVIOCSFF,&effect)<0) { close(fd); return -1; }
    *h=(struct haptics){.fd=fd,.effect=effect.id,.duration=duration};
    fprintf(stderr,"Haptic feedback: %s, %u ms\n",name,duration);
    return 0;
}
int haptics_open(struct haptics *h,const char *device,unsigned duration)
{
    *h=(struct haptics)HAPTICS_INITIALIZER;
    if(!duration) return 0;
    if(duration>50) { errno=EINVAL; return -1; }
    if(device) return attach(h,device,duration,1);
    glob_t paths;
    if(glob("/dev/input/event*",0,NULL,&paths)!=0) return -1;
    int found=-1;
    for(size_t i=0;i<paths.gl_pathc;i++)
        if(attach(h,paths.gl_pathv[i],duration,0)==0) { found=0; break; }
    globfree(&paths);
    return found;
}
int haptics_pulse(struct haptics *h,uint64_t now)
{
    if(h->fd<0 || !h->duration || (h->last && now>=h->last && now-h->last<15)) return 0;
    struct input_event event={.type=EV_FF,.code=(unsigned short)h->effect,.value=1};
    ssize_t result;
    do result=write(h->fd,&event,sizeof(event)); while(result<0 && errno==EINTR);
    if(result==(ssize_t)sizeof(event)) { h->last=now; return 1; }
    if(result<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return 0;
    /* A feedback failure must never block keyboard input. Closing releases
       any uploaded effect; the kernel also bounds each pulse by its length. */
    close(h->fd); h->fd=-1; h->effect=-1;
    return -1;
}
void haptics_close(struct haptics *h)
{
    if(h->fd>=0) {
        struct input_event stop={.type=EV_FF,.code=(unsigned short)h->effect};
        (void)write(h->fd,&stop,sizeof(stop));
        (void)ioctl(h->fd,EVIOCRMFF,h->effect);
        close(h->fd);
    }
    *h=(struct haptics)HAPTICS_INITIALIZER;
}
