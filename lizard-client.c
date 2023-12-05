#include <ncurses.h>
#include "remote-char.h"
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

uint32_t hash_function(const char *str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash * 31) ^ (uint32_t)(*str++);
    }
    return hash;
}


int main(int argc, char *argv[]) {


    // Check if the correct number of arguments is provided
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-address> <port>\n", argv[0]);
        return 1;
    }

    // Extract the server address and port number from the command line arguments
    char *server_address = argv[1];
    int port = atoi(argv[2]); // Convert the port number from string to integer

    // Check if the port number is valid
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        return 1;
    }

    char full_address[60];
    char id_string[60];

    snprintf(id_string, sizeof(id_string), "%s:%s", server_address, argv[2]);
    uint32_t id_int = hash_function(id_string);

    printf("Unique ID: %u\n", id_int);

    sprintf(full_address, "tcp://%s:%d", server_address, port);
 
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    assert(requester != NULL);
    int rc = zmq_connect (requester, full_address);
    assert(rc == 0);

    // send connection message
    remote_char_t m;
    m.msg_type = 2;
    m.nChars = 1;
    int ok = 0;
    char char_ok;
    m.id = id_int;

    int my_score = 0;

    size_t send, recv;

    send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
    assert(send != -1);
    recv = zmq_recv (requester, &ok, sizeof(char), 0);
    assert(recv != -1);

    char_ok = (char) ok;
    
    if(char_ok == '?') {
        printf("Connection failed\n");
        exit(0);
    }

    ok = 0;
    m.ch[0] = char_ok;
    

	initscr();			    /* Start curses mode */
	cbreak();				/* Line buffering disabled */
	keypad(stdscr, TRUE);	/* We get F1, F2 etc.. */
	noecho();			    /* Don't echo() while we do getch */

    mvprintw(2, 0, "You are lizard %c", char_ok);
    
    int n = 0;

    // prepare the movement message
    m.msg_type = 3;
    int disconnect = 0;
    
    int key;
    do
    {
    	key = getch();		
        n++;
        switch (key)
        {
        case KEY_LEFT:
            mvprintw(0,0,"%d Left arrow is pressed", n);
            // prepare the movement message
            m.direction[0] = LEFT;
            break;
        case KEY_RIGHT:
            mvprintw(0,0,"%d Right arrow is pressed", n);
            // prepare the movement message
            m.direction[0] = RIGHT;
            break;
        case KEY_DOWN:
            mvprintw(0,0,"%d Down arrow is pressed", n);
            // prepare the movement message
           m.direction[0] = DOWN;
            break;
        case KEY_UP:
            mvprintw(0,0,"%d :Up arrow is pressed", n);
            // prepare the movement message
            m.direction[0] = UP;
            break;
        case 'q':
        case 'Q':
            mvprintw(0,0,"Disconect");
            disconnect = 1;
            break;
        default:
            key = 'x'; 
            break;
        }

        //TODO_10
        //send the movement message
        if(disconnect == 1) {
            m.msg_type = 4;
        }
        if (key != 'x'){
            send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
            assert(send != -1);
            recv = zmq_recv (requester, &my_score, sizeof(int), 0);
            assert(recv != -1);

            if(my_score == -1) { //The request was not fullfilled
                exit(0);
            }

            mvprintw(4, 0, "Your score is %d", my_score);

        }

        refresh();			/* Print it on to the real screen */
    }while(key != 27 && key != 'Q' && key != 'q');
    
    
  	endwin();			    /* End curses mode */
    zmq_close (requester);
    zmq_ctx_destroy (context);

	return 0;
}