/* Exercise production dispatch, libevdev frame handling and all rotations. */
#include <fcntl.h>
#include <stdarg.h>
#include <sys/ioctl.h>
static int console_test_mode;
static int console_test_open(const char *path, int flags);
static int console_test_ioctl(int fd, unsigned long request, ...);
#define open console_test_open
#define ioctl console_test_ioctl
#define main keyboard_program_main
#include "fbkeyboard.c"
#undef main
#undef ioctl
#undef open
#include <assert.h>

static struct winsize console_test_size;
static int console_test_open(const char *path, int flags)
{
    if(console_test_mode && !strcmp(path,"/dev/tty1")) return dup(ttyfd);
    return open(path,flags);
}
static int console_test_ioctl(int fd, unsigned long request, ...)
{
    va_list args;
    va_start(args,request);
    void *argument=va_arg(args,void *);
    va_end(args);
    if(!console_test_mode) return ioctl(fd,request,argument);
    if(request==VT_GETSTATE) { ((struct vt_stat *)argument)->v_active=1; return 0; }
    if(request==KDGETMODE) { *(int *)argument=KD_TEXT; return 0; }
    if(request==TIOCGWINSZ) { *(struct winsize *)argument=console_test_size; return 0; }
    if(request==TIOCSWINSZ) { console_test_size=*(struct winsize *)argument; return 0; }
    if(request==KDFONTOP) {
        struct console_font_op *op=argument;
        /* Linux con_font_get rejects undersized width/height even with data=NULL. */
        assert(op->op==KD_FONT_OP_GET && !op->data);
        if(console_test_mode>1) { errno=ENOSYS; return -1; }
        if(op->width<16 || op->height<32) { errno=ENOSPC; return -1; }
        op->width=16; op->height=32; op->charcount=512;
        return 0;
    }
    assert(!"Unexpected console ioctl");
    return -1;
}
static void test_login_console(void)
{
    ttyfd=open("/dev/null",O_RDWR); assert(ttyfd>=0);
    screen_w=1080; screen_h=2248; width=1080; full_height=2248;
    key_height=full_height/3/ROWS; kb_height=key_height*ROWS;
    console_test_mode=1;
    console_test_size=(struct winsize){.ws_row=70,.ws_col=67,.ws_xpixel=1072,.ws_ypixel=2240};
    active_vt=-1; resized[1]=0;
    assert(console_ready());
    assert(resized[1] && console_test_size.ws_row==(full_height-kb_height)/32);
    /* A restart must reserve the same absolute rows, not shrink the VT again. */
    resized[1]=0;
    assert(console_ready());
    assert(resized[1] && console_test_size.ws_row==(full_height-kb_height)/32);
    console_test_mode=2; resized[1]=0;
    console_test_size=(struct winsize){.ws_row=70,.ws_col=67,.ws_xpixel=1072,.ws_ypixel=2240};
    assert(console_ready());
    assert(resized[1] && console_test_size.ws_row==(full_height-kb_height)/32);
    resized[1]=0;
    assert(console_ready());
    assert(resized[1] && console_test_size.ws_row==(full_height-kb_height)/32);
    /* Missing font and pixel metrics must not hide the login keyboard. */
    console_test_mode=3; resized[1]=0;
    console_test_size=(struct winsize){.ws_row=70,.ws_col=67};
    assert(console_ready());
    assert(!resized[1] && console_test_size.ws_row==70);
    console_test_mode=0; resized[1]=0; active_vt=-1;
    close(ttyfd); ttyfd=-1;
}

