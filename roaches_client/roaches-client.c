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
#include "../lizards.pb-c.h"

void *context;
void *requester;
char *roaches;

int zmq_read_OkMessage(void * requester){
    zmq_msg_t msg_raw;
    zmq_msg_init (&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    const uint8_t *pb_msg = (const uint8_t*)zmq_msg_data(&msg_raw);

    OkMessage  * ret_value =  
            ok_message__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close (&msg_raw); 
    return ret_value->msg_ok;
}

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
    free_safe_r(roaches);
}

/* Convert each string in a unique ID */
uint32_t hash_function(const char *str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash * 31) ^ (uint32_t)(*str++);
    }
    return hash;
}



void zmq_send_RemoteChar(void * requester, RemoteChar *element){

    RemoteChar m_struct = REMOTE_CHAR__INIT;
    m_struct.ch = malloc(strlen(element->ch) + 1); 
    memcpy(m_struct.ch, element->ch, strlen(element->ch) + 1);
    m_struct.msg_type = element->msg_type;
    m_struct.direction = element->direction;
    m_struct.nchars = element->nchars;
    m_struct.id = element->id;
    m_struct.n_direction = 10;
    
    int size_bin_msg = remote_char__get_packed_size(&m_struct);
    uint8_t * pb_m_bin = malloc(size_bin_msg);
    remote_char__pack(&m_struct, pb_m_bin);
    
    zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    //free(pb_m_bin);
    //free(pb_m_struct.ch.data);

}

int main(int argc, char *argv[]) {
    int nRoaches, sleep_delay, ok = 0, port, rc, kk = 0;
    char full_address[60], id_string[60], *server_address, char_ok;
    // size_t send, recv;
    uint32_t id_int;
    RemoteChar m = REMOTE_CHAR__INIT;

    /* Check if the correct number of arguments is provided */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <client-address> <client-roach-wasp-port>\n", argv[0]);
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
        printf("How many roaches (1 to 10)? ");
        if (scanf("%d", &nRoaches) != 1 || getchar() != '\n') {
            fprintf(stderr, "Invalid input. Please enter a valid integer.\n");

            // Clear the input buffer
            while (getchar() != '\n');

            printf("How many roaches (1 to 10)? ");
        } else if (nRoaches < 1 || nRoaches > 10) {
            fprintf(stderr, "Invalid input. Please enter a number in the range 1 to 10.\n");
        } else {
            // Valid input
            break;
        }
    }

    /* Seed the random number generator */
    srand(time(NULL));

    roaches = (char *)calloc(nRoaches, sizeof(char));
    if (roaches == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        free_exit_r();
        exit(0);
    }
    
    /* Send connection message */
    m.msg_type = 0;
    m.nchars = nRoaches;
    m.id = id_int;
    m.ch = (char *) malloc(sizeof(char) * MAX_ROACHES_PER_CLIENT);
    m.direction = (Direction *) malloc(sizeof(Direction) * m.n_direction);

    /* Assign random characters between '1' and '5' to each element in the array */
    for (int i = 0; i < nRoaches; i++) {
        int randomNum = (rand() % 5) + 1;
        roaches[i] = '0' + randomNum;
        m.ch[i] = roaches[i];
    }
    

    /* Connection message */
    zmq_send_RemoteChar(requester, &m);
    ok = zmq_read_OkMessage(requester);

    /* From server response check connection success */
    char_ok = (char) ok;
    if (char_ok == '?') {
        printf("Connection failed\n");
        free_exit_r();
        exit(0);
    }
    
    /* Request was not fullfilled when number of roaches in the 
    server does not allow the addition of these number of roaches */
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
        
        /* For each roach, choose next direction (including a non-movement) randomly */
        for (kk = 0; kk < nRoaches; kk++) {
          m.direction[kk] = random() % 5;  
        }

        /* Roaches movement message type */
        m.msg_type = 1;

        /* Send the movement message */
        zmq_send_RemoteChar(requester, &m);
        ok = zmq_read_OkMessage(requester);
    
        /* Check if roach movement request failed */
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