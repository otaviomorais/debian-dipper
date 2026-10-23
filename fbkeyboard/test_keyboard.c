/* Exercise production dispatch, libevdev frame handling and all rotations. */
#define main keyboard_program_main
#include "fbkeyboard.c"
#undef main
#include <assert.h>

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
    test_events(); test_frames(); test_render();
    puts("keyboard: complete key-up chords, fast MT taps, nonzero axis minima, layout bounds, 12 page/rotation renders: OK");
    return 0;
}
