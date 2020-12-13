#include <gb/gb.h>
#include <stdint.h>
#include <rand.h>

#include "../build/squont8ng_micro_2bpp.c"
#include "../build/blowharder_path_2bpp.c"

#define u8(x)      (uint8_t)(x)
#define PATH_START u8(128)
#define FONT_START u8(48)
#define FONT_ASCII u8(FONT_START-48)
#define FONT_HEX   u8(FONT_START+7)
#define map_width  u8(10)
#define map_size   u8(8*map_width)
#define dir_E      u8(1<<0)
#define dir_S      u8(1<<1)
#define dir_W      u8(1<<2)
#define dir_N      u8(1<<3)

uint16_t seed;
uint8_t overworld[map_size];
uint8_t backtrack[map_size];
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
    uint8_t try = 0;
    uint8_t visited = 1;
    uint8_t tile = arand() & 0x3F;// 16 tiles are left out (%64) map_size;
    // 0: E, 1: S, 2: W, 3: N
    uint8_t direction;// = 1<<(arand() & 0x3);
    //overworld[tile] = direction;
    //write_hex(16, 0, tile);
    //write_hex(18, 0, overworld[tile]);
    do{
        uint8_t next_tile = tile;
        try = 0;
        direction = 1<<(arand() & 0x3);
        write_hex(8, 0, overworld[tile]);
        write_hex(11, 0, direction);
        // check if direction is free
        // rotate otherwise
        // also calculate next tile
        do{
            if(direction == dir_E){
                // is that direction free && is it within map bounds?
                if(!(overworld[tile] & dir_E) && (tile%map_width) != map_width-1){
                    ++next_tile;
                    break;
                }
            }else if(direction == dir_W){
                if(!(overworld[tile] & dir_W) && (tile%map_width) != 0){
                    --next_tile;
                    break;
                }
            }else if(direction == dir_N){
                if(!(overworld[tile] & dir_N) && tile < map_width){
                    next_tile -= map_width;
                    break;
                }
            }else if(!(overworld[tile] & dir_S) && tile < (map_size - map_width)){
                next_tile += map_width;
                break;
            }
            // circular shift
            direction = ((direction << 1) | (direction >> 3)) & 0xF;
            ++try;
        }while(try < 4);
        if(try >= 4){
            //nothing was free :/
            break;
        }
        // actually draw path
        overworld[tile] |= direction;
        //i = ((i << 4) | (i >> 4));
        direction = ((direction << 2) | (direction >> 2)) & 0xF;
        write_hex(13, 0, direction);
        // swap nibbles
        overworld[next_tile] |= direction;// |= u8( u8(direction & u8(0x0F)) << u8(4) | u8(direction & u8(0xF0)) >> u8(4) );
        //TODO: remove this debug stuff
        write_hex(16, 0, tile);
        write_hex(18, 0, next_tile);
        //draw_overworld();
        //waitpad(0xFF);
        //waitpadup();
        next_tile = tile;
        ++visited;
        if(visited >= map_size)
            break;
    }while(visited);
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