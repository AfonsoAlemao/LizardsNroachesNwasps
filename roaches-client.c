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

    int nRoaches;
    // Ask the user for the size of the array
    printf("How many roaches (1 to 10)? ");
    scanf("%d", &nRoaches);
    
    // Validate the input
    while (nRoaches < 1 || nRoaches > 10) {
        printf("\nInvalid input. Please enter a number in the range 1 to 10: ");
        scanf("%d", &nRoaches);
    }

    // Seed the random number generator
    srand(time(NULL));

    // Create a character array of size n
    char *roaches;

    roaches = (char *)calloc(nRoaches, sizeof(char));
    
    
    // TODO_6
    // send connection message
    remote_char_t m;
    m.msg_type = 0;
    m.nChars = nRoaches;
    m.id = id_int;

    // Assign random characters between '1' and '5' to each element in the array
    for (int i = 0; i < nRoaches; i++) {
        int randomNum = (rand() % 5) + 1; // Generates a random number between 1 and 5
        roaches[i] = '0' + randomNum; // Convert number to character
        m.ch[i] = roaches[i];
    }

    int ok = 0;

    size_t send, recv;        

    // connection message
    send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
    assert(send != -1);
    recv = zmq_recv (requester, &ok, sizeof(int), 0);
    assert(recv != -1);
    
    /* for example when number of roaches in the 
    server does not allow the addition of these number of roaches */
    
    if(ok == 0) { 
        printf("The request was not fullfilled\n");
        free(roaches);
        exit(0);
    }

    ok = 0;

    int sleep_delay;
    int kk = 0;

    while (1) 
    {
        sleep_delay = random()%1000000;
        usleep(sleep_delay);
        
        for (kk = 0; kk < nRoaches; kk++){
          m.direction[kk] = random()%5;  
        }

        m.msg_type = 1;

        //TODO_10
        //send the movement message
        send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
        assert(send != -1);
        recv = zmq_recv (requester, &ok, sizeof(int), 0);
        assert(recv != -1);
    
        /* for example when number of roaches in the 
        server does not allow the addition of these number of roaches */

        if(ok == 0) { 
            printf("The request was not fullfilled\n");
            free(roaches);
            exit(0);
        }

        ok = 0;

    }

    zmq_close (requester);
    zmq_ctx_destroy (context);
    free(roaches);
 
	return 0;
}