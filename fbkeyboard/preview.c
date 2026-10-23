/* Render the production keyboard without a framebuffer/touchscreen device. */
#define main keyboard_program_main
#include "fbkeyboard.c"
#undef main
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) { fputs("preview-keyboard OUTPUT.ppm [0|1|2]\n",stderr); return 2; }
    screen_w=width=1080; screen_h=full_height=2248;
    kb_height=full_height*34/100;
    layout();
    if(argc==3) { page=atoi(argv[2]); if(page<0 || page>2) return 2; }
    fix.line_length=screen_w*4;
    var.red=(struct fb_bitfield){.offset=16,.length=8};
    var.green=(struct fb_bitfield){.offset=8,.length=8};
    var.blue=(struct fb_bitfield){.offset=0,.length=8};
    if(FT_Init_FreeType(&ft) || FT_New_Face(ft,"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",0,&face)) return 1;
    canvas=calloc((size_t)width*kb_height,sizeof(*canvas));
    char temporary[]="/tmp/dipper-preview-XXXXXX";
    fbfd=mkstemp(temporary); unlink(temporary);
    if(!canvas || fbfd<0) return 1;
    draw();
    FILE *output=fopen(argv[1],"wb");
    if(!output) return 1;
    fprintf(output,"P6\n%d %d\n255\n",width,kb_height);
    for(int i=0;i<width*kb_height;i++) {
        unsigned char rgb[]={(unsigned char)(canvas[i]>>16),(unsigned char)(canvas[i]>>8),(unsigned char)canvas[i]};
        if(fwrite(rgb,1,3,output)!=3) return 1;
    }
    if(fclose(output)) return 1;
    cleanup();
    return 0;
}
