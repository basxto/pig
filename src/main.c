#include <gb/gb.h>
#include <stdint.h>
#include <rand.h>

#include "../build/squont8ng_micro_2bpp.c"
#include "../build/blowharder_path_2bpp.c"

#define PATH_START (128)
#define FONT_START (48)
#define FONT_ASCII (FONT_START-48)
#define FONT_HEX   (FONT_START+7)
#define map_width  (10)
#define map_size   (8*map_width)

uint16_t seed;
uint8_t overworld[map_size];
uint8_t tmp_tile[4];

void init_screen() {
    HIDE_BKG;
    HIDE_WIN;
    HIDE_SPRITES;
    DISPLAY_OFF;
    set_bkg_data(PATH_START, (build_blowharder_path_2bpp_len/16), build_blowharder_path_2bpp);
    set_bkg_data(FONT_START, (build_squont8ng_micro_2bpp_len/16), build_squont8ng_micro_2bpp);
    // first screen
    set_bkg_tiles(3, 9, 13, 1, "PRESS0START1");
    // menu screen
    set_win_tiles(5,  7,  5, 1, "SEED2");
    set_win_tiles(7,  9,  5, 1, "START");
    set_win_tiles(7, 10,  6, 1, "REROLL");
    set_win_tiles(7, 11,  6, 1, "CHANGE");
    SHOW_BKG;
    DISPLAY_ON;
}

void write_hex(uint8_t x, uint8_t y, uint8_t num){
    uint8_t hex[2];
    hex[0] = FONT_HEX + (num >> 4);
    hex[1] = FONT_HEX + (num & 0xF);
    set_win_tiles(x, y, 2, 1, hex);
}

void roll_seed(){
    seed = LY_REG;
    seed |= (uint16_t)DIV_REG << 8;
    write_hex(11, 7, (uint8_t)(seed>>8));
    write_hex(13, 7, (uint8_t)(seed));
}

void draw_overworld(){
    uint8_t x = 0;
    uint8_t y = 0;
    for(uint8_t i = 0; i < map_size; ++i){
        tmp_tile[0] = PATH_START + overworld[i]*4;
        tmp_tile[2] = tmp_tile[0]+1;
        tmp_tile[1] = tmp_tile[2]+1;
        tmp_tile[3] = tmp_tile[1]+1;
        set_bkg_tiles(x, y, 2, 2, tmp_tile);
        x += 2;
        if(x >= (map_width*2)){
            x = 0;
            y += 2;
        }
    }
}

void generate_overworld(){
    // reset path
    for(uint8_t i = 0; i < map_size; ++i){
        overworld[i] = 0;
    }
    uint8_t start = arand() & 0x3F;// 16 tiles are left out (%64) map_size;
    overworld[start] = 2;
    write_hex(16, 0, start);
    write_hex(18, 0, overworld[start]);
}

void generate_map(){
    // initialize random numbers
    initarand(seed);
    generate_overworld();
}

void main() {
    init_screen();
    while(1){
        waitpad(0xFF);
        waitpadup();
        move_win(7, 0);
        SHOW_WIN;
        roll_seed();
        set_win_tiles(5, 9,  1, 1, "3");
        waitpad(0xFF);
        waitpadup();
        generate_map();
        draw_overworld();
        move_win(7, 17*8);
    }
}