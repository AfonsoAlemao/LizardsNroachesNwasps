#include "auxiliar.h"
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

void *context;
void *requester;
char *wasps;

void *free_safe_r (void *aux) {
    if (aux != NULL) {
        free(aux);
    }
    return NULL;
}

/* Free allocated memory if an error occurs */
void free_exit_r() {
    zmq_close (requester);
    int rc = zmq_ctx_destroy (context);
    assert(rc == 0);
    free_safe_r(wasps);
}

/* Convert each string in a unique ID */
uint32_t hash_function(const char *str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash * 31) ^ (uint32_t)(*str++);
    }
    return hash;
}

int main(int argc, char *argv[]) {
    int nwasps, sleep_delay, ok = 0, port, rc, kk = 0;
    char full_address[60], id_string[60], *server_address, char_ok;
    size_t send, recv;
    uint32_t id_int;
    remote_char_t m;

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

    while (1) {
        printf("How many wasps (1 to 10)? ");
        if (scanf("%d", &nwasps) != 1 || getchar() != '\n') {
            fprintf(stderr, "Invalid input. Please enter a valid integer.\n");

            // Clear the input buffer
            while (getchar() != '\n');

            printf("How many wasps (1 to 10)? ");
        } else if (nwasps < 1 || nwasps > 10) {
            fprintf(stderr, "Invalid input. Please enter a number in the range 1 to 10.\n");
        } else {
            // Valid input
            break;
        }
    }

    /* Seed the random number generator */
    srand(time(NULL));

    wasps = (char *)calloc(nwasps, sizeof(char));
    if (wasps == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        free_exit_r();
        exit(0);
    }
    
    /* Send connection message */
    m.msg_type = 6;
    m.nChars = nwasps;
    m.id = id_int;

    /* Assign '#' to each element in the array */
    for (int i = 0; i < nwasps; i++) {
        m.ch[i] = '#';
    }

    /* Connection message */
    send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
    assert(send != -1);
    recv = zmq_recv (requester, &ok, sizeof(int), 0);
    assert(recv != -1);

    /* From server response check connection success */
    char_ok = (char) ok;
    if (char_ok == '?') {
        printf("Connection failed\n");
        free_exit_r();
        exit(0);
    }
    
    /* Request was not fullfilled when number of wasps in the 
    server does not allow the addition of these number of wasps */
    if (ok == 0) { 
        printf("The request was not fullfilled\n");
        free_exit_r();
        exit(0);
    }
    ok = 0;
    
    while (1) 
    {
        sleep_delay = random()%1000000;
        usleep(sleep_delay);
        
        /* For each wasp, choose next direction (including a non-movement) randomly */
        for (kk = 0; kk < nwasps; kk++) {
          m.direction[kk] = random() % 5;  
        }

        /* wasps movement message type */
        m.msg_type = 7;

        /* Send the movement message */
        send = zmq_send (requester, &m, sizeof(remote_char_t), 0);
        assert(send != -1);
        recv = zmq_recv (requester, &ok, sizeof(int), 0);
        assert(recv != -1);
    
        /* Check if wasp movement request failed */
        if (ok == 0) { 
            printf("The request was not fullfilled\n");
            free_exit_r();
            exit(0);
        }
        ok = 0;

    }
    free_exit_r();
 
	return 0;
}