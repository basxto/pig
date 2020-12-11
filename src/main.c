#include <gb/gb.h>
#include <stdint.h>

#include "../build/squont8ng_micro_2bpp.c"
#include "../build/blowharder_path_2bpp.c"

#define PATH_START (128)
#define FONT_START (48)
#define FONT_ASCII (FONT_START-48)
#define FONT_HEX   (FONT_START+7)

void init_screen() {
    HIDE_BKG;
    HIDE_WIN;
    HIDE_SPRITES;
    DISPLAY_OFF;
    set_win_data(PATH_START, (build_blowharder_path_2bpp_len/16), build_blowharder_path_2bpp);
    set_win_data(FONT_START, (build_squont8ng_micro_2bpp_len/16), build_squont8ng_micro_2bpp);
    set_bkg_tiles(5, 5, 13, 1, "PRESS START!");
    SHOW_BKG;
    DISPLAY_ON;
}

void main() {
    init_screen();
    //puts("Push any key (2)");
    //waitpad(0xFF);
    //waitpadup();
    
    waitpad(0xFF);
    waitpadup();
    set_bkg_tiles(5, 5, 13, 1, "            ");
    uint16_t seed = LY_REG;
    seed |= (uint16_t)DIV_REG << 8;
    unsigned char num []= {(uint8_t)(seed & 0xF)+FONT_HEX, (uint8_t)((seed>>4) & 0xF)+FONT_HEX, (uint8_t)((seed>>8) & 0xF)+FONT_HEX, (uint8_t)((seed>>12) & 0xF)+FONT_HEX};
    set_bkg_tiles(5, 5, 4, 1, num);
}