/* SPDX-License-Identifier: GPL-3.0-or-later
 * Framebuffer keyboard for the Linux VT, based on fbkeyboard by
 * Julian Winkler (2017) and Ferenc Bakonyi (2020).
 * Touch recovery uses libevdev; no physical key remains held between taps.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/uinput.h>
#include <linux/vt.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <libevdev/libevdev.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "touch.h"

#define ROWS 7
#define COLS 10
#define MOD_SHIFT 1
#define MOD_CTRL 2
#define MOD_ALT 4
#define ACT_SHIFT (KEY_MAX + 1)
#define ACT_CTRL (KEY_MAX + 2)
#define ACT_ALT (KEY_MAX + 3)
#define ACT_NUM (KEY_MAX + 4)
#define ACT_FN (KEY_MAX + 5)
#define ACT_RESET (KEY_MAX + 6)
struct key { const char *label; unsigned short code; unsigned char shift; };
#define K(label, code) {label, code, 0}
#define S(label, code) {label, code, 1}
static const struct key common[ROWS][COLS] = {
    {K("Esc",KEY_ESC), K("Tab",KEY_TAB), K("Home",KEY_HOME), K("End",KEY_END), K("PgUp",KEY_PAGEUP), K("PgDn",KEY_PAGEDOWN), K("Del",KEY_DELETE), K("Ins",KEY_INSERT), K("Fn",ACT_FN), K("Reset",ACT_RESET)},
    {K("q",KEY_Q),K("w",KEY_W),K("e",KEY_E),K("r",KEY_R),K("t",KEY_T),K("y",KEY_Y),K("u",KEY_U),K("i",KEY_I),K("o",KEY_O),K("p",KEY_P)},
    {K("a",KEY_A),K("s",KEY_S),K("d",KEY_D),K("f",KEY_F),K("g",KEY_G),K("h",KEY_H),K("j",KEY_J),K("k",KEY_K),K("l",KEY_L),K("/",KEY_SLASH)},
    {K("Shift",ACT_SHIFT),K("z",KEY_Z),K("x",KEY_X),K("c",KEY_C),K("v",KEY_V),K("b",KEY_B),K("n",KEY_N),K("m",KEY_M),K(".",KEY_DOT),K("Bksp",KEY_BACKSPACE)},
    {K("Ctrl",ACT_CTRL),K("Alt",ACT_ALT),K("123",ACT_NUM),K("Space",KEY_SPACE),K("Space",KEY_SPACE),K("-",KEY_MINUS),K("=",KEY_EQUAL),K("\\",KEY_BACKSLASH),K("'",KEY_APOSTROPHE),K("Enter",KEY_ENTER)},
    {K("Left",KEY_LEFT),K("Down",KEY_DOWN),K("Up",KEY_UP),K("Right",KEY_RIGHT),K("Menu",KEY_F1),K("New",KEY_F2),K("Prev",KEY_F3),K("Next",KEY_F4),K("Split",KEY_F5),K("Zoom",KEY_F6)},
    {K("Hist",KEY_F8),K("Select",KEY_F10),K("Copy",KEY_F11),K("Paste",KEY_F12),K("Cancel",KEY_ESC),K("Pane",KEY_F7),K("Cmd",KEY_F9),K("`",KEY_GRAVE),S("~",KEY_GRAVE),K("Reset",ACT_RESET)}
};
static const struct key numbers[3][COLS] = {
    {K("1",KEY_1),K("2",KEY_2),K("3",KEY_3),K("4",KEY_4),K("5",KEY_5),K("6",KEY_6),K("7",KEY_7),K("8",KEY_8),K("9",KEY_9),K("0",KEY_0)},
    {S("!",KEY_1),S("@",KEY_2),S("#",KEY_3),S("$",KEY_4),S("%",KEY_5),S("^",KEY_6),S("&",KEY_7),S("*",KEY_8),S("(",KEY_9),S(")",KEY_0)},
    {K("Shift",ACT_SHIFT),K("[",KEY_LEFTBRACE),K("]",KEY_RIGHTBRACE),S("{",KEY_LEFTBRACE),S("}",KEY_RIGHTBRACE),K(";",KEY_SEMICOLON),S(":",KEY_SEMICOLON),K(",",KEY_COMMA),S("?",KEY_SLASH),K("Bksp",KEY_BACKSPACE)}
};
static const struct key functions[3][COLS] = {
    {K("F1",KEY_F1),K("F2",KEY_F2),K("F3",KEY_F3),K("F4",KEY_F4),K("F5",KEY_F5),K("F6",KEY_F6),K("F7",KEY_F7),K("F8",KEY_F8),K("F9",KEY_F9),K("F10",KEY_F10)},
    {K("F11",KEY_F11),K("F12",KEY_F12),K("`",KEY_GRAVE),S("~",KEY_GRAVE),S("_",KEY_MINUS),S("+",KEY_EQUAL),S("|",KEY_BACKSLASH),S("\"",KEY_APOSTROPHE),S("<",KEY_COMMA),S(">",KEY_DOT)},
    {K("Shift",ACT_SHIFT),K("z",KEY_Z),K("x",KEY_X),K("c",KEY_C),K("v",KEY_V),K("b",KEY_B),K("n",KEY_N),K("m",KEY_M),K(".",KEY_DOT),K("Bksp",KEY_BACKSPACE)}
};
static volatile sig_atomic_t stopped;
static int fbfd=-1, uifd=-1, ttyfd=-1, inputfd=-1, rotation;
static int screen_w, screen_h, width, full_height, key_height, kb_height, page, mods;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;
static uint32_t *canvas;
static FT_Library ft;
static FT_Face face;
static struct libevdev *input;
static struct winsize saved_size[MAX_NR_CONSOLES+1];
static unsigned char resized[MAX_NR_CONSOLES+1];
static unsigned char resize_warning[MAX_NR_CONSOLES+1];
static struct touch_state gesture;
static int dirty=1, active_vt=-1;

static uint64_t milliseconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
static void stop(int signal_number) { (void)signal_number; stopped=1; }
static void fail(const char *message) { perror(message); exit(EXIT_FAILURE); }
static struct key key_at(int index)
{
    int r=index/COLS, c=index%COLS;
    if (page==1 && r>=1 && r<=3) return numbers[r-1][c];
    if (page==2 && r>=1 && r<=3) return functions[r-1][c];
    return common[r][c];
}
static int repeatable(int index)
{
    if (index<0) return 0;
    unsigned code=key_at(index).code;
    return code==KEY_BACKSPACE || code==KEY_DELETE || code==KEY_LEFT ||
           code==KEY_RIGHT || code==KEY_UP || code==KEY_DOWN;
}
static int send_event(unsigned type, unsigned code, int value)
{
    struct input_event e={.type=type,.code=code,.value=value};
    ssize_t n;
    do n=write(uifd,&e,sizeof(e)); while(n<0 && errno==EINTR);
    return n==(ssize_t)sizeof(e) ? 0 : -1;
}
static void release_all(void)
{
    if (uifd<0) return;
    for (unsigned code=1;code<=KEY_MAX;code++) send_event(EV_KEY,code,0);
    send_event(EV_SYN,SYN_REPORT,0);
}
static int tap(unsigned code, unsigned mask)
{
    static const unsigned modifier_codes[]={KEY_LEFTSHIFT,KEY_LEFTCTRL,KEY_LEFTALT};
    int error=0;
    for (int i=0;i<3;i++) if(mask&(1U<<i)) error|=send_event(EV_KEY,modifier_codes[i],1);
    error|=send_event(EV_KEY,code,1);
    error|=send_event(EV_SYN,SYN_REPORT,0);
    error|=send_event(EV_KEY,code,0);
    for (int i=2;i>=0;i--) if(mask&(1U<<i)) error|=send_event(EV_KEY,modifier_codes[i],0);
    error|=send_event(EV_SYN,SYN_REPORT,0);
    if(error) { release_all(); stopped=1; }
    return error;
}
static void dispatch(int index)
{
    struct key k=key_at(index);
    switch(k.code) {
    case ACT_SHIFT: mods^=MOD_SHIFT; break;
    case ACT_CTRL: mods^=MOD_CTRL; break;
    case ACT_ALT: mods^=MOD_ALT; break;
    case ACT_NUM: page=page==1?0:1; mods=0; break;
    case ACT_FN: page=page==2?0:2; mods=0; break;
    case ACT_RESET: mods=0; page=0; release_all(); break;
    default: tap(k.code,(unsigned)mods|(k.shift?MOD_SHIFT:0)); mods=0; break;
    }
    dirty=1;
}
/* Draw in upright logical coordinates, then rotate only the small keyboard area. */
static void rect(int x,int y,int w,int h,uint32_t color)
{
    for(int j=y;j<y+h;j++) for(int i=x;i<x+w;i++)
        if(i>=0 && i<width && j>=0 && j<kb_height) canvas[j*width+i]=color;
}
static void text(int x,int y,const char *label,int cellwidth,uint32_t color)
{
    int px=key_height/4;
    int limit=cellwidth/((int)strlen(label)+1);
    if(px>limit*3/2) px=limit*3/2;
    if(px<8) px=8;
    FT_Set_Pixel_Sizes(face,0,(unsigned)px);
    for(const unsigned char *p=(const unsigned char *)label;*p;p++) {
        if(FT_Load_Char(face,*p,FT_LOAD_RENDER)) continue;
        FT_GlyphSlot g=face->glyph;
        for(unsigned j=0;j<g->bitmap.rows;j++) for(unsigned i=0;i<g->bitmap.width;i++) {
            int dx=x+g->bitmap_left+(int)i, dy=y-g->bitmap_top+(int)j;
            unsigned char alpha=g->bitmap.buffer[j*g->bitmap.pitch+i];
            if(alpha && dx>=0 && dx<width && dy>=0 && dy<kb_height)
                canvas[dy*width+dx]=color;
        }
        x+=(int)(g->advance.x>>6);
    }
}
static uint32_t pixel(uint32_t rgb)
{
    uint32_t r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
    return ((r>>(8-var.red.length))<<var.red.offset)|
           ((g>>(8-var.green.length))<<var.green.offset)|
           ((b>>(8-var.blue.length))<<var.blue.offset);
}
static void draw(void)
{
    rect(0,0,width,kb_height,0x0b1220);
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) {
        if(r==4 && c==4) continue; /* space spans two columns */
        int index=r*COLS+c, x=c*width/COLS, y=r*key_height;
        int w=(c+(r==4 && c==3?2:1))*width/COLS-x;
        struct key k=key_at(index);
        int selected=gesture.id>=0 && !gesture.blocked && gesture.key==index;
        int enabled=(k.code==ACT_SHIFT && (mods&MOD_SHIFT)) ||
                    (k.code==ACT_CTRL && (mods&MOD_CTRL)) ||
                    (k.code==ACT_ALT && (mods&MOD_ALT)) ||
                    (k.code==ACT_NUM && page==1) || (k.code==ACT_FN && page==2);
        rect(x+2,y+2,w-4,key_height-4,selected?0x2563eb:enabled?0x7c3aed:0x243247);
        char label[16];
        snprintf(label,sizeof(label),"%s",k.label);
        if((mods&MOD_SHIFT) && strlen(label)==1 && label[0]>='a' && label[0]<='z') label[0]-=32;
        text(x+6,y+key_height*2/3,label,w-12,0xe2e8f0);
    }
    int pw=(rotation&1)?kb_height:width;
    int ph=(rotation&1)?width:kb_height;
    uint32_t *line=calloc((size_t)pw,sizeof(*line));
    if(!line) fail("framebuffer row");
    for(int py=0;py<ph;py++) {
        for(int px=0;px<pw;px++) {
            int lx=px,ly=py;
            if(rotation==1) { lx=py; ly=kb_height-1-px; }
            if(rotation==2) { lx=width-1-px; ly=kb_height-1-py; }
            if(rotation==3) { lx=width-1-py; ly=px; }
            line[px]=pixel(canvas[ly*width+lx]);
        }
        int ox=rotation==3?screen_w-kb_height:0;
        int oy=rotation==0?screen_h-kb_height:0;
        off_t offset=(off_t)(oy+py+var.yoffset)*fix.line_length+(ox+var.xoffset)*4;
        if(pwrite(fbfd,line,(size_t)pw*4,offset)!=(ssize_t)pw*4) { free(line); fail("write framebuffer"); }
    }
    free(line);
    dirty=0;
}
static int hit(int ax,int ay)
{
    const struct input_absinfo *a=libevdev_get_abs_info(input,ABS_MT_POSITION_X);
    const struct input_absinfo *b=libevdev_get_abs_info(input,ABS_MT_POSITION_Y);
    int64_t x=(int64_t)(ax-a->minimum)*(screen_w-1)/(a->maximum-a->minimum);
    int64_t y=(int64_t)(ay-b->minimum)*(screen_h-1)/(b->maximum-b->minimum);
    if(x<0 || x>=screen_w || y<0 || y>=screen_h) return -1;
    int lx=(int)x,ly=(int)y;
    if(rotation==1) { lx=(int)y; ly=screen_w-1-(int)x; }
    if(rotation==2) { lx=screen_w-1-(int)x; ly=screen_h-1-(int)y; }
    if(rotation==3) { lx=screen_h-1-(int)y; ly=(int)x; }
    ly-=full_height-kb_height;
    if(ly<0 || ly>=kb_height) return -1;
    int r=ly/key_height,c=lx*COLS/width;
    if(r==4 && c==4) c=3;
    return r*COLS+c;
}
static void frame(void)
{
    int count=0,id=-1,key=-1;
    for(int i=0;i<libevdev_get_num_slots(input);i++) {
        int tracking=libevdev_get_slot_value(input,(unsigned)i,ABS_MT_TRACKING_ID);
        if(tracking<0) continue;
        count++; id=tracking;
        key=hit(libevdev_get_slot_value(input,(unsigned)i,ABS_MT_POSITION_X),
                libevdev_get_slot_value(input,(unsigned)i,ABS_MT_POSITION_Y));
    }
    int before=gesture.key, blocked=gesture.blocked, down=gesture.id>=0;
    int action=touch_frame(&gesture,count,id,key,repeatable(key),milliseconds());
    if(action) dispatch(gesture.key);
    if(before!=gesture.key || blocked!=gesture.blocked || down!=(gesture.id>=0)) dirty=1;
}
static int console_ready(void)
{
    struct vt_stat vt;
    if(ioctl(ttyfd,VT_GETSTATE,&vt)<0 || vt.v_active<1 || vt.v_active>MAX_NR_CONSOLES) return 0;
    char path[32]; snprintf(path,sizeof(path),"/dev/tty%u",vt.v_active);
    int fd=open(path,O_RDWR|O_CLOEXEC|O_NOCTTY), mode=KD_GRAPHICS;
    if(fd<0) return 0;
    int ready=ioctl(fd,KDGETMODE,&mode)==0 && mode==KD_TEXT;
    if(active_vt!=vt.v_active) {
        active_vt=vt.v_active; touch_cancel(&gesture); mods=0; release_all(); dirty=1;
    }
    if(ready && !resized[vt.v_active]) {
        struct winsize size;
        if(ioctl(fd,TIOCGWINSZ,&size)==0 && size.ws_row>0) {
            saved_size[vt.v_active]=size;
            /* Linux checks these capacities even when only requesting metrics
               (data=NULL). Zero-initialized capacities return ENOSPC. */
            struct console_font_op font_op={.op=KD_FONT_OP_GET,
                .width=UINT_MAX,.height=UINT_MAX,.charcount=UINT_MAX};
            unsigned font_height=0;
            if(ioctl(fd,KDFONTOP,&font_op)==0) font_height=font_op.height;
            if(!font_height && size.ws_ypixel && size.ws_ypixel%size.ws_row==0)
                font_height=size.ws_ypixel/size.ws_row;
            if(!font_height || font_height>(unsigned)full_height) {
                if(!resize_warning[vt.v_active]) {
                    fputs("Console font metrics unavailable; drawing keyboard without reserving rows\n",stderr);
                    resize_warning[vt.v_active]=1;
                }
                close(fd); return ready; /* a font query must not block login */
            }
            /* Absolute font metrics avoid shrinking again after a restart. */
            unsigned rows=(unsigned)(full_height-kb_height)/font_height;
            size.ws_row=(unsigned short)(rows?rows:1);
            size.ws_ypixel=(unsigned short)(size.ws_row*font_height);
            if(ioctl(fd,TIOCSWINSZ,&size)==0) resized[vt.v_active]=1;
        }
    }
    close(fd); return ready;
}
static int usable(int fd)
{
    struct libevdev *candidate=NULL;
    if(libevdev_new_from_fd(fd,&candidate)<0) return 0;
    int ok=libevdev_has_event_code(candidate,EV_ABS,ABS_MT_TRACKING_ID) &&
           libevdev_has_event_code(candidate,EV_ABS,ABS_MT_POSITION_X) &&
           libevdev_has_event_code(candidate,EV_ABS,ABS_MT_POSITION_Y) &&
           libevdev_get_num_slots(candidate)>0;
    if(ok) {
        const struct input_absinfo *x=libevdev_get_abs_info(candidate,ABS_MT_POSITION_X);
        const struct input_absinfo *y=libevdev_get_abs_info(candidate,ABS_MT_POSITION_Y);
        ok=x->maximum>x->minimum && y->maximum>y->minimum;
    }
    if(ok) { input=candidate; return 1; }
    libevdev_free(candidate); return 0;
}
static void cleanup(void)
{
    release_all();
    if(uifd>=0) { ioctl(uifd,UI_DEV_DESTROY); close(uifd); }
    for(int i=1;i<=MAX_NR_CONSOLES;i++) if(resized[i]) {
        char path[32]; snprintf(path,sizeof(path),"/dev/tty%d",i);
        int fd=open(path,O_RDWR|O_CLOEXEC|O_NOCTTY);
        if(fd>=0) { ioctl(fd,TIOCSWINSZ,&saved_size[i]); close(fd); }
    }
    if(input) libevdev_free(input);
    if(inputfd>=0) close(inputfd);
    if(ttyfd>=0) close(ttyfd);
    if(fbfd>=0) close(fbfd);
    if(face) FT_Done_Face(face);
    if(ft) FT_Done_FreeType(ft);
    free(canvas);
}
int main(int argc,char **argv)
{
    const char *device=NULL,*font="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    int option; rotation=-1;
    while((option=getopt(argc,argv,"d:f:r:h"))!=-1) {
        if(option=='h') { puts("fbkeyboard [-d /dev/input/eventN] [-f font.ttf] [-r 0..3]\nSeven visible rows; one-shot modifiers; hold capped at 3 seconds."); return 0; }
        if(option=='d') device=optarg;
        else if(option=='f') font=optarg;
        else if(option=='r') {
            char *end; long value=strtol(optarg,&end,10);
            if(*optarg=='\0' || *end || value<0 || value>3) { fputs("rotation must be 0..3\n",stderr); return 2; }
            rotation=(int)value;
        } else return 2;
    }
    atexit(cleanup); signal(SIGTERM,stop); signal(SIGINT,stop); signal(SIGHUP,stop);
    touch_cancel(&gesture);
    if(rotation<0) {
        FILE *f=fopen("/sys/class/graphics/fbcon/rotate","r"); rotation=0;
        if(f) { if(fscanf(f,"%d",&rotation)!=1 || rotation<0 || rotation>3) rotation=0; fclose(f); }
    }
    fbfd=open("/dev/fb0",O_RDWR|O_CLOEXEC);
    if(fbfd<0 || ioctl(fbfd,FBIOGET_FSCREENINFO,&fix)<0 || ioctl(fbfd,FBIOGET_VSCREENINFO,&var)<0) fail("framebuffer");
    if(var.bits_per_pixel!=32 || var.red.length<1 || var.red.length>8 || var.green.length<1 || var.green.length>8 || var.blue.length<1 || var.blue.length>8 || var.red.offset+var.red.length>32 || var.green.offset+var.green.length>32 || var.blue.offset+var.blue.length>32 || var.xres<240 || var.yres<240 || var.xres>16384 || var.yres>16384) {
        fputs("Requires a 32-bit RGB framebuffer, dimensions 240..16384\n",stderr); return 1;
    }
    if((uint64_t)(var.xoffset+var.xres)*4>fix.line_length ||
       (uint64_t)(var.yoffset+var.yres)*fix.line_length>fix.smem_len) { fputs("Invalid framebuffer stride/offset\n",stderr); return 1; }
    screen_w=(int)var.xres; screen_h=(int)var.yres;
    width=(rotation&1)?screen_h:screen_w; full_height=(rotation&1)?screen_w:screen_h;
    key_height=full_height/(width>full_height?2:3)/ROWS;
    kb_height=key_height*ROWS;
    canvas=calloc((size_t)width*kb_height,sizeof(*canvas));
    if(!canvas) fail("keyboard buffer");
    if(FT_Init_FreeType(&ft) || FT_New_Face(ft,font,0,&face)) { fputs("Cannot load font\n",stderr); return 1; }
    ttyfd=open("/dev/tty0",O_RDWR|O_CLOEXEC|O_NOCTTY);
    if(ttyfd<0) fail("tty0");
    if(device) {
        inputfd=open(device,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(inputfd<0 || !usable(inputfd)) { fputs("Not a valid multitouch slot device\n",stderr); return 1; }
    } else {
        glob_t paths;
        if(glob("/dev/input/event*",0,NULL,&paths)!=0) { fputs("No input devices\n",stderr); return 1; }
        for(size_t i=0;i<paths.gl_pathc;i++) {
            int fd=open(paths.gl_pathv[i],O_RDONLY|O_NONBLOCK|O_CLOEXEC);
            if(fd<0) continue;
            if(usable(fd)) { inputfd=fd; break; }
            close(fd);
        }
        globfree(&paths);
        if(inputfd<0) { fputs("No multitouch touchscreen found\n",stderr); return 1; }
    }
    fprintf(stderr,"Touchscreen: %s; rotation %d\n",libevdev_get_name(input),rotation);
    uifd=open("/dev/uinput",O_WRONLY|O_CLOEXEC);
    if(uifd<0) fail("uinput");
    if(ioctl(uifd,UI_SET_EVBIT,EV_KEY)<0) fail("uinput EV_KEY");
    unsigned char supported[KEY_MAX+1]={0};
    supported[KEY_LEFTSHIFT]=supported[KEY_LEFTCTRL]=supported[KEY_LEFTALT]=1;
    for(int p=0;p<3;p++) {
        page=p;
        for(int i=0;i<ROWS*COLS;i++) {
            unsigned code=key_at(i).code;
            if(code && code<=KEY_MAX) supported[code]=1;
        }
    }
    page=0;
    for(unsigned code=1;code<=KEY_MAX;code++)
        if(supported[code] && ioctl(uifd,UI_SET_KEYBIT,code)<0) fail("uinput key capability");
    struct uinput_setup setup={.id={.bustype=BUS_VIRTUAL,.vendor=1,.product=1,.version=2}};
    snprintf(setup.name,sizeof(setup.name),"fbkeyboard");
    if(ioctl(uifd,UI_DEV_SETUP,&setup)<0 || ioctl(uifd,UI_DEV_CREATE)<0) fail("create uinput");
    int was_ready=-1, at_sync=1;
    while(!stopped) {
        int ready=console_ready();
        if(ready!=was_ready) { touch_cancel(&gesture); mods=0; release_all(); dirty=1; }
        was_ready=ready;
        struct pollfd pfd={.fd=inputfd,.events=POLLIN};
        int rc=poll(&pfd,1,50);
        if(rc<0 && errno!=EINTR) fail("poll touchscreen");
        if(pfd.revents&(POLLERR|POLLHUP|POLLNVAL)) { fputs("Touchscreen disconnected\n",stderr); return 1; }
        struct input_event event;
        int budget=512;
        while(budget-- && (rc=libevdev_next_event(input,LIBEVDEV_READ_FLAG_NORMAL,&event))>=0) {
            if(rc==LIBEVDEV_READ_STATUS_SYNC) {
                touch_cancel(&gesture); mods=0; release_all(); dirty=1;
                do rc=libevdev_next_event(input,LIBEVDEV_READ_FLAG_SYNC,&event);
                while(rc==LIBEVDEV_READ_STATUS_SYNC);
                if(rc<0 && rc!=-EAGAIN) { errno=-rc; fail("sync touchscreen"); }
                at_sync=1;
                continue;
            }
            at_sync=event.type==EV_SYN && event.code==SYN_REPORT;
            if(ready && at_sync) frame();
        }
        if(rc<0 && rc!=-EAGAIN && rc!=-EINTR) { errno=-rc; fail("read touchscreen"); }
        /* Timer also bounds repeats when the finger is motionless. */
        if(ready) { if(at_sync && rc==-EAGAIN) frame(); if(dirty) draw(); }
    }
    return 0;
}