static void test_events(void)
{
    int pipes[2]; assert(pipe(pipes)==0); uifd=pipes[1];
    dispatch(40); /* Ctrl is UI state only, not a held uinput key. */
    assert(mods==MOD_CTRL);
    dispatch(23); /* Ctrl+F */
    assert(!mods);
    close(pipes[1]); uifd=-1;
    struct input_event e;
    int held[KEY_MAX+1]={0}, seen_ctrl=0, seen_f=0, count=0;
    while(read(pipes[0],&e,sizeof(e))==(ssize_t)sizeof(e)) {
        count++;
        if(e.type!=EV_KEY) continue;
        assert(e.code<=KEY_MAX);
        held[e.code]=e.value;
        if(e.code==KEY_LEFTCTRL && e.value==1) seen_ctrl=1;
        if(e.code==KEY_F && e.value==1) { assert(held[KEY_LEFTCTRL]); seen_f=1; }
    }
    close(pipes[0]);
    assert(count==6 && seen_ctrl && seen_f);
    for(int i=0;i<=KEY_MAX;i++) assert(!held[i]);
    assert(pipe(pipes)==0); uifd=pipes[1];
    page=1; dispatch(20); /* ! includes a bounded Shift chord */
    close(pipes[1]); uifd=-1;
    while(read(pipes[0],&e,sizeof(e))==(ssize_t)sizeof(e)) {
        if(e.type==EV_KEY) held[e.code]=e.value;
        if(e.type==EV_KEY && e.code==KEY_1 && e.value==1) assert(held[KEY_LEFTSHIFT]);
    }
    close(pipes[0]);
    for(int i=0;i<=KEY_MAX;i++) assert(!held[i]);
    page=0;
}
static void test_frames(void)
{
    input=libevdev_new(); assert(input);
    struct input_absinfo slots={.minimum=0,.maximum=1};
    struct input_absinfo x={.minimum=100,.maximum=1100}, y={.minimum=-50,.maximum=950};
    struct input_absinfo tracking={.minimum=0,.maximum=65535};
    assert(!libevdev_enable_event_code(input,EV_ABS,ABS_MT_SLOT,&slots));
    assert(!libevdev_enable_event_code(input,EV_ABS,ABS_MT_TRACKING_ID,&tracking));
    assert(!libevdev_enable_event_code(input,EV_ABS,ABS_MT_POSITION_X,&x));
    assert(!libevdev_enable_event_code(input,EV_ABS,ABS_MT_POSITION_Y,&y));
    screen_w=1080;screen_h=2248;rotation=0;width=screen_w;full_height=screen_h;
    key_height=full_height/3/ROWS;kb_height=key_height*ROWS;
    for(int i=0;i<2;i++) assert(!libevdev_set_slot_value(input,(unsigned)i,ABS_MT_TRACKING_ID,-1));
    touch_reset(&gesture);
    int pipes[2];assert(!pipe(pipes));uifd=pipes[1];
    /* down+up queued together, same coordinate tapped twice */
    for(int i=0;i<2;i++) {
        libevdev_set_slot_value(input,0,ABS_MT_POSITION_X,150);
        int y_middle=full_height-kb_height+5*key_height+key_height/2;
        libevdev_set_slot_value(input,0,ABS_MT_POSITION_Y,-50+y_middle*1000/(screen_h-1));
        libevdev_set_slot_value(input,0,ABS_MT_TRACKING_ID,100+i);
        frame();
        libevdev_set_slot_value(input,0,ABS_MT_TRACKING_ID,-1);
        frame();
    }
    close(pipes[1]);uifd=-1;
    struct input_event e;int presses=0;
    while(read(pipes[0],&e,sizeof(e))==(ssize_t)sizeof(e))
        if(e.type==EV_KEY && e.code==KEY_LEFT && e.value==1) presses++;
    assert(presses==2);close(pipes[0]);
    for(int r=0;r<4;r++) {
        rotation=r;width=(r&1)?screen_h:screen_w;full_height=(r&1)?screen_w:screen_h;
        key_height=full_height/(width>full_height?2:3)/ROWS;kb_height=key_height*ROWS;
        /* No point may become an out-of-bounds layout index. */
        for(int ax=100;ax<=1100;ax+=10) for(int ay=-50;ay<=950;ay+=10) {
            int k=hit(ax,ay); assert(k>=-1 && k<ROWS*COLS);
        }
    }
    libevdev_free(input);input=NULL;
}
static void test_render(void)
{
    char path[]="/tmp/fbkeyboard-test-XXXXXX";
    fbfd=mkstemp(path);assert(fbfd>=0);unlink(path);
    screen_w=1080;screen_h=2248;
    fix.line_length=screen_w*4;
    var.red=(struct fb_bitfield){.offset=16,.length=8};
    var.green=(struct fb_bitfield){.offset=8,.length=8};
    var.blue=(struct fb_bitfield){.offset=0,.length=8};
    assert(!FT_Init_FreeType(&ft));
    assert(!FT_New_Face(ft,"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",0,&face));
    for(int r=0;r<4;r++) {
        rotation=r;width=(r&1)?screen_h:screen_w;full_height=(r&1)?screen_w:screen_h;
        key_height=full_height/(width>full_height?2:3)/ROWS;kb_height=key_height*ROWS;
        canvas=calloc((size_t)width*kb_height,sizeof(*canvas));assert(canvas);
        for(int p=0;p<3;p++) { page=p;draw(); }
        free(canvas);canvas=NULL;
    }
    close(fbfd);fbfd=-1;FT_Done_Face(face);face=NULL;FT_Done_FreeType(ft);ft=NULL;
}
int main(void)
{
    test_login_console(); test_events(); test_frames(); test_render();
    puts("keyboard: login font ABI/fallback, restart sizing, complete key-up chords, fast MT taps, layout bounds, 12 page/rotation renders: OK");
    return 0;
}
