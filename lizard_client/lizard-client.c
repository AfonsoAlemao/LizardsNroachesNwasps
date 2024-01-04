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
#include "z_helpers2.h"
#include <termios.h>
#include <unistd.h>

void *context;
void *requester;

WINDOW *my_win;
WINDOW *debug_win;
WINDOW *stats_win;
char *password;
void *context;
void *subscriber;

void *free_safe_d (void *aux) {
    if (aux != NULL) {
        free(aux);
    }
    return NULL;
}

/* Display each player (lizard) name and score */
void display_stats(pos_lizards *client_lizards) {
    int i = 0;

    for (int j = 0; j < MAX_LIZARDS; j++) {
        mvwprintw(stats_win, j, 0, "\t\t\t\t\t");
        wrefresh(stats_win);
        if (client_lizards[j].valid) {
            if (client_lizards[j].alive) {
                mvwprintw(stats_win, i, 1, "Player %c: Score = %lf", client_lizards[j].char_data.ch, client_lizards[j].score);
                wrefresh(stats_win);
            }
            else {
                mvwprintw(stats_win, i, 1, "Player %c: Lost!", client_lizards[j].char_data.ch);
                wrefresh(stats_win);
            }
            i++;
        }
    }
}

/* Free allocated memory if an error occurs */
void free_exit_display() {
    zmq_close (subscriber);
    int rc = zmq_ctx_destroy(context);
    assert(rc == 0);
    free_safe_d(password);
    delwin(stats_win);
    delwin(debug_win);
    delwin(my_win);
  	endwin(); /* End curses mode */
}

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
    char *client_address, full_address_client[60], full_address_display[60];
    char id_string_client[60], id_string_display[60], char_ok;
    int port_client, port_display, ok = 0, n = 0, disconnect = 0, key, rc;
    uint32_t id_int;
    remote_char_t m;
    double my_score = 0;
    size_t send, recv;

    msg msg_subscriber;
    int new = 0, j = 0, ch, i = 0;
    struct termios oldt, newt;
    size_t bufsize = 100, rcv;
    char *type;

    /* Check if the correct number of arguments is provided */
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client-address> <client-port_client> <display-port_client>\n", argv[0]);
        return 1;
    }

    /* Extract the server address and port_client number from the command line arguments */
    client_address = argv[1];
    port_client = atoi(argv[2]); /* Convert the client port number from string to integer */
    port_display = atoi(argv[3]); /* Convert the display port number from string to integer */

    /* Check if the port_client number is valid */
    if (port_client <= 0 || port_client > 65535) {
        fprintf(stderr, "Invalid port_client number. Please provide a number between 1 and 65535.\n");
        return 1;
    }    

    /* Check if the port_display number is valid */
    if (port_display <= 0 || port_display > 65535) {
        fprintf(stderr, "Invalid port_display number. Please provide a number between 1 and 65535.\n");
        return 1;
    }

    snprintf(id_string_client, sizeof(id_string_client), "%s:%s", client_address, argv[2]);
    id_int = hash_function(id_string_client);
    sprintf(full_address_client, "tcp://%s:%d", client_address, port_client);

    snprintf(id_string_client, sizeof(id_string_display), "%s:%s", client_address, argv[3]);
    sprintf(full_address_display, "tcp://%s:%d", client_address, port_display);
 
    context = zmq_ctx_new();
    assert(context != NULL);
    requester = zmq_socket (context, ZMQ_REQ);
    assert(requester != NULL);
    rc = zmq_connect (requester, full_address_client);
    assert(rc == 0);


    subscriber = zmq_socket (context, ZMQ_SUB);
    assert(subscriber != NULL);
    zmq_connect (subscriber, full_address_display);
    assert(subscriber != NULL);

    password = NULL;

    /* Allocate memory for the password */
    password = (char *) malloc(bufsize * sizeof(char));
    if (password == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        free_exit_display();
        exit(0);
    }

    
    
	initscr();			    /* Start curses mode */
	cbreak();				/* Line buffering disabled */
	keypad(stdscr, true);	/* We get F1, F2 etc.. */
	noecho();			    /* Don't echo() while we do getch */

    // mvprintw(2, 0, "You are lizard %c", char_ok);
    
    /* Lizard movement message type */
    m.msg_type = 3;

    /* Turn off echoing of characters */
    tcgetattr(STDIN_FILENO, &oldt); /* Get current terminal attributes */
    newt = oldt;
    newt.c_lflag &= ~(ECHO); /* Turn off ECHO */
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    /* Read password */
    printw("Enter password: ");
    refresh();

    i = 0;
    while(i < 99 && (ch = getch()) != '\n') {
        password[i++] = ch;
    }
    password[i] = '\0'; /* Null-terminate the string */

    /* Restore terminal settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    rc = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, password, strlen(password));		    
    assert(rc == 0);

    /* Creates a window and draws a border */
    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);

    /* Creates a window for stats */
    stats_win = newwin(MAX_LIZARDS + 1, 100, WINDOW_SIZE + 1, 0);

    /* Creates a window for debug */
    debug_win = newwin(20, 100, WINDOW_SIZE + 1 + MAX_LIZARDS + 1, 0);

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

    do {
        /* Receives message from publisher if the subscriber password matches the topic published */
        type = s_recv (subscriber);
        assert(type != NULL);
        if (strcmp(type, password) != 0 ) {
            printf("Wrong password\n");
            free_exit_display();
            exit(0);
        }
        rcv = zmq_recv (subscriber, &msg_subscriber, sizeof(msg), 0);
        assert(rcv != -1);

        /* When display-app receives data for the first time, it must display 
        the whole field game and update the lizard stats */
        if (new == 0) {
            new = 1;
            for (i = 0; i < WINDOW_SIZE; i++) {
                for (j = 0; j < WINDOW_SIZE; j++) {
                    ch = msg_subscriber.field[i][j];
                    if (ch != ' ') {
                        wmove(my_win, i, j);
                        waddch(my_win, ch | A_BOLD);
                        wrefresh(my_win); 
                    }
                }
            }
            display_stats(msg_subscriber.lizards);
        } else {
            /* Then, each time the field is updated, the display-app is updated accordingly */
            if (msg_subscriber.x_upd != -1 && msg_subscriber.y_upd != -1) {
                ch = msg_subscriber.field[msg_subscriber.x_upd][msg_subscriber.y_upd];
                wmove(my_win, msg_subscriber.x_upd, msg_subscriber.y_upd);
                waddch(my_win, ch | A_BOLD);
                wrefresh(my_win);
            }
            display_stats(msg_subscriber.lizards);
        }
        free_safe_d(type);

        /* Get next movement from user */
    	key = getch();		
        n++;
        switch (key)
        {
        case KEY_LEFT:
            // mvprintw(0,0,"%d :Left arrow is pressed ", n);
            m.direction[0] = LEFT;
            break;
        case KEY_RIGHT:
            // mvprintw(0,0,"%d :Right arrow is pressed", n);
            m.direction[0] = RIGHT;
            break;
        case KEY_DOWN:
            // mvprintw(0,0,"%d :Down arrow is pressed ", n);
           m.direction[0] = DOWN;
            break;
        case KEY_UP:
            // mvprintw(0,0,"%d :Up arrow is pressed   ", n);
            m.direction[0] = UP;
            break;
        case 'q':
        case 'Q':
            // mvprintw(0,0,"Disconnect                 ");
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
                // mvprintw(4, 0, "Connection failed!\t\t\t\n");
                free_exit_l();
                exit(0);
            }
            // else if (my_score == -1000) { /* The request was not fullfilled */
            //     mvprintw(4, 0, "You have lost!\t\t\t\n");
            //     free_exit_l();
            //     exit(0);
            // }
            // else {
            //     /* From server response, update user score */
            //     mvprintw(4, 0, "Your score is %lf", my_score);
            // }

            
        }
        refresh(); /* Print it on to the real screen */
    } while(key != 27 && key != 'Q' && key != 'q');
    
  	endwin(); /* End curses mode */
    free_exit_l();
    free_exit_display();

	return 0;
}