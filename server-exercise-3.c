
#include <ncurses.h>
#include "remote-char.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  
#include <stdlib.h>
#include <zmq.h> 
#include <string.h>
#include <stdio.h>
#include <assert.h> 

#define WINDOW_SIZE 15 

// STEP 1


direction_t random_direction(){
    return  random()%4;

}
    void new_position(int* x, int *y, direction_t direction){
        switch (direction)
        {
        case UP:
            (*x) --;
            if(*x ==0)
                *x = 2;
            break;
        case DOWN:
            (*x) ++;
            if(*x ==WINDOW_SIZE-1)
                *x = WINDOW_SIZE-3;
            break;
        case LEFT:
            (*y) --;
            if(*y ==0)
                *y = 2;
            break;
        case RIGHT:
            (*y) ++;
            if(*y ==WINDOW_SIZE-1)
                *y = WINDOW_SIZE-3;
            break;
        default:
            break;
        }
}

int find_ch_info(ch_info_t char_data[], int n_char, int ch){

    for (int i = 0 ; i < n_char; i++){
        if(ch == char_data[i].ch){
            return i;
        }
    }
    return -1;
}

int main()
{	
       
    //STEP 2
    remote_display_msg msg_publisher;
    remote_char_t m;

    void *context = zmq_ctx_new ();
    assert(context != NULL);
    void *responder = zmq_socket (context, ZMQ_REP);
    assert(responder != NULL);
    int rc = zmq_bind (responder, "tcp://127.0.0.1:5560");
    assert (rc == 0);

    void *publisher = zmq_socket (context, ZMQ_PUB);
    assert(publisher != NULL);
    int rc2 = zmq_bind (publisher, "tcp://127.0.0.1:5558");
    assert(rc2 == 0);

	initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();			    

    /* creates a window and draws a border */
    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);

    int ch;
    int pos_x;
    int pos_y;

    int ok = 1;
    char *position = "position";
    msg_publisher.n_chars = 0;


    direction_t  direction;
    while (1)
    {
        zmq_recv (responder, &m, sizeof(remote_char_t), 0);
        zmq_send (responder, &ok, sizeof(int), 0);

        msg_publisher.message_human_client = m;

        if(m.msg_type == 0){
            ch = m.ch;
            pos_x = WINDOW_SIZE/2;
            pos_y = WINDOW_SIZE/2;

            //STEP 3 
            msg_publisher.char_data[msg_publisher.n_chars].ch = ch;
            msg_publisher.char_data[msg_publisher.n_chars].pos_x = pos_x;
            msg_publisher.char_data[msg_publisher.n_chars].pos_y = pos_y;
            msg_publisher.n_chars++;
        }
        if(m.msg_type == 1){
            //STEP 4
            int ch_pos = find_ch_info(msg_publisher.char_data, msg_publisher.n_chars, m.ch);
            if(ch_pos != -1){
                pos_x = msg_publisher.char_data[ch_pos].pos_x;
                pos_y = msg_publisher.char_data[ch_pos].pos_y;
                ch = msg_publisher.char_data[ch_pos].ch;
                /*deletes old place */
                wmove(my_win, pos_x, pos_y);
                waddch(my_win,' ');

                /* claculates new direction */
                direction = m.direction;

                /* claculates new mark position */
                new_position(&pos_x, &pos_y, direction);
                msg_publisher.char_data[ch_pos].pos_x = pos_x;
                msg_publisher.char_data[ch_pos].pos_y = pos_y;

            } 
                  
        }
        
        zmq_send (publisher, position, strlen(position), ZMQ_SNDMORE);
        zmq_send (publisher, &msg_publisher, sizeof(remote_display_msg), 0);  

        /* draw mark on new position */
        wmove(my_win, pos_x, pos_y);
        waddch(my_win,ch| A_BOLD);
        wrefresh(my_win);		
    }
  	endwin();			/* End curses mode		  */
    zmq_close (responder);
    zmq_close (publisher);
    zmq_ctx_destroy (context);

	return 0;
}