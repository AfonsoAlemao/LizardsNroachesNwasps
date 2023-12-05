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
} pos_roaches;

typedef struct pos_lizards
{
    int score;
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


typedef struct list_element 
{
    int index_client;  // index in table of clients with roaches or in table of lizards 
    int index_roaches;  // index in table of the client of the roaches. Only used for roaches
    int element_type;   // 0-tail of lizz, 1-liz head, 2-roach

    /* Examples:
    lizard stored in lizardtable[0]: 
        index_client=0
        index_roaches=don't care
        element_type=1

    tail of lizard stored in lizardtable[0]: 
        index_client=0
        index_roaches=don't care
        element_type=0

    roach stored in roachtable[0][3]: 
        index_client=0
        index_roaches=3
        element_type=2
    */
}list_element;