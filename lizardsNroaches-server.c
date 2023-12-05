
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

#define WINDOW_SIZE 20 
#define MAX_LIZARDS 26 
#define TAIL_SIZE 5

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

void display_in_field(WINDOW *my_win, char ch, int x, int y) {
    wmove(my_win, x, y);
    waddch(my_win, ch | A_BOLD);
    wrefresh(my_win);
}

void tail(direction_t direction, int x, int y, bool delete, WINDOW *my_win) {
    char display;
    if (delete == TRUE) {
        display = ' ';
    }
    else {
        display = '.';
    }
    
    
    switch (direction)
    {
    case LEFT:
        for(int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (y + kk < WINDOW_SIZE - 1) {
                display_in_field(my_win, display, x, y + kk);
            }
        }
        break;
    case RIGHT:
        for(int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (y - kk > 1) {
                display_in_field(my_win, display, x, y - kk);
            }
        }
        
        break;
    case DOWN:
        for(int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (x - kk > 1) {
                display_in_field(my_win, display, x - kk, y);
            }
        }
        
        break;
    case UP:
        for(int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (x + kk < WINDOW_SIZE - 1) {
                display_in_field(my_win, display, x + kk, y);
            }
        }
        break;
    default:
        break;
    }
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

    int total_lizards = 0; 

    pos_lizards client_lizards[MAX_LIZARDS];

    int max_roaches = floor(((WINDOW_SIZE-2)*(WINDOW_SIZE - 2))/3);

    //in worst case scenario each client has one roach and there are (WINDOW_SIZE*WINDOW_SIZE)/3 roaches
    client_roaches = (pos_roaches*) calloc(max_roaches, sizeof(pos_roaches));

    int pos_x_roaches, pos_x_lizards, pos_y_roaches, pos_y_lizards;

    int ok = 1;

    srand(time(NULL));

    int n_clients_roaches = 0;

    int total_roaches = 0;

    int i = 0;
    size_t send, recv;
    
    while (1)
    {
        recv = zmq_recv (responder, &m, sizeof(remote_char_t), 0);
        assert(recv != -1);
        if(m.nChars + total_roaches > max_roaches && m.msg_type == 0) {
            ok = 0;
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;
            continue;
        } else if(m.msg_type == 2) {
            if(total_lizards + 1 > MAX_LIZARDS) {
                ok = (int) '?'; //in case the pool is full of lizards
            } else {
                ok = (int) 'a' + total_lizards;
            }
            
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;
        } else if (m.msg_type != 3) {
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
        }
        

        ok = 1;

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
                display_in_field(my_win, ch, pos_x_roaches, pos_y_roaches);
            }

            n_clients_roaches++;

            total_roaches += m.nChars;

        } else if(m.msg_type == 1){
            uint32_t index_client_roaches_id = 0;

            for(int jjj = 0; jjj < n_clients_roaches;jjj++) {
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
                display_in_field(my_win, ' ', pos_x_roaches, pos_y_roaches);

                /* calculates new mark position */
                new_position(&pos_x_roaches, &pos_y_roaches, m.direction[i]);
                client_roaches[index_client_roaches_id].char_data[i].pos_x = pos_x_roaches;
                client_roaches[index_client_roaches_id].char_data[i].pos_y = pos_y_roaches;

                /* draw mark on new position */
                display_in_field(my_win, ch, pos_x_roaches, pos_y_roaches);
            }
                
        } else if(m.msg_type == 2){
            // TODO: em vez de acrescentar na posiçao (total_lizards), acrescentar na primeira posiçao com .valid=False
            client_lizards[total_lizards].id = m.id;
            client_lizards[total_lizards].score = 0;
            client_lizards[total_lizards].valid = TRUE;

            client_lizards[total_lizards].char_data.pos_x =  rand() % (WINDOW_SIZE - 2) + 1;
            client_lizards[total_lizards].char_data.pos_y =  rand() % (WINDOW_SIZE - 2) + 1;
            client_lizards[total_lizards].char_data.ch = (char) ((int) 'a' + total_lizards);

            pos_x_lizards = client_lizards[total_lizards].char_data.pos_x;
            pos_y_lizards = client_lizards[total_lizards].char_data.pos_y;
            ch = client_lizards[total_lizards].char_data.ch;

            
            m.direction[0] = rand() % 4;
            
            tail(m.direction[0], pos_x_lizards, pos_y_lizards, FALSE, my_win);

            client_lizards[total_lizards].prevdirection = m.direction[0];

            /* draw mark on new position */
            display_in_field(my_win, ch, pos_x_lizards, pos_y_lizards);

            total_lizards++;

        } else if(m.msg_type == 3){
            uint32_t index_client_lizards_id = 0;

            for(int jjj = 0; jjj < total_lizards;jjj++) {
                if(client_lizards[jjj].id == m.id) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }

            pos_x_lizards = client_lizards[index_client_lizards_id].char_data.pos_x;
            pos_y_lizards = client_lizards[index_client_lizards_id].char_data.pos_y;
            ch = client_lizards[index_client_lizards_id].char_data.ch;
            

            // delete old tail
            tail(client_lizards[index_client_lizards_id].prevdirection, pos_x_lizards, pos_y_lizards, TRUE, my_win);

            /*deletes old place */
            display_in_field(my_win, ' ', pos_x_lizards, pos_y_lizards);


            /* calculates new mark position */
            new_position(&pos_x_lizards, &pos_y_lizards, m.direction[0]);
            client_lizards[index_client_lizards_id].char_data.pos_x = pos_x_lizards;
            client_lizards[index_client_lizards_id].char_data.pos_y = pos_y_lizards;

            /* draw mark on new position */
            display_in_field(my_win, ch, pos_x_lizards, pos_y_lizards);

            send = zmq_send (responder, &client_lizards[index_client_lizards_id].score, sizeof(int), 0);
            assert(send != -1);

            tail(m.direction[0], pos_x_lizards, pos_y_lizards, FALSE, my_win);

            client_lizards[index_client_lizards_id].prevdirection = m.direction[0];
                
        } else if(m.msg_type == 4){
            uint32_t index_client_lizards_id = 0;

            for(int jjj = 0; jjj < total_lizards;jjj++) {
                if(client_lizards[jjj].id == m.id) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }

            pos_x_lizards = client_lizards[index_client_lizards_id].char_data.pos_x;
            pos_y_lizards = client_lizards[index_client_lizards_id].char_data.pos_y;
            ch = client_lizards[index_client_lizards_id].char_data.ch;
            /*deletes old place */
            display_in_field(my_win, ' ', pos_x_lizards, pos_y_lizards);

            client_lizards[total_lizards].valid = FALSE;
        }
        	
    }
  	endwin();			/* End curses mode		  */
    zmq_close (responder);
    zmq_ctx_destroy (context);
    free(client_roaches);

	return 0;
}