
#include <ncurses.h>
#include "auxiliar.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  
#include <stdlib.h>
#include <zmq.h> 
#include <string.h>
#include <stdio.h>
#include <assert.h> 
#include "z_helpers2.h"
#include <termios.h>
#include <unistd.h>

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

int main(int argc, char *argv[]) {
    /* Check if the correct number of arguments is provided */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <display-address> <display-port>\n", argv[0]);
        return 1;
    }

    /* Extract the server address and port number from the command line arguments */
    char *server_address = argv[1];
    int port = atoi(argv[2]); /* Convert the port number from string to integer */

    /* Check if the port number is valid */
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        return 1;
    }

    char full_address[60], id_string[60];
    msg msg_subscriber;
    int new = 0, j = 0, ch, i = 0;
    struct termios oldt, newt;
    size_t bufsize = 100, rcv;
    char *type;

    snprintf(id_string, sizeof(id_string), "%s:%s", server_address, argv[2]);
    sprintf(full_address, "tcp://%s:%d", server_address, port);

    context = zmq_ctx_new ();
    assert(context != NULL);
    subscriber = zmq_socket (context, ZMQ_SUB);
    assert(subscriber != NULL);
    zmq_connect (subscriber, full_address);
    assert(subscriber != NULL);
    
    password = NULL;

    initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();	

    /* Allocate memory for the password */
    password = (char *) malloc(bufsize * sizeof(char));
    if (password == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        free_exit_display();
        exit(0);
    }

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

    int rc = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, password, strlen(password));		    
    assert(rc == 0);

    /* Creates a window and draws a border */
    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);

    /* Creates a window for stats */
    stats_win = newwin(MAX_LIZARDS + 1, 100, WINDOW_SIZE + 1, 0);

    /* Creates a window for debug */
    debug_win = newwin(20, 100, WINDOW_SIZE + 1 + MAX_LIZARDS + 1, 0);

    while (1) {
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
    }
    free_exit_display();

	return 0;
}