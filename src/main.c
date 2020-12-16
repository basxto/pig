#include <gb/gb.h>
#include <stdint.h>
#include <rand.h>
#include <stdbool.h>

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

typedef struct {
    uint8_t destination;
    uint8_t length;
} edge;

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
bool slowmode;
uint8_t overworld[map_size];
// for DFS backtracking and dijkstra back pointers
uint8_t backtrack[map_size];
// dijkstra's node queque
uint8_t queue[map_size];
// distance from starting node
uint8_t distance[map_size];
uint8_t tmp_tile[4];

// at most 2*map_size due to deduplication
// even though one row and col have less edges
// index/2 is the position of the source node
// first target is source-node +1 (next column)
// second target is source-node +map_width (next row)

// 640bytes :/
// has duplicates of all edges
// 4-edge array is easier to loop through than a node struct
// 0 is east etc.
// will contain all junctions and leafs
edge graph[map_size][4];

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
    set_bkg_tiles(3, 9, 12, 1, "PRESS0START1");
    // menu screen
    set_win_tiles(5,  7,  5, 1, "SEED2");
    set_win_tiles(7,  9,  5, 1, "START");
    set_win_tiles(7, 10,  5, 1, "0SLOW");
    set_win_tiles(7, 11,  6, 1, "REROLL");
    set_win_tiles(7, 12,  6, 1, "CHANGE");
    SHOW_BKG;
    DISPLAY_ON;
}

void write_hex(uint8_t x, uint8_t y, uint8_t num){
    uint8_t hex[2];
    hex[0] = FONT_HEX + (num >> 4);
    hex[1] = FONT_HEX + (num & 0xF);
    set_win_tiles(x, y, 2, 1, hex);
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

// generates the initial overworld with DFS
// also builds a graph with all junctions and leafs
void generate_overworld(){
    // reset path
    for(uint8_t i = 0; i < map_size; ++i){
        overworld[i] = 0;
    }
    uint8_t try;
    uint8_t visited = 1;
    uint8_t tile = rand() & 0x3F;// 16 tiles are left out (%64) map_size;
    // 0: E, 1: S, 2: W, 3: N
    uint8_t direction;
    // for graph construction
    uint8_t last_node = 0; // 0 is none
    uint8_t last_direction = 0; // we have to add the edge in right direction
    // leaf and junction == 0 means it’s going towards a leaf
    uint8_t last_distance = 0;
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
        uint8_t last_tile = backtrack[visited];
        if(try >= 4){
            // nothing was free, go one tile back
            --visited;
            // reverse naming since we backtrack
            next_tile = backtrack[visited-1];
            uint8_t bits = 0;
            uint8_t value = overworld[tile];
            // count bits
            for(uint8_t i = 0; i < 4; ++i){
                bits += value & 0x1;
                value >>= 1;
            }
            // more than 2 edges means junction <==> set bits >2
            if(bits > 2){// this is a junction
                uint8_t direction_index = 1;// defaults to south
                if(DEBUG){
                    set_win_tiles(0, 1, 2, 1, "NO");
                    draw_overworld();
                    waitpad(0xFF);
                    waitpadup();
                }
                // find out direction from which we are coming
                if(tile + 1 == last_tile){
                    direction_index = 0;
                }else if(tile - 1 == last_tile){
                    direction_index = 2;
                }else if(tile > last_tile){
                    direction_index = 3;
                }
                // add edge to this node
                --last_node;
                graph[tile][direction_index].destination = last_node;
                graph[tile][direction_index].length = last_distance;
                graph[last_node][last_direction].destination = tile;
                graph[last_node][last_direction].length = last_distance;
                // goes on in next if
            }
            // store data for the last visited node
            if(bits != 2){ // leafs or junctions
                last_node = tile+1;
                last_distance = 0;
                // find out direction which we are going
                last_direction = 1;
                if(next_tile + 1 == tile){
                    last_direction = 2;
                }else if(next_tile - 1 == tile){
                    last_direction = 0;
                }else if(next_tile < tile){
                    last_direction = 3;
                }
            }
            // this can overflow, but we don’t use it in that case
            tile = next_tile;
            ++last_distance;
        } else {
            if(last_node != 0){
                if(DEBUG){
                    set_win_tiles(0, 1, 2, 1, "JN");
                    draw_overworld();
                    waitpad(0xFF);
                    waitpadup();
                }
                //TODO: add actual nodes and edges ??
                // find out direction from which we are coming
                uint8_t direction_index = 1;
                if(tile + 1 == last_tile){
                    direction_index = 0;
                }else if(tile - 1 == last_tile){
                    direction_index = 2;
                }else if(tile > last_tile){
                    direction_index = 3;
                }
                // add edge to this node
                --last_node;
                graph[tile][direction_index].destination = last_node;
                graph[tile][direction_index].length = last_distance;
                graph[last_node][last_direction].destination = tile;
                graph[last_node][last_direction].length = last_distance;

                last_node = 0;
            }
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
            }
            if(slowmode){
                draw_overworld();
                wait_vbl_done();
            }
            //store tile in backtracker
            backtrack[visited-1]=tile;
            ++visited;
            // move to next tile
            tile = next_tile;
        }
    }while(visited);
    // check start tile
    tile = backtrack[0];
    uint8_t direction_index = 4; // 0-3 are legit indices
    --last_node;
    switch(overworld[tile]){
      case dir_E:
        direction_index = 0;
        break;
      case dir_S:
        direction_index = 1;
        break;
      case dir_W:
        direction_index = 2;
        break;
      case dir_N:
        direction_index = 3;
        break;
    }
    // if it’s a leaf
    if(direction_index != 4){
        graph[tile][direction_index].destination = last_node;
        graph[tile][direction_index].length = last_distance;
        graph[last_node][last_direction].destination = tile;
        graph[last_node][last_direction].length = last_distance;
    }
}

