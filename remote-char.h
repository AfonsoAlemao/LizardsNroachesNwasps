#include <stdint.h>
#include <time.h>
#include <stdbool.h>

typedef enum direction_t {UP, DOWN, LEFT, RIGHT, NONE} direction_t;

typedef struct remote_char_t
{   
    int msg_type; /* 0-join roaches, 1-move roaches, 2-join lizards, 3-move lizards*/
    uint32_t id;
    char ch[10];
    int nChars; 
    direction_t direction[10];
    /* data */
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
    ch_info_t char_data; //saves the head position
    direction_t prevdirection;
    bool valid;

} pos_lizards;


typedef struct remote_display_msg
{   
    remote_char_t message_human_client;
    ch_info_t char_data[100];
    int n_chars;
    /* data */
}remote_display_msg;


