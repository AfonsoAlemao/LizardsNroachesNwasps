
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
#include "zhelpers.h"
#include <termios.h>
#include <unistd.h>



WINDOW *my_win;
WINDOW *debug_win;
WINDOW *stats_win;


int main()
{
    msg msg_subscriber;
    int new = 0, j = 0;

    void *context = zmq_ctx_new ();
    assert(context != NULL);
    void *subscriber = zmq_socket (context, ZMQ_SUB);
    zmq_connect (subscriber, "tcp://*:5560");
    assert(subscriber != NULL);
    
    struct termios oldt, newt;
    char *password = NULL;
    size_t bufsize = 100;
    int ch;

    initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();	

    // Allocate memory for the password
    password = (char *)malloc(bufsize * sizeof(char));
    if(password == NULL) {
        perror("Unable to allocate memory");
        exit(1);
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

    while (1)
    {
        char *type = s_recv (subscriber);
        if(strcmp(type, password) != 0 ) {
            printf("Wrong password\n");
            exit(0);
        }
        zmq_recv (subscriber, &msg_subscriber, sizeof(msg), 0);


        if(new == 0) {
            new = 1;
            
            for (i = 0; i < WINDOW_SIZE - 2; i++) {
                for(j = 0; j < WINDOW_SIZE - 2; j++) {
                    ch = msg_subscriber.field[i][j];
                    wmove(my_win, i, j);
                    waddch(my_win, ch | A_BOLD);
                    wrefresh(my_win);
                }
            }

        } else {
            if (msg_subscriber.x_upd != -1 && msg_subscriber.y_upd != -1) {
                ch = msg_subscriber.field[msg_subscriber.x_upd][msg_subscriber.y_upd];
                wmove(my_win, msg_subscriber.x_upd, msg_subscriber.y_upd);
                waddch(my_win, ch | A_BOLD);
                wrefresh(my_win);
            }
            
        }

        free_safe(type);		
    }
  	endwin();			/* End curses mode		  */
    zmq_close (subscriber);
    zmq_ctx_destroy (context);
    // Free the allocated memory
    free_safe(password);

	return 0;
}