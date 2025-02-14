#ifndef AUX_H
#define AUX_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#define WINDOW_SIZE 20 
#define MAX_LIZARDS 26
#define MAX_ROACHES_PER_CLIENT 10
#define TAIL_SIZE 5
#define RESPAWN_TIME 5000 /* ms */
#define POINTS_TO_WIN 50

typedef enum direction_t {UP, DOWN, LEFT, RIGHT, NONE} direction_t;

typedef struct remote_char_t
{   
    int msg_type; /* 0-join roaches, 1-move roaches, 2-join lizards, 3-move lizards */
    uint32_t id;
    char ch[10];
    int nChars; 
    direction_t direction[10];
}remote_char_t;

typedef struct ch_info_t
{
    int ch;
    int pos_x, pos_y;
} ch_info_t;

typedef struct pos_roaches
{
    uint32_t id;
    ch_info_t char_data[10];
    int nChars; 
    bool active[10];
} pos_roaches;

typedef struct pos_lizards
{
    double score;
    uint32_t id;
    ch_info_t char_data;
    direction_t prevdirection;
    bool valid;

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
    pos_lizards lizards[MAX_LIZARDS];
}msg;

#endif

