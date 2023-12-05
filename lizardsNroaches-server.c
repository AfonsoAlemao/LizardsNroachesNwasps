
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
#include <math.h>

#define WINDOW_SIZE 30 

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
    case NONE:
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
       
    remote_char_t m;

    void *context = zmq_ctx_new ();
    assert(context != NULL);
    void *responder = zmq_socket (context, ZMQ_REP);
    assert(responder != NULL);
    int rc = zmq_bind (responder, "tcp://*:5560");
    assert (rc == 0);

	initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();			    

    /* creates a window and draws a border */
    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);

    int ch;
    pos_roaches *client_roaches;

    //in worst case scenario each client has one roach and there are (WINDOW_SIZE*WINDOW_SIZE)/3 roaches
    client_roaches = (pos_roaches*) calloc(floor((WINDOW_SIZE*WINDOW_SIZE)/3), sizeof(pos_roaches));

    int pos_x_roaches;
    int pos_y_roaches;

    int ok = 1;

    srand(time(NULL));

    int n_clients_roaches = 0;

    int total_roaches = 0;

    int i = 0;
    direction_t  direction;
    size_t send, recv;
    
    while (1)
    {
        recv = zmq_recv (responder, &m, sizeof(remote_char_t), 0);
        assert(recv != -1);
        send = zmq_send (responder, &ok, sizeof(int), 0);
        assert(send != -1);

        if(m.msg_type == 0){

            client_roaches[n_clients_roaches].id = m.id;
            client_roaches[n_clients_roaches].nChars = m.nChars;

            for(i = 0; i < m.nChars; i++) {
                client_roaches[n_clients_roaches].char_data[i].ch = m.ch[i];


                client_roaches[n_clients_roaches].char_data[i].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                client_roaches[n_clients_roaches].char_data[i].pos_y =  rand() % (WINDOW_SIZE - 2) + 1;

                pos_x_roaches = client_roaches[n_clients_roaches].char_data[i].pos_x;
                pos_y_roaches = client_roaches[n_clients_roaches].char_data[i].pos_y;
                ch = client_roaches[n_clients_roaches].char_data[i].ch;

                /* draw mark on new position */
                wmove(my_win, pos_x_roaches, pos_y_roaches);
                waddch(my_win,ch| A_BOLD);
                wrefresh(my_win);
            }

            n_clients_roaches++;

            total_roaches += m.nChars;

        }
        if(m.msg_type == 1){
            uint32_t index_client_roaches_id = 0;

            for(int jjj; jjj < n_clients_roaches;jjj++) {
                if(client_roaches[jjj].id == m.id) {
                    index_client_roaches_id = jjj;
                    break;
                }
            }

            for(i = 0; i < m.nChars; i++) {

                pos_x_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_x;
                pos_y_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_y;
                ch = client_roaches[index_client_roaches_id].char_data[i].ch;
                /*deletes old place */
                wmove(my_win, pos_x_roaches, pos_y_roaches);
                waddch(my_win,' ');

                /* calculates new mark position */
                new_position(&pos_x_roaches, &pos_y_roaches, m.direction[i]);
                client_roaches[index_client_roaches_id].char_data[i].pos_x = pos_x_roaches;
                client_roaches[index_client_roaches_id].char_data[i].pos_y = pos_y_roaches;

                /* draw mark on new position */
                wmove(my_win, pos_x_roaches, pos_y_roaches);
                waddch(my_win,ch| A_BOLD);
                wrefresh(my_win);	
            }
                
        }
        	
    }
  	endwin();			/* End curses mode		  */
    zmq_close (responder);
    zmq_ctx_destroy (context);

	return 0;
}