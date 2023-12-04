
typedef enum direction_t {UP, DOWN, LEFT, RIGHT} direction_t;

typedef struct remote_char_t
{   
    int msg_type; /* 0 join   1 - move */
    char ch; 
    direction_t direction ;
    /* data */
}remote_char_t;

#define FIFO_NAME "/tmp/fifo_snail"

typedef struct ch_info_t
{
    int ch;
    int pos_x, pos_y;
} ch_info_t;


typedef struct remote_display_msg
{   
    remote_char_t message_human_client;
    ch_info_t char_data[100];
    int n_chars;
    /* data */
}remote_display_msg;