// travel the path until a node is found
// return length
// return tile through pointer
uint8_t nearest_node(uint8_t* tile, uint8_t direction){
    uint8_t current = *tile;
    uint8_t length = 0;
    // while it’s not a node
    while (length != 0xFF && (graph[current][0].length | graph[current][1].length | graph[current][2].length | graph[current][3].length) == 0){
        ++length;
        switch(direction){
          case dir_E:
            ++current;
            if((current%map_width) == 0){
                length = 0xFF;
            }
            break;
          case dir_W:
            if((current%map_width) == 0){
                length = 0xFF;
            }
            --current;
            break;
          case dir_N:
            if(current < map_width){
                length = 0xFF;
            }
            current -= map_width;
            break;
          case dir_S:
            current += map_width;
            if(current > map_size){
                length = 0xFF;
            }
            break;
        }
        // mirror direction
        direction = ((direction << 2) | (direction >> 2)) & 0xF;
        // works if only 2 bits are set
        direction ^= overworld[current];
    }
    *tile = current;
    return length;
}

// mhm check them checkerboard style with dijkstra
// generate shortcuts -> makes path cyclic
// find shortest distance between adjacent tiles with help of graph
void generate_shortcuts(){
    // 0x80 means from north, otherwise from west
    // max distance for 10x8 is 80
    edge longest = {0xFF, 0x00};
    edge src[2]; // virtual source node
    edge dst[2]; // virtual destination node
    uint8_t shortest;
    uint8_t value;
    uint8_t bits;
    uint8_t tile;
    for(uint8_t i = 0; i < map_size; ++i){
        src[0].length = 0xFF;
        src[1].length = 0xFF;
        dst[0].length = 0xFF;
        dst[1].length = 0xFF;
        shortest = 0xFF;
        tile=i;
        value = overworld[tile];
        uint8_t node = 0;
        // loop through directions
        for(uint8_t dir = 1; dir < 0x10; dir <<= 1){
            if(value & dir){
                src[node].destination = tile;
                src[node].length = nearest_node(&(src[node].destination), dir);
                ++node;
            }
        }
        tile = i + 1;
        value = overworld[tile];
        node = 0;
        for(uint8_t dir = 1; dir < 0x10; dir <<= 1){
            if(value & dir){
                dst[node].destination = tile;
                dst[node].length = nearest_node(&(dst[node].destination), dir);
                ++node;
            }
        }
        // clear distances
        for(uint8_t j = 0; j < map_size; ++j)
            distance[j]  = 0xFF;
        // TODO: do dijkstra here

    }
}

