
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
#include "zhelpers.h"
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

void display_stats(pos_lizards *client_lizards) {
    int i = 0;

    for (int j = 0; j < MAX_LIZARDS; j++) {
        mvwprintw(stats_win, j, 0, "\t\t\t\t\t");
        wrefresh(stats_win);
        if (client_lizards[j].valid) {
            mvwprintw(stats_win, i, 0, "Player: %c, Score: %lf", client_lizards[j].char_data.ch, client_lizards[j].score);
            wrefresh(stats_win);
            i++;
        }
    }
}

void free_exit_display() {
    zmq_close (subscriber);
    zmq_ctx_destroy (context);
    free_safe_d(password);
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

    sprintf(full_address, "tcp://%s:%d", server_address, port);


    msg msg_subscriber;
    int new = 0, j = 0;

    context = zmq_ctx_new ();
    assert(context != NULL);
    subscriber = zmq_socket (context, ZMQ_SUB);
    zmq_connect (subscriber, full_address);
    assert(subscriber != NULL);
    
    struct termios oldt, newt;
    password = NULL;
    size_t bufsize = 100;
    int ch;

    initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();	

    // Allocate memory for the password
    password = (char *)malloc(bufsize * sizeof(char));
    if(password == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        free_exit_display();
        exit(0);
    }

    // Turn off echoing of characters
    tcgetattr(STDIN_FILENO, &oldt); // get current terminal attributes
    newt = oldt;
    newt.c_lflag &= ~(ECHO); // turn off ECHO
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Read password
    printw("Enter password: ");
    refresh();

    int i = 0;
    while(i < 99 && (ch = getch()) != '\n') {
        password[i++] = ch;
        // addch('*'); // Display an asterisk for each character
    }
    password[i] = '\0'; // Null-terminate the string

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, password, strlen(password));		    

    /* creates a window and draws a border */
    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);

    // creates a window for stats
    stats_win = newwin(MAX_LIZARDS + 1, 100, WINDOW_SIZE + 1, 0);

    // creates a window for debug
    debug_win = newwin(20, 100, WINDOW_SIZE + 1 + MAX_LIZARDS + 1, 0);

    // mvwprintw(stats_win, 1, 1, "pass: %s", password);
    // wrefresh(stats_win);

    while (1) {
        char *type = s_recv (subscriber);
        if(strcmp(type, password) != 0 ) {
            printf("Wrong password\n");
            free_exit_display();
            exit(0);
        }

        zmq_recv (subscriber, &msg_subscriber, sizeof(msg), 0);

        // mvwprintw(stats_win, 5, 0, "x=%d, y=%d", msg_subscriber.x_upd, msg_subscriber.y_upd);
        // wrefresh(stats_win);


        if(new == 0) {
            new = 1;
            
            for (i = 0; i < WINDOW_SIZE; i++) {
                for(j = 0; j < WINDOW_SIZE; j++) {
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
  	endwin();			/* End curses mode		  */
    free_exit_display();

	return 0;
}