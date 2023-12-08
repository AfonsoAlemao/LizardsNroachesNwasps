
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
    remote_display_msg msg_subscriber;
    ch_info_t char_data[100];
    int n_chars = 0;

    void *context = zmq_ctx_new ();
    assert(context != NULL);
    void *subscriber = zmq_socket (context, ZMQ_SUB);
    zmq_connect (subscriber, "tcp://127.0.0.1:5558");
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
    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);

    int pos_x;
    int pos_y;

    int new = 0; // see if user as just entered the game. 0 -> means it has just entered; 1 -> else

    direction_t  direction;
    while (1)
    {
        char *type = s_recv (subscriber);
        if(strcmp(type, password) != 0 ) {
            printf("Wrong password\n");
            exit(0);
        }
        zmq_recv (subscriber, &msg_subscriber, sizeof(remote_display_msg), 0);


        if(new == 0) {
            new = 1;

            for(int jj = 0; jj < msg_subscriber.n_chars; jj++) {

                n_chars++;
                char_data[jj] = msg_subscriber.char_data[jj];
                
                pos_x = char_data[jj].pos_x;
                pos_y = char_data[jj].pos_y;
                ch = char_data[jj].ch;

                /* draw mark on new position */
                wmove(my_win, pos_x, pos_y);
                waddch(my_win,ch| A_BOLD);
                wrefresh(my_win);
            }

        } else {

            if(msg_subscriber.message_human_client.msg_type == 0){
                ch = msg_subscriber.message_human_client.ch;
                pos_x = WINDOW_SIZE/2;
                pos_y = WINDOW_SIZE/2;

                //STEP 3
                char_data[n_chars].ch = ch;
                char_data[n_chars].pos_x = pos_x;
                char_data[n_chars].pos_y = pos_y;
                n_chars++;
            }
            if(msg_subscriber.message_human_client.msg_type == 1){
                //STEP 4
                int ch_pos = find_ch_info(char_data, n_chars, msg_subscriber.message_human_client.ch);
                if(ch_pos != -1){
                    pos_x = char_data[ch_pos].pos_x;
                    pos_y = char_data[ch_pos].pos_y;
                    ch = char_data[ch_pos].ch;
                    /*deletes old place */
                    wmove(my_win, pos_x, pos_y);
                    waddch(my_win,' ');

                    /* claculates new direction */
                    direction = msg_subscriber.message_human_client.direction;

                    /* claculates new mark position */
                    new_position(&pos_x, &pos_y, direction);
                    char_data[ch_pos].pos_x = pos_x;
                    char_data[ch_pos].pos_y = pos_y;

                }        
            }
            /* draw mark on new position */
            wmove(my_win, pos_x, pos_y);
            waddch(my_win,ch| A_BOLD);
            wrefresh(my_win);
        }

        free(type);		
    }
  	endwin();			/* End curses mode		  */
    zmq_close (subscriber);
    zmq_ctx_destroy (context);
    // Free the allocated memory
    free(password);

	return 0;
}