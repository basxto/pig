#include <gb/gb.h>
#include <stdint.h>
#include <rand.h>

#include "../build/squont8ng_micro_2bpp.c"
#include "../build/blowharder_path_2bpp.c"

#define PATH_START (128)
#define FONT_START (48)
#define FONT_ASCII (FONT_START-48)
#define FONT_HEX   (FONT_START+7)

uint16_t seed;

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

void roll_seed(){
    uint16_t seed = LY_REG;
    seed |= (uint16_t)DIV_REG << 8;
    unsigned char num []= {(uint8_t)(seed & 0xF)+FONT_HEX, (uint8_t)((seed>>4) & 0xF)+FONT_HEX, (uint8_t)((seed>>8) & 0xF)+FONT_HEX, (uint8_t)((seed>>12) & 0xF)+FONT_HEX};
    set_win_tiles(11, 7, 4, 1, num);
}

void generate_map(){
    // initialize random numbers
    initarand(seed);
}

void main() {
    init_screen();
    waitpad(0xFF);
    waitpadup();
    SHOW_WIN;
    roll_seed();
    set_win_tiles(5, 9,  1, 1, "3");
    waitpad(0xFF);
    waitpadup();
    generate_map();
}