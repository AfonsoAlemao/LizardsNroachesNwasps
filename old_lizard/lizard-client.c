#include <ncurses.h>
#include "auxiliar.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <ctype.h>
#include <stdlib.h>
#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "../lizards.pb-c.h"

void *context;
void *requester;

/* Convert each string in a unique ID */
uint32_t hash_function(const char *str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash * 31) ^ (uint32_t)(*str++);
    }
    return hash;
}

/* Free allocated memory if an error occurs */
void free_exit_l() {
    zmq_close (requester);
    int rc = zmq_ctx_destroy (context);
    assert(rc == 0);
}

int zmq_read_OkMessage(void * requester){
    zmq_msg_t msg_raw;
    zmq_msg_init (&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    const uint8_t *pb_msg = (const uint8_t*)zmq_msg_data(&msg_raw);

    OkMessage  * ret_value =  
            ok_message__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close (&msg_raw); 
    return ret_value->msg_ok;
}

double zmq_read_Myscore(void * requester){
    
    zmq_msg_t msg_raw;
    zmq_msg_init (&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    const uint8_t *pb_msg = (const uint8_t*)zmq_msg_data(&msg_raw);

    ScoreMessage * ret_value =  
            score_message__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close (&msg_raw); 
    return ret_value->my_score;
}

void zmq_send_RemoteChar(void * requester, RemoteChar *element){

    RemoteChar m_struct = REMOTE_CHAR__INIT;
    m_struct.ch = malloc(strlen(element->ch) + 1); 
    memcpy(m_struct.ch, element->ch, strlen(element->ch) + 1);


    m_struct.msg_type = element->msg_type;
    m_struct.direction = element->direction;
    m_struct.nchars = element->nchars;
    m_struct.id = element->id;
    m_struct.n_direction = 10;

    
    int size_bin_msg = remote_char__get_packed_size(&m_struct);
    uint8_t * pb_m_bin = malloc(size_bin_msg);
    remote_char__pack(&m_struct, pb_m_bin);
    
    zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    //free(pb_m_bin);
    //free(pb_m_struct.ch.data);

}

int main(int argc, char *argv[]) {
    char *server_address, full_address[60], id_string[60], char_ok;
    int port, ok = 0, n = 0, disconnect = 0, key, rc;
    uint32_t id_int;
    RemoteChar m = REMOTE_CHAR__INIT;
    double my_score = 0;
    // size_t send, recv;

    /* Check if the correct number of arguments is provided */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <client-address> <client-port>\n", argv[0]);
        return 1;
    }

    /* Extract the server address and port number from the command line arguments */
    server_address = argv[1];
    port = atoi(argv[2]); /* Convert the port number from string to integer */

    /* Check if the port number is valid */
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        return 1;
    }    

    snprintf(id_string, sizeof(id_string), "%s:%s", server_address, argv[2]);
    id_int = hash_function(id_string);
    sprintf(full_address, "tcp://%s:%d", server_address, port);
 
    context = zmq_ctx_new();
    assert(context != NULL);
    requester = zmq_socket (context, ZMQ_REQ);
    assert(requester != NULL);
    rc = zmq_connect (requester, full_address);
    assert(rc == 0);

    /* Send connection message */
    m.msg_type = 2;
    m.nchars = 1;
    m.id = id_int;    
    m.ch = (char *) malloc(sizeof(char) * MAX_ROACHES_PER_CLIENT);
    m.direction = (Direction *) malloc(sizeof(Direction) * m.n_direction);

    /* Connection message */
    zmq_send_RemoteChar(requester, &m);
    // send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
    // assert(send != -1);
    // recv = zmq_recv (requester, &ok, sizeof(int), 0);
    // assert(recv != -1);
    ok = zmq_read_OkMessage(requester);

    /* From server response check connection success */
    char_ok = (char) ok;
    if (char_ok == '?') {
        printf("Connection failed\n");
        free_exit_l();
        exit(0);
    }
    ok = 0;

    /* Assign character to lizard */
    m.ch[0] = char_ok;
    
	initscr();			    /* Start curses mode */
	cbreak();				/* Line buffering disabled */
	keypad(stdscr, true);	/* We get F1, F2 etc.. */
	noecho();			    /* Don't echo() while we do getch */

    mvprintw(2, 0, "You are lizard %c", char_ok);
    
    /* Lizard movement message type */
    m.msg_type = 3;

    do
    {
        /* Get next movement from user */
    	key = getch();		
        n++;
        switch (key)
        {
        case KEY_LEFT:
            mvprintw(0,0,"%d :Left arrow is pressed ", n);
            m.direction[0] = LEFT;
            break;
        case KEY_RIGHT:
            mvprintw(0,0,"%d :Right arrow is pressed", n);
            m.direction[0] = RIGHT;
            break;
        case KEY_DOWN:
            mvprintw(0,0,"%d :Down arrow is pressed ", n);
           m.direction[0] = DOWN;
            break;
        case KEY_UP:
            mvprintw(0,0,"%d :Up arrow is pressed   ", n);
            m.direction[0] = UP;
            break;
        case 'q':
        case 'Q':
            mvprintw(0,0,"Disconnect                 ");
            disconnect = 1;
            break;
        default:
            key = 'x'; 
            break;
        }

        /* Send the movement message */
        if (disconnect == 1) {
            /* Message type for user disconnection */
            m.msg_type = 4;
        }
        if (key != 'x') {
            /* Send movement to server */
            
            mvprintw(2, 0, "aqui1\n");
            zmq_send_RemoteChar(requester, &m);
            // send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
            // assert(send != -1);
            // recv = zmq_recv (requester, &my_score, sizeof(int), 0);
            // assert(recv != -1);
            my_score = zmq_read_Myscore(requester);

            if (my_score == -1) { /* The request was not fullfilled */
                mvprintw(4, 0, "Connection failed!\t\t\t\n");
                free_exit_l();
                exit(0);
            }
            else if (my_score == -1000) { /* The request was not fullfilled */
                mvprintw(4, 0, "You have lost!\t\t\t\n");
                // free_exit_l();
                // exit(0);
            }
            else {
                /* From server response, update user score */
                mvprintw(4, 0, "Your score is %lf", my_score);
            }

            
        }
        refresh(); /* Print it on to the real screen */
    } while(key != 27 && key != 'Q' && key != 'q');
    
  	endwin(); /* End curses mode */
    free_exit_l();

	return 0;
}