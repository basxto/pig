#include <gb/gb.h>
#include <stdint.h>
#include <rand.h>

#include "../build/squont8ng_micro_2bpp.c"
#include "../build/blowharder_path_2bpp.c"
#include "../build/blowharder_bridge_2bpp.c"

#ifndef DEBUG
#define DEBUG      (0)
#endif

// palettes from doavi
const unsigned int overworld_pal[][4] = {{
	RGB(12, 25, 0), RGB(28, 16, 0), RGB(18, 7, 0), RGB(0, 0, 0)
},{
	RGB(28, 16, 0), RGB(8, 20, 31), RGB(18, 7, 0), RGB(0, 0, 0)
},{
	RGB(12, 25, 0), RGB(18, 7, 0), RGB(3, 14, 0), RGB(0, 0, 0)
},{
	RGB(12, 25, 0), RGB(18, 18, 18), RGB(10, 10, 10), RGB(0, 0, 0)
}};

#define u8(x)           (uint8_t)(x)
#define PATH_START      u8(128)
#define BRIDGE_START    u8(PATH_START+(build_blowharder_path_2bpp_len/16))
#define FONT_START      u8(48)
#define FONT_ASCII      u8(FONT_START-48)
#define FONT_HEX        u8(FONT_START+7)
#define map_width       u8(10)
#define map_size        u8(8*map_width)
#define dir_E           u8(1<<0)
#define dir_S           u8(1<<1)
#define dir_W           u8(1<<2)
#define dir_N           u8(1<<3)
#define terr_water      u8(1<<4)

uint16_t seed;
uint8_t overworld[map_size];
uint8_t backtrack[map_size];
uint8_t tmp_tile[4];

// placeholder for the function available in gbdk-2020 4.0.1
void fill_bkg_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t tile){
    for(uint8_t tmp_x = x; tmp_x<x+w; ++tmp_x)
        for(uint8_t tmp_y = y; tmp_y<y+h; ++tmp_y)
            set_bkg_tiles(tmp_x, tmp_y, 1, 1, &tile);
}
void fill_win_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t tile){
    for(uint8_t tmp_x = x; tmp_x<x+w; ++tmp_x)
        for(uint8_t tmp_y = y; tmp_y<y+h; ++tmp_y)
            set_win_tiles(tmp_x, tmp_y, 1, 1, &tile);
}

void init_screen() {
    HIDE_BKG;
    HIDE_WIN;
    HIDE_SPRITES;
    DISPLAY_OFF;
    // initialize palettes
    set_bkg_palette(0, 4, overworld_pal[0]);
    // initialize assigned palettes
    VBK_REG=1;
    fill_bkg_rect(0, 0, 20, 18, 3);
    fill_win_rect(0, 0, 20, 18, 3);
    VBK_REG=0;
    fill_bkg_rect(0, 0, 20, 18, '0');
    fill_win_rect(0, 0, 20, 18, '0');
    // load tilesets
    set_bkg_data(PATH_START,   (build_blowharder_path_2bpp_len/16),   build_blowharder_path_2bpp);
    set_bkg_data(BRIDGE_START, (build_blowharder_bridge_2bpp_len/16), build_blowharder_bridge_2bpp);
    set_bkg_data(FONT_START,   (build_squont8ng_micro_2bpp_len/16),   build_squont8ng_micro_2bpp);
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
        VBK_REG=1;
        tmp_tile[0] = (overworld[i]>>4)&0x1;
        tmp_tile[2] = tmp_tile[0];
        tmp_tile[1] = tmp_tile[2];
        tmp_tile[3] = tmp_tile[1];
        set_bkg_tiles(x, y, 2, 2, tmp_tile);
        VBK_REG=0;
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
    uint8_t try;
    uint8_t visited = 1;
    uint8_t tile = arand() & 0x3F;// 16 tiles are left out (%64) map_size;
    // 0: E, 1: S, 2: W, 3: N
    uint8_t direction;
    if(DEBUG)
        set_win_tiles(3, 0, 17, 1, "V0TY0OT0D0MD0T0NT");
    do{
        uint8_t next_tile;
        try = 0;
        direction = 1<<(arand() & 0x3);
        // check if direction is free
        // rotate otherwise
        // also calculate next tile
        do{
            next_tile = tile;
            if(direction == dir_E){
                ++next_tile;
                // is that direction free && is it within map bounds? && next_tile is empty?
                if(!(overworld[tile] & dir_E) && (tile%map_width) != map_width-1 && !(overworld[next_tile])){
                    break;
                }
            }else if(direction == dir_W){
                --next_tile;
                if(!(overworld[tile] & dir_W) && (tile%map_width) != 0 && !(overworld[next_tile])){
                    break;
                }
            }else if(direction == dir_N){
                next_tile -= map_width;
                if(!(overworld[tile] & dir_N) && tile >= map_width && !(overworld[next_tile])){
                    break;
                }
            }else{
                next_tile += map_width;
                if(!(overworld[tile] & dir_S) && tile < (map_size - map_width) && !(overworld[next_tile])){
                    break;
                }
            }
            // circular shift
            direction = ((direction << 1) | (direction >> 3)) & 0xF;
            ++try;
        }while(try < 4);
        if(try < 4){
            if(DEBUG){
                write_hex(3, 1, visited);
                write_hex(8, 1, overworld[tile]);
                write_hex(11, 1, direction);
                write_hex(5, 1, try);
            }
            // actually draw path
            overworld[tile] |= direction;
            direction = ((direction << 2) | (direction >> 2)) & 0xF;
            if(DEBUG)
                write_hex(13, 1, direction);
            overworld[next_tile] |= direction;
            if(DEBUG){
                write_hex(16, 1, tile);
                write_hex(18, 1, next_tile);
                draw_overworld();
                waitpad(0xFF);
                waitpadup();
            }
            //store tile in backtracker
            backtrack[visited-1]=tile;
            ++visited;
            // move to next tile
            tile = next_tile;
            // we visited all tiles and don’t have to go through the whole backtrack
            if(visited >= map_size)
                break;
        } else {
            // nothing was free, go one tile back
            --visited;
            // this can overflow, but we don’t use it in that case
            tile = backtrack[visited-1];
        }
    }while(visited);
}

void generate_terrain(){
    int height = 7;
    for(uint8_t x = 0; x < map_width; ++x){
        switch(arand() & 0x7){
            case 0:
                ++height;
                break;
            case 1:
            case 2:
                --height;
                break;
            default:
                break;
        }
        if(height==4)
            height = 5;
        if(height==8)
            height = 7;
        for(uint8_t y = height*map_width; y < (8*map_width); y+=map_width){
            overworld[x+y] |= terr_water;
        }
    }
}

void generate_map(){
    // initialize random numbers
    initarand(seed);
    generate_overworld();
    generate_terrain();
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
        if(DEBUG)
            move_win(7, 16*8);
        generate_map();
        draw_overworld();
        if(!DEBUG)
            move_win(7, 16*8);
        set_win_tiles(16, 0, 17, 1, "SEED");
        write_hex(16, 1, (uint8_t)(seed>>8));
        write_hex(18, 1, (uint8_t)(seed));
    }
}