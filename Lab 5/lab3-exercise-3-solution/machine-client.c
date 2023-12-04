#include "remote-char.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <zmq.h> 
#include <string.h>
#include <stdio.h>
#include <assert.h> 


int main()
{	 

    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    assert(requester != NULL);
    int rc = zmq_connect (requester, "tcp://127.0.0.1:5560");
    assert(rc == 0);

    //TODO_5
    // read the character from the user
    char ch;
    do{
        printf("what is your symbol(a..z)?: ");
        ch = getchar();
        ch = tolower(ch);  
    }while(!isalpha(ch));

    // TODO_6
    // send connection message
    remote_char_t m;
    m.msg_type = 0;
    m.ch = ch;
    int ok = 0;
    
    zmq_send (requester, &m, sizeof(remote_char_t), 0);
    zmq_recv (requester, &ok, sizeof(int), 0);

    int sleep_delay;
    direction_t direction;
    int n = 0;
    
    while (1)
    {
        n++;
        sleep_delay = random()%700000;
        usleep(sleep_delay);
        direction = random()%4;
        switch (direction)
        {
        case LEFT:
           printf("%d Going Left   \n", n);
            break;
        case RIGHT:
            printf("%d Going Right   \n", n);
           break;
        case DOWN:
            printf("%d Going Down   \n", n);
            break;
        case UP:
            printf("%d Going Up    \n", n);
            break;
        }

        //TODO_9
        // prepare the movement message
        m.direction = direction;
        m.msg_type = 1;

        //TODO_10
        //send the movement message
        zmq_send (requester, &m, sizeof(remote_char_t), 0);
        zmq_recv (requester, &ok, sizeof(int), 0);
    }

    zmq_close (requester);
    zmq_ctx_destroy (context);
 
	return 0;
}