#ifndef AUX_H
#define AUX_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#define WINDOW_SIZE 20
#define MAX_LIZARDS 4
#define MAX_ROACHES_PER_CLIENT 10
#define TAIL_SIZE 5
#define RESPAWN_TIME 5000 /* ms */
#define POINTS_TO_WIN 50

typedef enum direction_t {UP, DOWN, LEFT, RIGHT, NONE} direction_t;

typedef struct remote_char_t
{   
    int msg_type; /* 0-join roaches, 1-move roaches, 2-join lizards, 3-move lizards, 4-disconnect lizard, 
    5-disconnect roaches, 6-join wasp, 7-move wasp, 8-disconnect wasp */
    uint32_t id;
    char ch[10];
    int nchars; 
    direction_t direction[10];
}remote_char_t;

typedef struct ch_info_t
{
    int ch;
    int pos_x, pos_y;
} ch_info_t;

typedef struct pos_roachesNwasps
{
    uint32_t id;
    ch_info_t char_data[10];
    int nchars; 
    bool active[10];
    bool valid;
} pos_roachesNwasps;

typedef struct pos_lizards
{
    double score;
    uint32_t id;
    ch_info_t char_data;
    direction_t prevdirection;
    bool valid;
    bool alive;
} pos_lizards;

typedef struct remote_display_msg
{   
    remote_char_t message_human_client;
    ch_info_t char_data[100];
    int n_chars;
}remote_display_msg;

typedef struct msg
{   
    int x_upd;
    int y_upd;
    char field[WINDOW_SIZE][WINDOW_SIZE];
    char ch[MAX_LIZARDS];
    double lizards[MAX_LIZARDS];
    bool lizard_valid[MAX_LIZARDS];
    bool lizard_alive[MAX_LIZARDS];
}msg;

#endif