// generate the shore
void generate_terrain(){
    int height = 7;
    for(uint8_t x = 0; x < map_width; ++x){
        switch(rand() & 0x7){
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
    // reset graph
    for(uint8_t i = 0; i < map_size; ++i)
        for(uint8_t j = 0; j < 4; ++j)
            graph[i][j].length = 0;
    generate_overworld();
    if(slowmode){
        draw_overworld();
        wait_vbl_done();
        wait_vbl_done();
    }
    generate_shortcuts();
    if(slowmode){
        draw_overworld();
        wait_vbl_done();
        wait_vbl_done();
    }
    if(DEBUG){
        draw_overworld();
        waitpad(0xFF);
        waitpadup();
        // for debugging visualization invert all nodes
        // manually redraws all node tiles
        for(uint8_t i = 0; i < map_size; ++i){
            uint8_t tile = 0;
            for(uint8_t j = 0; j < 4; ++j)
                if(graph[i][j].length != 0)
                    tile |= 0x1<<j;
            if(tile != 0){
                overworld[i] = tile;
                //overworld[i] = ~overworld[i];
            }
        }
        draw_overworld();
        waitpad(0xFF);
        waitpadup();
    }
    generate_terrain();
}

void main() {
    init_screen();
    uint8_t x = 3; // reroll
    uint8_t y = 0;
    // trigger reroll on first run
    uint8_t buttons = J_A;
    waitpad(0xFF);
    SHOW_WIN;
    while(1){
        // handle input
        if(x == 0){
            if(buttons & J_LEFT){
                ++y;
            } else if(buttons & J_RIGHT){
                --y;
            }
            if(y == u8(-1)){
                // jump to last one
                y = 3;
            }
            if(y == 4){
                // jump to first one
                y = 0;
            }

            uint16_t step = 0x1;
            if(y == 1)
                step = 0x10;
            if(y == 2)
                step = 0x100;
            if(y == 3)
                step = 0x1000;

            if(buttons & J_UP){
                seed += step;
            } else if(buttons & J_DOWN){
                seed -= step;
            }

            if(buttons & (J_B | J_A | J_SELECT | J_START)){
                // jump back to select
                x = 4;
            }
        } else {
            if(buttons & J_UP){
                --x;
            } else if(buttons & J_DOWN){
                ++x;
            }
            if(x == 0){
                // jump to last one
                x = 4;
            }
            if(x == 5){
                // jump to first one
                x = 1;
            }

            if(buttons & (J_B | J_A | J_SELECT | J_START)){
                switch(x){
                  case 2: // slow mode
                    slowmode = true;
                    move_win(7, 16*8);
                  case 1: // start
                    if(DEBUG)
                        move_win(7, 16*8);
                    set_win_tiles(16, 0, 4, 1, "SEED");
                    write_hex(16, 1, (uint8_t)(seed>>8));
                    write_hex(18, 1, (uint8_t)(seed));
                    generate_map();
                    draw_overworld();
                    if(!DEBUG)
                        move_win(7, 16*8);
                    // wait for any button
                    waitpad(0xFF);
                    slowmode = false;
                    break;
                  case 3: // reroll
                    seed = LY_REG;
                    seed |= (uint16_t)DIV_REG << 8;
                    break;
                  case 4: // change
                    x = 0;
                    y = 0;
                    break;
                }
            }
        }
        waitpadup();
        // draw menu
        move_win(7, 0);
        fill_win_rect(0, 0, 20, 2, '0');
        // clean up arrows
        fill_win_rect(11, 6, 4, 3, '0');
        fill_win_rect(5, 9, 1, 4, '0');
        // draw arrow(s)
        if(x == 0)
            set_win_tiles(14-y, 6, 1, 3, "^0]");
        else
            set_win_tiles(5, 8+x, 1, 1, "3");
        // draw seed
        write_hex(11, 7, (uint8_t)(seed>>8));
        write_hex(13, 7, (uint8_t)(seed));

        buttons = waitpad(0xFF);
    }
}