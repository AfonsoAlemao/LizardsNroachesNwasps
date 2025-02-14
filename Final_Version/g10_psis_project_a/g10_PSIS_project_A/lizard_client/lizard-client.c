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

int main(int argc, char *argv[]) {
    char *server_address, full_address[60], id_string[60], char_ok;
    int port, ok = 0, n = 0, disconnect = 0, key, rc;
    uint32_t id_int;
    remote_char_t m;
    double my_score = 0;
    size_t send, recv;

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
    m.nChars = 1;
    m.id = id_int;    

    /* Connection message */
    send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
    assert(send != -1);
    recv = zmq_recv (requester, &ok, sizeof(char), 0);
    assert(recv != -1);

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
            send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
            assert(send != -1);
            recv = zmq_recv (requester, &my_score, sizeof(double), 0);
            assert(recv != -1);

            if (my_score == -1) { /* The request was not fullfilled */
                printf("Connection failed\n");
                free_exit_l();
                exit(0);
            }

            /* From server response, update user score */
            mvprintw(4, 0, "Your score is %lf", my_score);
        }
        refresh(); /* Print it on to the real screen */
    } while(key != 27 && key != 'Q' && key != 'q');
    
  	endwin(); /* End curses mode */
    free_exit_l();

	return 0;
}