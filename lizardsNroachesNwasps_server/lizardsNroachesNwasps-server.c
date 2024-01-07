#include <ncurses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  
#include <stdlib.h>
#include <zmq.h> 
#include <string.h>
#include <stdio.h>
#include <assert.h> 
#include <math.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

#include "lists.h"
#include "fifo.h"
#include "auxiliar.h"
#include "z_helpers.h"
#include "../lizards.pb-c.h"
#include <pthread.h>

WINDOW *my_win;
WINDOW *debug_win;
WINDOW *stats_win;
list_element ***field;
fifo_element *roaches_killed;
pos_roachesNwasps *client_roaches;
pos_roachesNwasps *client_wasps;
pos_lizards client_lizards[MAX_LIZARDS];
int max_bots;
// void *responder2;


#define MAX_PORT_STR_LEN 6 /* Max port number "65535" plus null terminator */
#define ADDRESS_PREFIX_LEN 10 // Length of "tcp://*:" including null terminator
#define FULL_ADDRESS_LEN (MAX_PORT_STR_LEN + ADDRESS_PREFIX_LEN)
#define N_THREADS 8

int end_game;
int total_lizards;
void *context;
void *publisher, *responder;

msg msg_publisher;
char *password = NULL;
int n_clients_roaches, n_clients_wasps, total_roaches, total_lizards, total_wasps;
pthread_mutex_t mtx_roaches, mtx_wasps, mtx_lizards, mtx_field, mtx_publish;
int s_roaches, s_wasps, s_lizards, s_field, s_publish;
char full_address_display[FULL_ADDRESS_LEN], full_address_client_rNw[FULL_ADDRESS_LEN], full_address_client_lizard[FULL_ADDRESS_LEN];
void *backend, *frontend;


void new_position(int* x, int *y, direction_t direction);
void split_health(list_element *head, int index_client);
void search_and_destroy_roaches(list_element *head, int index_client);
list_element *display_in_field(char ch, int x, int y, int index_client, int index_bot, int element_type, list_element *head, void *publisher);
void tail(direction_t direction, int x, int y, bool delete, int index_client, void *publisher);
int check_in_square(list_element *head, int type);
char check_prioritary_element(list_element *head);
void *free_safe (void *aux);
void loser_lizard(int index, void *publisher);
list_element ***allocate3DArray();
void free3DArray(list_element ***table);
void ressurect_roaches();
void free_exit();
bool disconnect_wasp(int index, void *publisher);
bool disconnect_roach(int index, void *publisher);
bool disconnect_lizard(int index, void *publisher);

void errExitEN(int s, const char *msg) {
    
    fprintf(stderr, "%s error: %d\n", msg, s);
    exit(EXIT_FAILURE);
}

/* Apply movement to get new position */
void new_position(int *x, int *y, direction_t direction) {
    switch (direction) {
        case UP:
            (*x) --;
            if (*x == 0)
                *x = 2;
            break;
        case DOWN:
            (*x) ++;
            if (*x == WINDOW_SIZE - 1)
                /* Ricochet of the wall */
                *x = WINDOW_SIZE - 3; 
            break;
        case LEFT:
            (*y) --;
            if (*y ==0)
                *y = 2;
            break;
        case RIGHT:
            (*y) ++;
            if (*y == WINDOW_SIZE - 1)
                /* Ricochet of the wall */
                *y = WINDOW_SIZE - 3;
            break;
        case NONE:
        default:
            break;
    }
}

/* Split health between 2 lizards whose heads collided */
void split_health(list_element *head, int index_client) {
    list_element *current = head;
    list_element *nextNode;
    double avg = 0;

    avg = client_lizards[index_client].score;

    while (current != NULL) {
        if (current->data.element_type == 1) { /* Head of lizard */
            avg += client_lizards[current->data.index_client].score;
            break;
        }
        nextNode = current->next;
        current = nextNode;
    }
    
    avg = avg / 2;

    s_lizards = pthread_mutex_lock(&mtx_lizards);
    if (s_lizards != 0) {
        errExitEN(s_lizards, "pthread_mutex_lock");
    }

    client_lizards[index_client].score = avg;
    client_lizards[current->data.index_client].score = avg;
    
    s_lizards = pthread_mutex_unlock(&mtx_lizards);
    if (s_lizards != 0) {
        errExitEN(s_lizards, "pthread_mutex_unlock");
    }

    return;
}

void wasp_attack(list_element *head, void *publisher) {
    list_element *current = head;
    list_element *nextNode;

    while (current != NULL) {
        if (current->data.element_type == 1) { /* Head of lizard */
            s_lizards = pthread_mutex_lock(&mtx_lizards);
            if (s_lizards != 0) {
                errExitEN(s_lizards, "pthread_mutex_lock");
            }

            client_lizards[current->data.index_client].score -= 10;

            s_lizards = pthread_mutex_unlock(&mtx_lizards);
            if (s_lizards != 0) {
                errExitEN(s_lizards, "pthread_mutex_unlock");
            }
            
            if (client_lizards[current->data.index_client].score < 0) {
                loser_lizard(current->data.index_client, publisher);
            }
            break;
        }
        nextNode = current->next;
        current = nextNode;
    }
    
    return;
}

/* When a lizard's head moves to a new position,
search and destroy all the cockroaches in that position,
increasing the lizard's score accordingly */
void search_and_destroy_roaches(list_element *head, int index_client) {
    list_element *current = head;
    list_element *nextNode;
    bool end = false;
    bool check;
    dead_roach r;
    square s;

    while (!end) {
        while (current != NULL) {
            if (current->data.element_type == 2) { 
                /* Found roach */

                s_lizards = pthread_mutex_lock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_lock");
                }

                /* Increase lizard score */
                client_lizards[index_client].score += client_roaches[current->data.index_client].char_data[current->data.index_bot].ch - '0';

                s_lizards = pthread_mutex_unlock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_unlock");
                }

                /* Disable cockroach */
                s_roaches = pthread_mutex_lock(&mtx_roaches);
                if (s_roaches != 0) {
                    errExitEN(s_roaches, "pthread_mutex_lock");
                }
                
                client_roaches[current->data.index_client].active[current->data.index_bot] = false;

                s_roaches = pthread_mutex_unlock(&mtx_roaches);
                if (s_roaches != 0) {
                    errExitEN(s_roaches, "pthread_mutex_unlock");
                }
            
                /* Insert roach in inactive roaches list with associated death time */
                r.death_time = s_clock();
                assert(r.death_time >= 0);
                r.index_client = current->data.index_client;
                r.index_bot = current->data.index_bot;
                check = push_fifo(&roaches_killed, r);
                if (!check) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free_exit();
                    exit(0);
                }

                /* Remove roach from field */
                s.element_type = current->data.element_type;
                s.index_client = current->data.index_client;
                s.index_bot = current->data.index_bot;
                s_field = pthread_mutex_lock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_lock");
                }
                deletelist_element(&head, s);

                s_field = pthread_mutex_unlock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_unlock");
                }
                break;
            }
            
            nextNode = current->next;
            current = nextNode;
            if (current == NULL) {
                end = true;
            }
        
        }
    }

    return;
}

/* Update lizards information in the message to be published to the display-app */
void update_lizards() {
    for (int i = 0; i < MAX_LIZARDS; i++) {
        msg_publisher.lizards[i].valid = client_lizards[i].valid;
        msg_publisher.lizards[i].alive = client_lizards[i].alive;
        msg_publisher.lizards[i].score = client_lizards[i].score;
        msg_publisher.lizards[i].char_data.ch = client_lizards[i].char_data.ch;
    }
    return;
}

/* Deal with the display in field of an element in a new position */
list_element *display_in_field(char ch, int x, int y, int index_client, 
                    int index_bot, int element_type, list_element *head, void *publisher) {
    square new_data;
    size_t send1, send2;
    bool check;
    /* element type: 0-tail of lizard, 1-lizard head, 2-roach, 3-wasp, 4-fake lizard head*/

    /* The display field becomes static after the game ends */          
    if (end_game < 2) {
        new_data.element_type = element_type;
        new_data.index_client = index_client;
        new_data.index_bot = index_bot;
        if (ch == ' ') {
            s_field = pthread_mutex_lock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_lock");
            }
            /* Delete from list position xy in field */
            deletelist_element(&head, new_data);

            s_field = pthread_mutex_unlock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_unlock");
            }
            
            if (head != NULL) {
                /* Display an empty space or a previously hidden element in position xy*/
                ch = check_prioritary_element(head); 
            }
        }
        else {
            s_field = pthread_mutex_lock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_lock");
            }
            /* Add to list position xy in field */
            check = insertBegin(&head, new_data);

            s_field = pthread_mutex_unlock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_unlock");
            }
            if (!check) {
                fprintf(stderr, "Memory allocation failed\n");
                free_exit();
                exit(0);
            }

            if (head != NULL && element_type == 0) {
                /* Choose to display the most important element in position xy */
                ch = check_prioritary_element(head);
            }

            if (element_type == 1) {
                /* If it was a movement from a roach head, deal with roaches in position xy */
                search_and_destroy_roaches(head, index_client);
            }

        }
        
        /* Display ch in position xy */
        // wmove(my_win, x, y);
        // waddch(my_win, ch | A_BOLD);
        // wrefresh(my_win);

        s_publish = pthread_mutex_lock(&mtx_publish);
        if (s_publish != 0) {
            errExitEN(s_publish, "pthread_mutex_lock");
        }
        /* Publish message to the subscribers regarding the previous display update */

        msg_publisher.field[x][y] = ch;
        msg_publisher.x_upd = x;
        msg_publisher.y_upd = y;
        
        update_lizards();

        send1 = zmq_send(publisher, password, strlen(password), ZMQ_SNDMORE);
        assert(send1 != -1);
        send2 = zmq_send(publisher, &msg_publisher, sizeof(msg), 0);  
        assert(send2 != -1);

        s_publish = pthread_mutex_unlock(&mtx_publish);
        if (s_publish != 0) {
            errExitEN(s_publish, "pthread_mutex_unlock");
        }

        
    }
    return head;
}

/* Insert tail in new position of the playing field */
void tail(direction_t direction, int x, int y, bool delete, int index_client, void * publisher) {
    char display;
    int index_bot = -1;  
    int element_type = 0;
    list_element *field_aux;

    /* If the game has already ended and there are no '*' on the playing field, 
    display a tail of '*' instead of ' ' */
    if (end_game == 1) {
        display = '*';
    }
    else {
        if (delete == true) {
            display = ' ';
        }
        else {
            display = '.';
        }
    }
    
    switch (direction)
    {
    case LEFT:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (y + kk < WINDOW_SIZE - 1) {          
                field_aux = display_in_field(display, x, y + kk, index_client, 
                    index_bot, element_type, field[x][y + kk], publisher);
                
                s_field = pthread_mutex_lock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_lock");
                }

                field[x][y + kk] = field_aux;

                s_field = pthread_mutex_unlock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_unlock");
                }
            }
        }
        break;

    case RIGHT:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (y - kk > 1) {
                field_aux = display_in_field(display, x, y - kk, index_client, 
                    index_bot, element_type, field[x][y - kk], publisher);

                s_field = pthread_mutex_lock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_lock");
                }
                field[x][y - kk] = field_aux;

                s_field = pthread_mutex_unlock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_unlock");
                }
            }
        }
        break;

    case DOWN:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (x - kk > 1) {
                field_aux = display_in_field(display, x - kk, y, index_client, 
                    index_bot, element_type, field[x - kk][y], publisher);

                s_field = pthread_mutex_lock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_lock");
                }

                field[x - kk][y] = field_aux;

                s_field = pthread_mutex_unlock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_unlock");
                }
            }
        }
        break;

    case UP:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (x + kk < WINDOW_SIZE - 1) {
                field_aux = display_in_field(display, x + kk, y, index_client, 
                    index_bot, element_type, field[x + kk][y], publisher);

                s_field = pthread_mutex_lock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_lock");
                }
                
                field[x + kk][y] = field_aux;
                
                s_field = pthread_mutex_unlock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_unlock");
                }
            }
        }
        break;
        
    default:
        break;
    }
}

/* Check if the movement of element of type is invalid */
/* type: 0-tail of lizard, 1-lizard head, 2-roach, 3-wasp, 4-fake lizard head*/
int check_in_square(list_element *head, int type) {
    list_element *current = head;
    list_element *nextNode;

    if (type != 1 && type != 2 && type != 3) {
        return -1;
    }

    while (current != NULL) {
        if (type == 1) {
            if (current->data.element_type == 1 || current->data.element_type == 3) {
                return current->data.element_type;
            }
        }
        else if (type == 2) {
            if (current->data.element_type == 1 || current->data.element_type == 3) {
                return current->data.element_type;
            }
        }
        else if (type == 3) {
            if (current->data.element_type == 1 || current->data.element_type == 2 || current->data.element_type == 3) {
                return current->data.element_type;
            }
        }
        
        nextNode = current->next;
        current = nextNode;
    }
    return -1;
}

/* Regarding a field position, check which element in that position is the most prioritary:
Priority order: lizard head, roach, tail, fake lizard head */
char check_prioritary_element(list_element *head) {
    list_element *current = head;
    list_element *nextNode;
    char winner = '?';
    int winner_type = -1;

    while (current != NULL) {
        if (current->data.element_type == 1) { /* Head of lizard */
            winner = client_lizards[current->data.index_client].char_data.ch;
            
            return winner;
        }
        else if (current->data.element_type == 3 && (winner_type == -1 || winner_type == 0 || winner_type == 2 || winner_type == 4)) {
            winner = client_wasps[current->data.index_client].char_data[current->data.index_bot].ch;
            winner_type = 3;    
        }
        else if (current->data.element_type == 2 && (winner_type == -1 || winner_type == 0 || winner_type == 4)) {
            winner = client_roaches[current->data.index_client].char_data[current->data.index_bot].ch;
            winner_type = 2;    
        }
        else if (current->data.element_type == 0 && (winner_type == -1 || winner_type == 4)) {
            if (end_game == 1) {
                winner = '*';
            }
            else {
                winner = '.';
            }
            winner_type = 0;
        }
        else if (current->data.element_type == 4 && winner_type == -1) {
            winner = client_lizards[current->data.index_client].char_data.ch;
        }
        nextNode = current->next;
        current = nextNode;
    }
    
    return winner;
}

void *free_safe (void *aux) {
    if (aux != NULL) {
        free(aux);
    }
    return NULL;
}

list_element ***allocate3DArray() {
    list_element ***table = (list_element ***)malloc(WINDOW_SIZE * sizeof(list_element **));
    if (table == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    for (int i = 0; i < WINDOW_SIZE; i++) {
        table[i] = (list_element **)malloc(WINDOW_SIZE * sizeof(list_element *));
        if (table[i] == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            for (int k = 0; k < i; k++) {
                table[k] = free_safe(table[k]);
            }
            table = free_safe(table);
            return NULL;
        }
        for (int j = 0; j < WINDOW_SIZE; j++) {
            table[i][j] = NULL;
        }

    }
    return table;
}

void free3DArray(list_element ***table) {
    if (table == NULL) {
        return; /* Nothing to free */
    }

    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (table[i] != NULL) {
            for (int j = 0; j < WINDOW_SIZE; j++) {
                /* Free the third dimension if it was allocated */
                freeList(table[i][j]);
            }
            /* Free the second dimension */
            table[i] = free_safe(table[i]);
        }
    }

    /* Finally, free the first dimension */
    table = free_safe(table);
}

/* Respawn time after a roach was killed, ressurect it */
void ressurect_roaches() {
    int64_t end_time, inactive_time;

    /* Verify, in the ordered (descending by death_time) dead roaches fifo,
    which can be ressurected */
    while (roaches_killed != NULL) {
        end_time = s_clock();
        assert(end_time >= 0);
        /* Compute their downtime and compare it with respawn time*/
        inactive_time = (end_time - roaches_killed->data.death_time); 

        /* This would be done even better with threads */
        if (inactive_time >= RESPAWN_TIME) {


            /* Activate roach*/
        
        
        
            s_roaches = pthread_mutex_lock(&mtx_roaches);
            if (s_roaches != 0) {
                errExitEN(s_roaches, "pthread_mutex_lock");
            }
            
            client_roaches[roaches_killed->data.index_client].active[roaches_killed->data.index_bot] = true;
        
            /* Compute its new random field position */
            while (1) {
                client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_bot].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_bot].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                /* Verify if new position matches the position of anothers' head lizard or wasp */
                if (check_in_square(field[client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_bot].pos_x]
                    [client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_bot].pos_y], 2) == -1) {
                    break;
                }
            }
            
            s_roaches = pthread_mutex_unlock(&mtx_roaches);
            if (s_roaches != 0) {
                errExitEN(s_roaches, "pthread_mutex_unlock");
            }
            
            /* Remove from dead roaches fifo */
            pop_fifo(&roaches_killed);

            
        }      
        else {
            break;
        }
        
    }

    return;
}

/* Display each player's (lizard's) name and score */
void display_stats() {
    /* When the game ends, there will be no more updates on this board */
   
    if (end_game <= 2) {
        if (end_game == 2) {
            end_game++;
        }
        int i = 0;
        /* Ordered by name */
        for (int j = 0; j < MAX_LIZARDS; j++) {
            
            s_field = pthread_mutex_lock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_lock");
            }

            mvwprintw(stats_win, j, 0, "\t\t\t\t\t");
            wrefresh(stats_win);
            if (client_lizards[j].valid) {
                if (client_lizards[j].alive) {
                    mvwprintw(stats_win, i, 0, "Player %c: Score = %lf", client_lizards[j].char_data.ch, client_lizards[j].score);
                    wrefresh(stats_win);
                }
                else {
                    mvwprintw(stats_win, i, 0, "Player %c: Lost!\t\t\t\t", client_lizards[j].char_data.ch);
                    wrefresh(stats_win);
                }
                i++;
            }

            s_field = pthread_mutex_unlock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_unlock");
            }
        }
    }
}

/* Free allocated memory if an error occurs */
void free_exit() {
    delwin(stats_win);
    delwin(debug_win);
    delwin(my_win);
    endwin();
    
    int rc = zmq_ctx_destroy (context);
    assert(rc == 0);
    client_roaches = free_safe(client_roaches);
    client_wasps = free_safe(client_wasps);
    free3DArray(field);
    password = free_safe(password);
    zmq_close(publisher);
    zmq_close(frontend);
    zmq_close(backend);
    zmq_close(responder);
    
}

void loser_lizard(int index, void *publisher) {
    int pos_x, pos_y;
    char ch;
    int index_bot, element_type;
    list_element *field_aux;

    pos_x = client_lizards[index].char_data.pos_x;
    pos_y = client_lizards[index].char_data.pos_y;
    ch = client_lizards[index].char_data.ch;

    /* Remove lizard tail from the playing field in the old position */
    tail(client_lizards[index].prevdirection, pos_x, pos_y, 
        true, index, publisher);
    
    index_bot = -1;  
    element_type = 1;

    /* Remove roach from the playing field in old position */
    field_aux = display_in_field(' ', pos_x, pos_y, index, 
        index_bot, element_type, field[pos_x][pos_y], publisher);
    
    s_field = pthread_mutex_lock(&mtx_field);
    if (s_field != 0) {
        errExitEN(s_field, "pthread_mutex_lock");
    }
    
    field[pos_x][pos_y] = field_aux;
    
    s_field = pthread_mutex_unlock(&mtx_field);
    if (s_field != 0) {
        errExitEN(s_field, "pthread_mutex_unlock");
    }
    
    s_lizards = pthread_mutex_lock(&mtx_lizards);
    if (s_lizards != 0) {
        errExitEN(s_lizards, "pthread_mutex_lock");
    }

    /* Inactivate lizard, and decrement number of lizards */
    client_lizards[index].alive = false;

    s_lizards = pthread_mutex_unlock(&mtx_lizards);
    if (s_lizards != 0) {
        errExitEN(s_lizards, "pthread_mutex_unlock");
    }

    // Add fake head to field
    element_type = 4; // fake lizard head
    
    field_aux = display_in_field(ch, pos_x, pos_y, index, 
        index_bot, element_type, field[pos_x][pos_y], publisher);
        
    s_field = pthread_mutex_lock(&mtx_field);
    if (s_field != 0) {
        errExitEN(s_field, "pthread_mutex_lock");
    }
    
    field[pos_x][pos_y] = field_aux;
    
    s_field = pthread_mutex_unlock(&mtx_field);
    if (s_field != 0) {
        errExitEN(s_field, "pthread_mutex_unlock");
    }
}

RemoteChar  * zmq_read_RemoteChar(void * responder_3){
    zmq_msg_t msg_raw;
    zmq_msg_init (&msg_raw);

    int n_bytes = zmq_recvmsg(responder_3, &msg_raw, 0);

    const uint8_t *pb_msg = (const uint8_t*) zmq_msg_data(&msg_raw);

    RemoteChar  * ret_value =  
            remote_char__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close (&msg_raw); 
    return ret_value;
}

void zmq_send_OkMessage(void * responder_3, int ok){

    OkMessage m_struct = OK_MESSAGE__INIT;
    m_struct.msg_ok = ok;
    
    int size_bin_msg = ok_message__get_packed_size(&m_struct);
    uint8_t * pb_m_bin = malloc(size_bin_msg);
    ok_message__pack(&m_struct, pb_m_bin);
        
    zmq_send(responder_3, pb_m_bin, size_bin_msg, 0);

    //free(pb_m_bin);
    //free(pb_m_struct.ch.data);

}

void zmq_send_MyScore(void * responder_3, double my_score){

    ScoreMessage m_struct = SCORE_MESSAGE__INIT;
    m_struct.my_score = my_score;
    
    int size_bin_msg = score_message__get_packed_size(&m_struct);
    uint8_t * pb_m_bin = malloc(size_bin_msg);
    score_message__pack(&m_struct, pb_m_bin);

    
    zmq_send(responder_3, pb_m_bin, size_bin_msg, 0);


    //free(pb_m_bin);
    //free(pb_m_struct.ch.data);

}

bool disconnect_wasp(int index, void *publisher) {
    int element_type, pos_x_wasps, pos_y_wasps, index_client, index_bot;
    list_element *field_aux;
    
    if (index != -1) {
        for (int i = 0; i < client_wasps[index].nchars; i++) {
            if (client_wasps[index].active[i] == true) {
                /* For each active wasp, compute its new position */
                pos_x_wasps = client_wasps[index].char_data[i].pos_x;
                pos_y_wasps = client_wasps[index].char_data[i].pos_y;

                index_client = index;
                index_bot = i;  
                element_type = 3;

                field_aux = display_in_field(' ', pos_x_wasps, pos_y_wasps, index_client, 
                    index_bot, element_type, field[pos_x_wasps][pos_y_wasps], publisher);
                    
                s_field = pthread_mutex_lock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_lock");
                }
                /* Remove wasp from the playing field in old position */
                field[pos_x_wasps][pos_y_wasps] = field_aux;
                
                s_field = pthread_mutex_unlock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_unlock");
                }
            }
        }

        /* Inactivate wasp, and decrement number of wasps */

        s_wasps = pthread_mutex_lock(&mtx_wasps);
        if (s_wasps != 0) {
            errExitEN(s_wasps, "pthread_mutex_lock");
        }
        
        client_wasps[index].valid = false;
        n_clients_wasps--;
        total_wasps -= client_wasps[index].nchars;
        
        s_wasps = pthread_mutex_unlock(&mtx_wasps);
        if (s_wasps != 0) {
            errExitEN(s_wasps, "pthread_mutex_unlock");
        }
        
        return true;
    }
    return false;
}

bool disconnect_roach(int index, void *publisher) {
    int element_type, pos_x_roaches, pos_y_roaches, index_client, index_bot;
    list_element *field_aux;
    
    if (index != -1) {
        for (int i = 0; i < client_roaches[index].nchars; i++) {
            if (client_roaches[index].active[i] == true) {
                /* For each active roach, compute its new position */
                pos_x_roaches = client_roaches[index].char_data[i].pos_x;
                pos_y_roaches = client_roaches[index].char_data[i].pos_y;

                index_client = index;
                index_bot = i;  
                element_type = 3;

                field_aux = display_in_field(' ', pos_x_roaches, pos_y_roaches, index_client, 
                    index_bot, element_type, field[pos_x_roaches][pos_y_roaches], publisher);

                s_field = pthread_mutex_lock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_lock");
                }
            
                /* Remove roach from the playing field in old position */
                field[pos_x_roaches][pos_y_roaches] = field_aux;
                
                s_field = pthread_mutex_unlock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_unlock");
                }
            }
        }
        
        s_roaches = pthread_mutex_lock(&mtx_roaches);
        if (s_roaches != 0) {
            errExitEN(s_roaches, "pthread_mutex_lock");
        }
        
        /* Inactivate roach, and decrement number of roaches */
        client_roaches[index].valid = false;
        n_clients_roaches--;
        total_roaches -= client_roaches[index].nchars;
        
        s_roaches = pthread_mutex_unlock(&mtx_roaches);
        if (s_roaches != 0) {
            errExitEN(s_roaches, "pthread_mutex_unlock");
        }
        return true;
    }
    return false;
}

bool disconnect_lizard(int index, void *publisher) {
    int element_type, pos_x_lizards, pos_y_lizards, index_client, index_bot;
    list_element *field_aux;


    if (index != -1) {
        pos_x_lizards = client_lizards[index].char_data.pos_x;
        pos_y_lizards = client_lizards[index].char_data.pos_y;

        if (client_lizards[index].alive) {
            /* Remove lizard tail from the playing field in the old position */
            tail(client_lizards[index].prevdirection, pos_x_lizards, pos_y_lizards, 
                true, index, publisher);
        }

        index_client = index;
        index_bot = -1;  
        element_type = 1;

        if (client_lizards[index].alive) {
            field_aux = display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                index_bot, element_type, field[pos_x_lizards][pos_y_lizards], publisher);
            
            s_field = pthread_mutex_lock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_lock");
            }
            
            /* Remove lizard from the playing field in old position */
            field[pos_x_lizards][pos_y_lizards] = field_aux;
            
            s_field = pthread_mutex_unlock(&mtx_field); 
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_unlock");
            }
        }
        else {
            element_type = 4;
            
            field_aux = display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                index_bot, element_type, field[pos_x_lizards][pos_y_lizards], publisher);
                
            s_field = pthread_mutex_lock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_lock");
            }

            field[pos_x_lizards][pos_y_lizards] = field_aux;

            s_field = pthread_mutex_unlock(&mtx_field);
            if (s_field != 0) {
                errExitEN(s_field, "pthread_mutex_unlock");
            }
        }

        s_lizards = pthread_mutex_lock(&mtx_lizards);
        if (s_lizards != 0) {
            errExitEN(s_lizards, "pthread_mutex_lock");
        }
        
        /* Inactivate lizard, and decrement number of lizards */
        client_lizards[index].valid = false;
        total_lizards--;

        s_lizards = pthread_mutex_unlock(&mtx_lizards);
        if (s_lizards != 0) {
            errExitEN(s_lizards, "pthread_mutex_unlock");
        }
        
        return true;
    }
    return false;
}

void *thread_function(void *arg) {
    int64_t now;
    // int64_t inactivity_time;
    // int i = 0;
    // int rc2;

    while(1) {
        
        /* Deal with ressurection of dead roaches (after respawn time) */
        ressurect_roaches();
        
        now = s_clock();
        assert(now >= 0);

        // for (i = 0; i < max_bots; i++) {
        //     if (client_roaches[i].valid) {
        //         inactivity_time = now - client_roaches[i].previous_interaction;
        //         if (inactivity_time > TIMEOUT_THRESHOLD) {

        //             disconnect_roach(i, publisher);
        //         }
        //     }
        // }

        // for (i = 0; i < max_bots; i++) {
        //     if (client_wasps[i].valid) {
        //         inactivity_time = now - client_wasps[i].previous_interaction;
        //         if (inactivity_time > TIMEOUT_THRESHOLD) {
        //             disconnect_wasp(i, publisher);
        //         }
        //     }
        // }

        // for (i = 0; i < MAX_LIZARDS; i++) {
        //     if (client_lizards[i].valid) {
        //         inactivity_time = now - client_lizards[i].previous_interaction;
        //         // mvwprintw(debug_win, 2, 1, "prev_iteraction: %ld", client_lizards[i].previous_interaction);
        //         // wrefresh(debug_win);
        //         // mvwprintw(debug_win, 3, 1, "inactive_time: %ld", inactivity_time);
        //         // wrefresh(debug_win);

        //         if (inactivity_time > TIMEOUT_THRESHOLD) {
                    
        //             disconnect_lizard(i, publisher);

        //         }
        //     }
        // }
        
    }
    return 0;
}

void *thread_function_display(void *arg) {
    int field_real[WINDOW_SIZE][WINDOW_SIZE];

    for (int i = 1; i < WINDOW_SIZE-1; i++) {
        for (int j = 1; j < WINDOW_SIZE-1; j++) {
            field_real[i][j] = ' ';
        }
    }

    while (1) {
        for (int i = 1; i < WINDOW_SIZE-1; i++) {
            for (int j = 1; j < WINDOW_SIZE-1; j++) {
                int ch = msg_publisher.field[i][j];
                if (ch == '?') {
                    ch = ' ';
                }
                if (ch != field_real[i][j]) {
                    wmove(my_win, i, j);
                    waddch(my_win, ch | A_BOLD);
                    wrefresh(my_win); 
                    field_real[i][j] = ch;
                    display_stats();
                }
            }
        }
        
    }
}

void *thread_function_msg_wasp_roaches(void *arg) {
    RemoteChar *m;
    bool success;
    int ok = 1, index_of_position_to_insert = -1;
    int pos_x_roaches, pos_y_roaches,pos_x_wasps, pos_y_wasps;
    int pos_x_roaches_aux, pos_y_roaches_aux, pos_x_wasps_aux, pos_y_wasps_aux;
    int index_client, index_bot, element_type;
    int ch, i, jj, index_client_roaches_id, index_client_wasps_id;
    list_element *field_aux_main;
    int check = -1, jjj = 0;

    while (1) {
        // mvwprintw(debug_win, 2, 0, "rNw aqui");
        // wrefresh(debug_win);

        /* Receive client message */ 
        m = zmq_read_RemoteChar(responder);

        // mvwprintw(debug_win, 0, 0, "rNw msg_type: %d", m->msg_type);
        // wrefresh(debug_win);

        // recv = zmq_recv (responder, &m, sizeof(remote_char_t), 0);
        // assert(recv != -1);        
        
        if (m->msg_type == 0) { /* Connection from roach client */
            /* Testing if roach max number allows new roach client */

            if (m->nchars + total_roaches + total_wasps > max_bots) {
                ok = 0;
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
                continue;
            }

            /* Check if new client id is already in use */
            for (int iii = 0; iii < n_clients_roaches; iii++) {
                if (client_roaches[iii].id == m->id) {
                    ok = (int) '?';
                    zmq_send_OkMessage(responder, ok);
                    // send = zmq_send (responder, &ok, sizeof(int), 0);
                    // assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }

            for (int iii = 0; iii < MAX_LIZARDS; iii++) {
                if (client_lizards[iii].valid && client_lizards[iii].id == m->id) {
                    ok = (int) '?';
                    zmq_send_OkMessage(responder, ok);
                    // send = zmq_send (responder, &ok, sizeof(int), 0);
                    // assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }

            /* Find roach client slot available to insert a new one */
            index_of_position_to_insert = -1;
            for (jj = 0; jj < max_bots; jj++) {
                if (!client_roaches[jj].valid) {
                    index_of_position_to_insert = jj;
                    break;
                }
            }
            

            if (index_of_position_to_insert != -1) {
                /* Send successful response to client */
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;

                s_roaches = pthread_mutex_lock(&mtx_roaches);
                if (s_roaches != 0) {
                    errExitEN(s_roaches, "pthread_mutex_lock");
                }
                
                client_roaches[index_of_position_to_insert].id = m->id;
                client_roaches[index_of_position_to_insert].nchars = m->nchars;
                client_roaches[index_of_position_to_insert].valid = true;
                client_roaches[index_of_position_to_insert].previous_interaction = s_clock();
                assert(client_roaches[index_of_position_to_insert].previous_interaction >= 0);

                s_roaches = pthread_mutex_unlock(&mtx_roaches);
                if (s_roaches != 0) {
                    errExitEN(s_roaches, "pthread_mutex_unlock");
                }

                for (int i = 0; i < m->nchars; i++) {
                    /* Insert in server each roach information */
                    
                    s_roaches = pthread_mutex_lock(&mtx_roaches);
                    if (s_roaches != 0) {
                        errExitEN(s_roaches, "pthread_mutex_lock");
                    }
                    client_roaches[index_of_position_to_insert].char_data[i].ch = m->ch[i];
                    client_roaches[index_of_position_to_insert].active[i] = true;

                    /* Compute roach initial random position */
                    while (1) {
                        client_roaches[index_of_position_to_insert].char_data[i].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                        client_roaches[index_of_position_to_insert].char_data[i].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                        /* Verify if new position matches the position of anothers' head lizard or wasp */
                        if (check_in_square(field[client_roaches[index_of_position_to_insert].char_data[i].pos_x][ 
                            client_roaches[index_of_position_to_insert].char_data[i].pos_y], 2) == -1) {
                            break;
                        }
                    }
                    
                    s_roaches = pthread_mutex_unlock(&mtx_roaches);
                    if (s_roaches != 0) {
                        errExitEN(s_roaches, "pthread_mutex_unlock");
                    }
                    
                    pos_x_roaches = client_roaches[index_of_position_to_insert].char_data[i].pos_x;
                    pos_y_roaches = client_roaches[index_of_position_to_insert].char_data[i].pos_y;
                    ch = client_roaches[index_of_position_to_insert].char_data[i].ch;

                    index_client = index_of_position_to_insert;
                    index_bot = i;  
                    element_type = 2;

                    field_aux_main = display_in_field(ch, pos_x_roaches, pos_y_roaches, index_client, 
                        index_bot, element_type, field[pos_x_roaches][pos_y_roaches], publisher);
                    /* Insert roach into the playing field */
                    s_field = pthread_mutex_lock(&mtx_field);
                    if (s_field != 0) {
                        errExitEN(s_field, "pthread_mutex_lock");
                    }
                    field[pos_x_roaches][pos_y_roaches] = field_aux_main;

                    s_field = pthread_mutex_unlock(&mtx_field);
                    if (s_field != 0) {
                        errExitEN(s_field, "pthread_mutex_unlock");
                    }
                }

                s_roaches = pthread_mutex_lock(&mtx_roaches);
                if (s_roaches != 0) {
                    errExitEN(s_roaches, "pthread_mutex_lock");
                }
                /* Increase number of client_roaches and number of roaches*/
                n_clients_roaches++;
                total_roaches += m->nchars;

                s_roaches = pthread_mutex_unlock(&mtx_roaches);
                if (s_roaches != 0) {
                    errExitEN(s_roaches, "pthread_mutex_unlock");
                }
            }
            else {
                /* Send successful response to client */
                ok = (int) '?';
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
        }
        else if (m->msg_type == 1) { /* Movements from roach client */
            /* Find roach in client_roaches from its id */
            index_client_roaches_id = -1;
            for (jjj = 0; jjj < n_clients_roaches;jjj++) {
                if (client_roaches[jjj].id == m->id) {
                    index_client_roaches_id = jjj;
                    break;
                }
            }

            if (index_client_roaches_id != -1) {
                s_roaches = pthread_mutex_lock(&mtx_roaches);
                if (s_roaches != 0) {
                    errExitEN(s_roaches, "pthread_mutex_lock");
                }
                
                client_roaches[index_client_roaches_id].previous_interaction = s_clock();
                assert(client_roaches[index_client_roaches_id].previous_interaction >= 0);

                s_roaches = pthread_mutex_unlock(&mtx_roaches);
                if (s_roaches != 0) {
                    errExitEN(s_roaches, "pthread_mutex_unlock");
                }
                
                /* Send successful response to client */
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;

                for (i = 0; i < m->nchars; i++) {
                    if (client_roaches[index_client_roaches_id].active[i] == true) {
                        /* For each active roach, compute its new position */
                        pos_x_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_x;
                        pos_y_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_y;
                        ch = client_roaches[index_client_roaches_id].char_data[i].ch;
                        pos_x_roaches_aux = pos_x_roaches;
                        pos_y_roaches_aux = pos_y_roaches;
                        new_position(&pos_x_roaches_aux, &pos_y_roaches_aux, m->direction[i]);

                        /* Check if new position is inside the playing field */
                        if (!(pos_x_roaches_aux < 0 || pos_y_roaches_aux < 0  || pos_x_roaches_aux >= WINDOW_SIZE || pos_y_roaches_aux  >= WINDOW_SIZE)) {
                            /* Verify if new position matches the position of anothers' head lizard or wasp */
                            if (check_in_square(field[pos_x_roaches_aux][pos_y_roaches_aux], 2) == -1) {
                                
                                index_client = index_client_roaches_id;
                                index_bot = i;  
                                element_type = 2;

                                field_aux_main = display_in_field(' ', pos_x_roaches, pos_y_roaches, index_client, 
                                    index_bot, element_type, field[pos_x_roaches][pos_y_roaches], publisher);
                                /* Remove roach from the playing field in old position */
                                s_field = pthread_mutex_lock(&mtx_field);
                                if (s_field != 0) {
                                    errExitEN(s_field, "pthread_mutex_lock");
                                }
                                field[pos_x_roaches][pos_y_roaches] = field_aux_main;

                                s_field = pthread_mutex_unlock(&mtx_field);
                                if (s_field != 0) {
                                    errExitEN(s_field, "pthread_mutex_unlock");
                                }

                                /* Updates roach position */
                                pos_x_roaches = pos_x_roaches_aux;
                                pos_y_roaches = pos_y_roaches_aux;

                                s_roaches = pthread_mutex_lock(&mtx_roaches);
                                if (s_roaches != 0) {
                                    errExitEN(s_roaches, "pthread_mutex_lock");
                                }
                                
                                client_roaches[index_client_roaches_id].char_data[i].pos_x = pos_x_roaches;
                                client_roaches[index_client_roaches_id].char_data[i].pos_y = pos_y_roaches;

                                s_roaches = pthread_mutex_unlock(&mtx_roaches);
                                if (s_roaches != 0) {
                                    errExitEN(s_roaches, "pthread_mutex_unlock");
                                }

                                field_aux_main = display_in_field(ch, pos_x_roaches, pos_y_roaches, index_client, 
                                    index_bot, element_type, field[pos_x_roaches][pos_y_roaches], publisher);

                                /* Insert roach into the playing field in new position */
                                
                                s_field = pthread_mutex_lock(&mtx_field);
                                if (s_field != 0) {
                                    errExitEN(s_field, "pthread_mutex_lock");
                                }
                                
                                field[pos_x_roaches][pos_y_roaches] = field_aux_main;

                                s_field = pthread_mutex_unlock(&mtx_field);
                                if (s_field != 0) {
                                    errExitEN(s_field, "pthread_mutex_unlock");
                                }
                            }
                        }
                    }
                }
            }
            else {
                /* Send unsuccessful response to client */
                ok = (int) '?';
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
        }
        else if (m->msg_type == 5) { /* Disconnection from roaches */

            /* Find roach in client_roaches from its id */
            index_client_roaches_id = -1;
            for (jjj = 0; jjj < max_bots; jjj++) {
                if (client_roaches[jjj].id == m->id && client_roaches[jjj].valid) {
                    index_client_roaches_id = jjj;
                    break;
                }
            }

            success = disconnect_roach(index_client_roaches_id, publisher);

            if (!success) {
                /* Send successful response to client */
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
            else {
                /* Send unsuccessful response to client */
                ok = (int) '?';
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
        }
        else if (m->msg_type == 6) { /* Connection from wasp client */ 
            /* Testing if roach max number allows new wasp clien */
            if (m->nchars + total_roaches + total_wasps > max_bots) {
                ok = 0;
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
                continue;
            }
            /* Check if new client id is already in use */
            for (int iii = 0; iii < n_clients_wasps; iii++) {
                if (client_wasps[iii].id == m->id) {
                    ok = (int) '?';
                    zmq_send_OkMessage(responder, ok);
                    // send = zmq_send (responder, &ok, sizeof(int), 0);
                    // assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }

            for (int iii = 0; iii < MAX_LIZARDS; iii++) {
                if (client_lizards[iii].valid && client_lizards[iii].id == m->id) {
                    ok = (int) '?';
                    zmq_send_OkMessage(responder, ok);
                    // send = zmq_send (responder, &ok, sizeof(int), 0);
                    // assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }

            /* Find wasp client slot available to insert a new one */
            index_of_position_to_insert = -1;
            for (jj = 0; jj < max_bots; jj++) {
                if (!client_wasps[jj].valid) {
                    index_of_position_to_insert = jj;
                    break;
                }
            }

            if (index_of_position_to_insert != -1) {
                /* Send successful response to client */
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;

                s_wasps = pthread_mutex_lock(&mtx_wasps);
                if (s_wasps != 0) {
                    errExitEN(s_wasps, "pthread_mutex_lock");
                }
                
                client_wasps[index_of_position_to_insert].id = m->id;
                client_wasps[index_of_position_to_insert].nchars = m->nchars;
                client_wasps[index_of_position_to_insert].valid = true;
                client_wasps[index_of_position_to_insert].previous_interaction = s_clock();
                assert(client_wasps[index_of_position_to_insert].previous_interaction >= 0);

                s_wasps = pthread_mutex_unlock(&mtx_wasps);
                if (s_wasps != 0) {
                    errExitEN(s_wasps, "pthread_mutex_unlock");
                }
        
                for (i = 0; i < m->nchars; i++) {
                    /* Insert in server each wasp information */
                    s_wasps = pthread_mutex_lock(&mtx_wasps);
                    if (s_wasps != 0) {
                        errExitEN(s_wasps, "pthread_mutex_lock");
                    }

                    client_wasps[index_of_position_to_insert].char_data[i].ch = m->ch[i];
                    client_wasps[index_of_position_to_insert].active[i] = true;

                    /* Compute wasp initial random position */
                    while (1) {
                        client_wasps[index_of_position_to_insert].char_data[i].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                        client_wasps[index_of_position_to_insert].char_data[i].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                        /* Verify if new position matches the position of anothers' head lizard */
                        if (check_in_square(field[client_wasps[index_of_position_to_insert].char_data[i].pos_x][ 
                            client_wasps[index_of_position_to_insert].char_data[i].pos_y], 3) == -1) {
                            break;
                        }
                    }

                    s_wasps = pthread_mutex_unlock(&mtx_wasps);
                    if (s_wasps != 0) {
                        errExitEN(s_wasps, "pthread_mutex_unlock");
                    }
                    
                    pos_x_wasps = client_wasps[index_of_position_to_insert].char_data[i].pos_x;
                    pos_y_wasps = client_wasps[index_of_position_to_insert].char_data[i].pos_y;
                    ch = client_wasps[index_of_position_to_insert].char_data[i].ch;

                    index_client = index_of_position_to_insert;
                    index_bot = i;  
                    element_type = 3;

                    field_aux_main = display_in_field(ch, pos_x_wasps, pos_y_wasps, index_client, 
                        index_bot, element_type, field[pos_x_wasps][pos_y_wasps], publisher);
                        
                    /* Insert wasp into the playing field */
                    
                    s_field = pthread_mutex_lock(&mtx_field);
                    if (s_field != 0) {
                        errExitEN(s_field, "pthread_mutex_lock");
                    }
                    
                    field[pos_x_wasps][pos_y_wasps] = field_aux_main;

                    s_field = pthread_mutex_unlock(&mtx_field);
                    if (s_field != 0) {
                        errExitEN(s_field, "pthread_mutex_unlock");
                    }
                }
                
                /* Increase number of client_wasps and number of wasps*/
                s_wasps = pthread_mutex_lock(&mtx_wasps);
                if (s_wasps != 0) {
                    errExitEN(s_wasps, "pthread_mutex_lock");
                }
                
                n_clients_wasps++;
                total_wasps += m->nchars;

                s_wasps = pthread_mutex_unlock(&mtx_wasps);
                if (s_wasps != 0) {
                    errExitEN(s_wasps, "pthread_mutex_unlock");
                }
            }
            else {
                /* Send unsuccessful response to client */
                ok = (int) '?';
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
        }
        else if (m->msg_type == 7) { /* Movements from wasp client */
            
            /* Find wasp in client_wasps from its id */
            index_client_wasps_id = -1;
            for (jjj = 0; jjj < n_clients_wasps;jjj++) {
                if (client_wasps[jjj].id == m->id) {
                    index_client_wasps_id = jjj;
                    break;
                }
            }
            if (index_client_wasps_id != -1) {
                s_wasps = pthread_mutex_lock(&mtx_wasps);
                if (s_wasps != 0) {
                    errExitEN(s_wasps, "pthread_mutex_lock");
                }
                
                client_wasps[index_client_wasps_id].previous_interaction = s_clock();
                assert(client_wasps[index_client_wasps_id].previous_interaction >= 0);

                s_wasps = pthread_mutex_unlock(&mtx_wasps);
                if (s_wasps != 0) {
                    errExitEN(s_wasps, "pthread_mutex_unlock");
                }

                /* Send successful response to client */
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;

                for (i = 0; i < m->nchars; i++) {
                    if (client_wasps[index_client_wasps_id].active[i] == true) {
                        /* For each active wasp, compute its new position */
                        pos_x_wasps = client_wasps[index_client_wasps_id].char_data[i].pos_x;
                        pos_y_wasps = client_wasps[index_client_wasps_id].char_data[i].pos_y;
                        ch = client_wasps[index_client_wasps_id].char_data[i].ch;
                        pos_x_wasps_aux = pos_x_wasps;
                        pos_y_wasps_aux = pos_y_wasps;
                        new_position(&pos_x_wasps_aux, &pos_y_wasps_aux, m->direction[i]);

                        /* Check if new position is inside the playing field */
                        if (!(pos_x_wasps_aux < 0 || pos_y_wasps_aux < 0  || pos_x_wasps_aux >= WINDOW_SIZE || pos_y_wasps_aux  >= WINDOW_SIZE)) {
                            /* Verify if new position matches the position of anothers' head lizard */
                            check = check_in_square(field[pos_x_wasps_aux][pos_y_wasps_aux], 3);
                            if (check == -1) {
                                
                                index_client = index_client_wasps_id;
                                index_bot = i;  
                                element_type = 3;

                                field_aux_main = display_in_field(' ', pos_x_wasps, pos_y_wasps, index_client, 
                                    index_bot, element_type, field[pos_x_wasps][pos_y_wasps], publisher);

                                /* Remove wasp from the playing field in old position */
                                
                                s_field = pthread_mutex_lock(&mtx_field);
                                if (s_field != 0) {
                                    errExitEN(s_field, "pthread_mutex_lock");
                                }
                                
                                field[pos_x_wasps][pos_y_wasps] = field_aux_main;

                                s_field = pthread_mutex_unlock(&mtx_field);
                                if (s_field != 0) {
                                    errExitEN(s_field, "pthread_mutex_unlock");
                                }

                                /* Updates wasp position */
                                pos_x_wasps = pos_x_wasps_aux;
                                pos_y_wasps = pos_y_wasps_aux;
                                
                                s_wasps = pthread_mutex_lock(&mtx_wasps);
                                if (s_wasps != 0) {
                                    errExitEN(s_wasps, "pthread_mutex_lock");
                                }
                                
                                client_wasps[index_client_wasps_id].char_data[i].pos_x = pos_x_wasps;
                                client_wasps[index_client_wasps_id].char_data[i].pos_y = pos_y_wasps;
   
                                s_wasps = pthread_mutex_unlock(&mtx_wasps);
                                if (s_wasps != 0) {
                                    errExitEN(s_wasps, "pthread_mutex_unlock");
                                }

                                field_aux_main = display_in_field(ch, pos_x_wasps, pos_y_wasps, index_client, 
                                    index_bot, element_type, field[pos_x_wasps][pos_y_wasps], publisher);
                                /* Insert wasp into the playing field in new position */
                                
                                s_field = pthread_mutex_lock(&mtx_field);
                                if (s_field != 0) {
                                    errExitEN(s_field, "pthread_mutex_lock");
                                }
                                
                                field[pos_x_wasps][pos_y_wasps] = field_aux_main;

                                s_field = pthread_mutex_unlock(&mtx_field);
                                if (s_field != 0) {
                                    errExitEN(s_field, "pthread_mutex_unlock");
                                }
                            }
                            else if (check == 1) {
                                wasp_attack(field[pos_x_wasps_aux][pos_y_wasps_aux], publisher);
                            }
                        }
                    }
                }
            }
            else {
                /* Send unsuccessful response to client */
                ok = (int) '?';
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
        }
        else if (m->msg_type == 8) { /* Disconnection from wasp */
            
            /* Find wasp in client_wasps from its id */
            index_client_wasps_id = -1;
            for (jjj = 0; jjj < max_bots; jjj++) {
                if (client_wasps[jjj].id == m->id && client_wasps[jjj].valid) {
                    index_client_wasps_id = jjj;
                    break;
                }
            }

            success = disconnect_wasp(index_client_wasps_id, publisher);

            if (success) {
                /* Send successful response to client */
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
            else {
                /* Send unsuccessful response to client */
                ok = (int) '?';
                zmq_send_OkMessage(responder, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
        }

        /* Display lizards stats: name and score */
        // display_stats();

    }
}

void *thread_function_msg_lizards(void *arg) {
    RemoteChar *m;
    bool success;
    int ok = 1;
    int pos_x_lizards, pos_y_lizards;
    int pos_x_lizards_aux, pos_y_lizards_aux, rc, jjj;
    int index_client, index_bot, element_type, jj, check;
    int index_of_position_to_insert = -1, ch, index_client_lizards_id = -1;
    list_element *field_aux_main;
    double to_send;    

    void *responder2 = zmq_socket(context, ZMQ_REP);
    assert(responder2 != NULL);
    rc = zmq_connect(responder2, "inproc://back-end");
    assert(rc == 0);

    while (1) {
        /* Receive client message */
        // mvwprintw(debug_win, 3, 0, "liz aqui");
        // wrefresh(debug_win);

        m = zmq_read_RemoteChar(responder2);
        
        // mvwprintw(debug_win, 1, 0, "liz msg_type: %d", m->msg_type);
        // wrefresh(debug_win);

        // recv = zmq_recv (responder, &m, sizeof(remote_char_t), 0);
        // assert(recv != -1);        
        
        if (m->msg_type == 2) { /* Connection from lizard */
            /* Check if there are still lizard slots available */
            if (total_lizards + 1 > MAX_LIZARDS) {
                ok = (int) '?'; /* In case the pool is full of lizards */
                zmq_send_OkMessage(responder2, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
                continue;
            }

            /* Check if new client id is already in use */
            for (int iii = 0; iii < n_clients_roaches; iii++) {
                if (client_roaches[iii].id == m->id) {
                    ok = (int) '?';
                    zmq_send_OkMessage(responder2, ok);
                    // send = zmq_send (responder, &ok, sizeof(int), 0);
                    // assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }
            for (int iii = 0; iii < MAX_LIZARDS; iii++) {
                if (client_lizards[iii].valid && client_lizards[iii].id == m->id) {
                    ok = (int) '?';
                    zmq_send_OkMessage(responder2, ok);
                    // send = zmq_send (responder, &ok, sizeof(int), 0);
                    // assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }

            /* Find lizard slot available to insert new lizard */
            index_of_position_to_insert = -1;
            for (jj = 0; jj < MAX_LIZARDS; jj++) {
                if (!client_lizards[jj].valid) {
                    index_of_position_to_insert = jj;
                    break;
                }
            }
            if (index_of_position_to_insert != -1) {

                ok = (int) 'a' + index_of_position_to_insert;

                /* Send lizard attributed name to client */
                zmq_send_OkMessage(responder2, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;

                /* Initialize lizard attributes */

                s_lizards = pthread_mutex_lock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_lock");
                }

                client_lizards[index_of_position_to_insert].id = m->id;
                client_lizards[index_of_position_to_insert].score = 0;
                client_lizards[index_of_position_to_insert].valid = true;
                client_lizards[index_of_position_to_insert].alive = true;
                client_lizards[index_of_position_to_insert].previous_interaction = s_clock();
                assert(client_lizards[index_of_position_to_insert].previous_interaction >= 0);
                
                s_lizards = pthread_mutex_unlock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_unlock");
                }

                /* Compute lizard initial random position */
                while (1) {
                    s_lizards = pthread_mutex_lock(&mtx_lizards);
                    if (s_lizards != 0) {
                        errExitEN(s_lizards, "pthread_mutex_lock");
                    }

                    client_lizards[index_of_position_to_insert].char_data.pos_x =  rand() % (WINDOW_SIZE - 2) + 1;
                    client_lizards[index_of_position_to_insert].char_data.pos_y =  rand() % (WINDOW_SIZE - 2) + 1;

                    s_lizards = pthread_mutex_unlock(&mtx_lizards);
                    if (s_lizards != 0) {
                        errExitEN(s_lizards, "pthread_mutex_unlock");
                    }

                    /* Verify if new position matches the position of anothers' head lizard or wasp */
                    if (check_in_square(field[client_lizards[index_of_position_to_insert].char_data.pos_x]
                        [client_lizards[index_of_position_to_insert].char_data.pos_y], 1) == -1) {
                        break;
                    }
                }

                s_lizards = pthread_mutex_lock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_lock");
                }

                client_lizards[index_of_position_to_insert].char_data.ch = (char) ((int) 'a' + index_of_position_to_insert);
                
                s_lizards = pthread_mutex_unlock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_unlock");
                }

                pos_x_lizards = client_lizards[index_of_position_to_insert].char_data.pos_x;
                pos_y_lizards = client_lizards[index_of_position_to_insert].char_data.pos_y;
                ch = client_lizards[index_of_position_to_insert].char_data.ch;

                /* Initial direction is random */
                m->direction[0] = rand() % 4;
                
                /* Insert lizard tail into the playing field */
                tail(m->direction[0], pos_x_lizards, pos_y_lizards, 
                    false, index_of_position_to_insert, publisher);

                s_lizards = pthread_mutex_lock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_lock");
                }

                client_lizards[index_of_position_to_insert].prevdirection = m->direction[0];

                s_lizards = pthread_mutex_unlock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_unlock");
                }

                index_client = index_of_position_to_insert;
                index_bot = -1;  
                element_type = 1;

                field_aux_main = display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                    index_bot, element_type, field[pos_x_lizards][pos_y_lizards], publisher);

                /* Insert lizard into the playing field */
                s_field = pthread_mutex_lock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_lock");
                }

                field[pos_x_lizards][pos_y_lizards] = field_aux_main;

                s_field = pthread_mutex_unlock(&mtx_field);
                if (s_field != 0) {
                    errExitEN(s_field, "pthread_mutex_unlock");
                }

                s_lizards = pthread_mutex_lock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_lock");
                }
                
                total_lizards++;

                s_lizards = pthread_mutex_unlock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_unlock");
                }
            }
            else {
                /* Send unsuccessful response to client */
                ok = (int) '?';
                zmq_send_OkMessage(responder2, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
        } 
        else if (m->msg_type == 3) { /* Movement from lizard */
            /* Find lizard in client_lizards from its id */
            index_client_lizards_id = -1;
            for (jjj = 0; jjj < MAX_LIZARDS; jjj++) {
                if (client_lizards[jjj].id == m->id && client_lizards[jjj].valid && client_lizards[jjj].alive) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }
            if (index_client_lizards_id != -1) {
                s_lizards = pthread_mutex_lock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_lock");
                }

                client_lizards[index_client_lizards_id].previous_interaction = s_clock();
                assert(client_lizards[index_client_lizards_id].previous_interaction >= 0);

                s_lizards = pthread_mutex_unlock(&mtx_lizards);
                if (s_lizards != 0) {
                    errExitEN(s_lizards, "pthread_mutex_unlock");
                }

                pos_x_lizards = client_lizards[index_client_lizards_id].char_data.pos_x;
                pos_y_lizards = client_lizards[index_client_lizards_id].char_data.pos_y;
                ch = client_lizards[index_client_lizards_id].char_data.ch;


                pos_x_lizards_aux = pos_x_lizards;
                pos_y_lizards_aux = pos_y_lizards;

                /* Compute lizard new position */
                new_position(&pos_x_lizards_aux, &pos_y_lizards_aux, m->direction[0]);
                
                /* Check if new position is inside the playing field */
                if (!(pos_x_lizards_aux < 0 || pos_y_lizards_aux < 0  || pos_x_lizards_aux >= WINDOW_SIZE || pos_y_lizards_aux  >= WINDOW_SIZE)) {
                    /* Verify if new position matches the position of anothers' head lizard */
                    check = check_in_square(field[pos_x_lizards_aux][pos_y_lizards_aux], 1);
                    if (check == -1) {

                        /* Remove lizard tail from the playing field in the old position */
                        tail(client_lizards[index_client_lizards_id].prevdirection, pos_x_lizards, pos_y_lizards, 
                            true, index_client_lizards_id, publisher);

                        /* Insert lizard tail into the playing field in new position */
                        tail(m->direction[0], pos_x_lizards_aux, pos_y_lizards_aux, false, 
                            index_client_lizards_id, publisher);

                        index_client = index_client_lizards_id;
                        index_bot = -1;  
                        element_type = 1;              

                        field_aux_main = display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                            index_bot, element_type, field[pos_x_lizards][pos_y_lizards], publisher);

                        /* Remove lizard from the playing field in old position */
                        
                        s_field = pthread_mutex_lock(&mtx_field);
                        if (s_field != 0) {
                            errExitEN(s_field, "pthread_mutex_lock");
                        }
                        
                        field[pos_x_lizards][pos_y_lizards] = field_aux_main;

                        s_field = pthread_mutex_unlock(&mtx_field);
                        if (s_field != 0) {
                            errExitEN(s_field, "pthread_mutex_unlock");
                        }

                        /* Calculates new mark position */
                        pos_x_lizards = pos_x_lizards_aux;
                        pos_y_lizards = pos_y_lizards_aux;

                        s_lizards = pthread_mutex_lock(&mtx_lizards);
                        if (s_lizards != 0) {
                            errExitEN(s_lizards, "pthread_mutex_lock");
                        }
                        client_lizards[index_client_lizards_id].char_data.pos_x = pos_x_lizards;
                        client_lizards[index_client_lizards_id].char_data.pos_y = pos_y_lizards;

                        s_lizards = pthread_mutex_unlock(&mtx_lizards);
                        if (s_lizards != 0) {
                            errExitEN(s_lizards, "pthread_mutex_unlock");
                        }

                        field_aux_main = display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                            index_bot, element_type, field[pos_x_lizards][pos_y_lizards], publisher);

                        /* Insert lizard into the playing field in new position */
                        
                        s_field = pthread_mutex_lock(&mtx_field);
                        if (s_field != 0) {
                            errExitEN(s_field, "pthread_mutex_lock");
                        }
                        
                        field[pos_x_lizards][pos_y_lizards] = field_aux_main;
                            
                        s_field = pthread_mutex_unlock(&mtx_field);
                        if (s_field != 0) {
                            errExitEN(s_field, "pthread_mutex_unlock");
                        }

                        s_lizards = pthread_mutex_lock(&mtx_lizards);
                        if (s_lizards != 0) {
                            errExitEN(s_lizards, "pthread_mutex_lock");
                        }
                        
                        client_lizards[index_client_lizards_id].prevdirection = m->direction[0];

                        s_lizards = pthread_mutex_unlock(&mtx_lizards);
                        if (s_lizards != 0) {
                            errExitEN(s_lizards, "pthread_mutex_unlock");
                        }

                        /* Check if lizard won the game */
                        if (client_lizards[index_client].score >= POINTS_TO_WIN && end_game == 0) {
                            /* Update lizard tail to '*' and flag the game as finished */
                            end_game = 1;
                            tail(m->direction[0], pos_x_lizards, pos_y_lizards, false, 
                                index_client_lizards_id, publisher);

                            end_game = 2;
                        }
                    }
                    else if (check == 1) {
                        /* In case lizard new position matches the position of anothers' head lizard, average their scores */
                        split_health(field[pos_x_lizards_aux][pos_y_lizards_aux], index_client_lizards_id);
                    }
                    else { // wasp
                        
                        s_lizards = pthread_mutex_lock(&mtx_lizards);
                        if (s_lizards != 0) {
                            errExitEN(s_lizards, "pthread_mutex_lock");
                        }

                        client_lizards[index_client_lizards_id].score -= 10;

                        s_lizards = pthread_mutex_unlock(&mtx_lizards);
                        if (s_lizards != 0) {
                            errExitEN(s_lizards, "pthread_mutex_unlock");
                        }

                        if (client_lizards[index_client_lizards_id].score < 0) {
                            loser_lizard(index_client_lizards_id, publisher);
                        }
                    }
                    
                }
                /* Send lizard score to client */
                zmq_send_MyScore(responder2, client_lizards[index_client_lizards_id].score);
                // send = zmq_send (responder, &client_lizards[index_client_lizards_id].score, sizeof(double), 0);
                // assert(send != -1);
            }
            else {
                to_send = -1000;
                // send = zmq_send (responder, &to_send, sizeof(double), 0);
                // assert(send != -1);
                zmq_send_MyScore(responder2, to_send);
            }

        }
        else if (m->msg_type == 4) { /* Disconnection from lizard */

            /* Find lizard in client_lizards from its id */
            index_client_lizards_id = -1;
            for (jjj = 0; jjj < MAX_LIZARDS; jjj++) {
                if (client_lizards[jjj].id == m->id && client_lizards[jjj].valid) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }

            success = disconnect_lizard(index_client_lizards_id, publisher);

            if (!success) {
                /* Send successful response to client */
                zmq_send_OkMessage(responder2, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
            else {
                /* Send unsuccessful response to client */
                ok = (int) '?';
                zmq_send_OkMessage(responder2, ok);
                // send = zmq_send (responder, &ok, sizeof(int), 0);
                // assert(send != -1);
                ok = 1;
            }
        } 

        /* Display lizards stats: name and score */
        // display_stats();

    }
    zmq_close(responder2);
    
}

int main(int argc, char *argv[]) {
    char *port_display, *port_client_lizard, *port_client_rNw;
    
    size_t bufsize = 100; //send; //, recv;
    struct termios oldt, newt;
    int rc, rc2, i, j, ch;

    max_bots = floor(((WINDOW_SIZE - 2) * (WINDOW_SIZE - 2)) / 3);

    roaches_killed = NULL;

    n_clients_roaches = 0; 
    n_clients_wasps = 0; 
    total_roaches = 0; 
    total_lizards = 0;
    total_wasps = 0;

    s_lizards = pthread_mutex_init(&mtx_lizards, NULL);
    if (s_lizards != 0) {
        errExitEN(s_lizards, "pthread_mutex_init");
    }

    s_publish = pthread_mutex_init(&mtx_publish, NULL);
    if (s_publish != 0) {
        errExitEN(s_publish, "pthread_mutex_init");
    }

    s_wasps = pthread_mutex_init(&mtx_wasps, NULL);
    if (s_wasps != 0) {
        errExitEN(s_wasps, "pthread_mutex_init");
    }

    s_roaches = pthread_mutex_init(&mtx_roaches, NULL);
    if (s_roaches != 0) {
        errExitEN(s_roaches, "pthread_mutex_init");
    }

    s_field = pthread_mutex_init(&mtx_field, NULL);
    if (s_field != 0) {
        errExitEN(s_field, "pthread_mutex_init");
    }
    
    /* Check if the correct number of arguments is provided */
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client-lizard-port> <client-roach-wasp-port> <display-port>\n", argv[0]);
        return 1;
    }

    end_game = 0;

    field = allocate3DArray();
    if (field == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_exit();
        exit(0);
    }

    port_client_lizard = argv[1];
    /* Validate the port client number */
    if (strlen(port_client_lizard) > 5 || atoi(port_client_lizard) <= 0 || atoi(port_client_lizard) > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        free_exit();
        exit(0);
    }
    /* Format the full address for the client */
    snprintf(full_address_client_lizard, sizeof(full_address_client_lizard), "tcp://*:%s", port_client_lizard);

    port_client_rNw = argv[2];
    /* Validate the port client number */
    if (strlen(port_client_rNw) > 5 || atoi(port_client_rNw) <= 0 || atoi(port_client_rNw) > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        free_exit();
        exit(0);
    }
    /* Format the full address for the client */
    snprintf(full_address_client_rNw, sizeof(full_address_client_rNw), "tcp://*:%s", port_client_rNw);

    port_display = argv[3];
    /* Validate the port number */
    if (strlen(port_display) > 5 || atoi(port_display) <= 0 || atoi(port_display) > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        free_exit();
        exit(0);
    }
    /* Format the full address for the display */
    snprintf(full_address_display, sizeof(full_address_display), "tcp://*:%s", port_display);

    context = zmq_ctx_new ();
    assert(context != NULL);

    // Socket facing clients
    frontend = zmq_socket(context, ZMQ_ROUTER);
    rc = zmq_bind(frontend, full_address_client_lizard);
    assert(rc == 0);

    // Socket facing services
    backend = zmq_socket(context, ZMQ_DEALER); // communication with lizards REQ/REP
    rc2 = zmq_bind(backend, "inproc://back-end");
    assert(rc2 == 0);

    // responder2 = zmq_socket(context, ZMQ_REP); // communication with roaches and wasps
    // assert(responder2 != NULL);
    // rc = zmq_bind(responder2, full_address_client_lizard);
    // assert(rc == 0);

    responder = zmq_socket(context, ZMQ_REP); // communication with roaches and wasps
    assert(responder != NULL);
    int rc8 = zmq_bind(responder, full_address_client_rNw);
    assert(rc8 == 0);

    publisher = zmq_socket(context, ZMQ_PUB); // display-app to lizards
    assert(publisher != NULL);
    rc2 = zmq_bind(publisher, full_address_display);
    assert(rc2 == 0);

    /* Initialize message regarding communication with display-app */
    msg_publisher.x_upd = -1;
    msg_publisher.y_upd = -1;
    for (i = 0; i < WINDOW_SIZE; i++) {
        for (int j = 0; j < WINDOW_SIZE; j++) {
            msg_publisher.field[i][j] = ' ';
        }
    }

    initscr();		    	
	cbreak();				
    keypad(stdscr, true);   
	noecho();	

    password = NULL;
    password = (char *) malloc(bufsize * sizeof(char));
    if (password == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        free_exit();
        exit(0);
    }

    /* Turn off echoing of characters */
    tcgetattr(STDIN_FILENO, &oldt); /* Get current terminal attributes */
    newt = oldt;
    newt.c_lflag &= ~(ECHO); /* Turn off ECHO */
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    /* Ask user a password for the subscribers of the display-app */
    printw("Broadcast password: ");
    refresh();
    i = 0; 
    while(i < 99 && (ch = getch()) != '\n') {
        password[i++] = ch;
    }
    password[i] = '\0'; /* Null-terminate the string */

    /* Publish message to the subscribers regarding the initial display */
    // send1 = zmq_send(publisher, password, strlen(password), ZMQ_SNDMORE);
    // assert(send1 != -1);
    // send2 = zmq_send(publisher, &msg_publisher, sizeof(msg), 0);  
    // assert(send2 != -1);

    /* Restore terminal settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);	    

    /* Creates a window and draws a border */
    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0, 0);
	wrefresh(my_win);

    /* Creates a window for stats */
    stats_win = newwin(MAX_LIZARDS + 1, 100, WINDOW_SIZE + 1, 0);

    /* Creates a window for debug */
    debug_win = newwin(20, 100, WINDOW_SIZE + 1 + MAX_LIZARDS + 1, 0);
    
    /* In worst case scenario each client has one roach and there are max_bots */
    client_roaches = (pos_roachesNwasps*) calloc(max_bots, sizeof(pos_roachesNwasps));
    if (client_roaches == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_exit();
        exit(0);
    }

    /* In worst case scenario each client has one wasp and there are max_bots */
    client_wasps = (pos_roachesNwasps*) calloc(max_bots, sizeof(pos_roachesNwasps));
    if (client_wasps == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_exit();
        exit(0);
    }

    srand(time(NULL));

    /* Initialize client_lizards, client_roaches and client_wasps */
    for (i = 0; i < MAX_LIZARDS; i++) {
        client_lizards[i].id = -1;
        client_lizards[i].valid = false;
        client_lizards[i].alive = false;
        client_lizards[i].score = 0;
        client_lizards[i].char_data.ch = '?';
    }
    for (i = 0; i < max_bots; i++) {
        client_roaches[i].id = -1;
        client_roaches[i].nchars = 0;
        for (j = 0; j < MAX_ROACHES_PER_CLIENT; j++) {
            client_roaches[i].active[j] = false;
        } 
        client_roaches[i].valid = false;
    }
    for (i = 0; i < max_bots; i++) {
        client_wasps[i].id = -1;
        client_wasps[i].nchars = 0;
        for (j = 0; j < MAX_WASPS_PER_CLIENT; j++) {
            client_wasps[i].active[j] = false;
        } 
        client_wasps[i].valid = false;
    }

    pthread_t thread_id[N_THREADS];
    // pthread_create(&thread_id[0], NULL, thread_function, NULL);
    // pthread_create(&thread_id[1], NULL, thread_function_msg_wasp_roaches, NULL);
    // pthread_create(&thread_id[2], NULL, thread_function_msg_lizards, NULL);
    // pthread_create(&thread_id[3], NULL, thread_function_msg_lizards, NULL);
    // pthread_create(&thread_id[4], NULL, thread_function_display, NULL);


    for (int worker_nbr = 0; worker_nbr < N_THREADS - 1; worker_nbr++) {
        if(worker_nbr == 0) {
            pthread_create(&thread_id[worker_nbr], NULL, thread_function, NULL);
        }
        else if(worker_nbr == 1) {
            pthread_create(&thread_id[worker_nbr], NULL, thread_function_msg_wasp_roaches, NULL);
        }
        else if(worker_nbr == 2) {
            pthread_create(&thread_id[worker_nbr], NULL, thread_function_display, NULL);
        }
        else {
            pthread_create(&thread_id[worker_nbr], NULL, thread_function_msg_lizards, NULL);
        }
    }

    zmq_proxy(frontend, backend, NULL);

    for(int jjjj = 0; jjjj < N_THREADS - 1; jjjj++) {
        pthread_join(thread_id[jjjj], NULL);
    }

    // pthread_join(thread_id[0], NULL);
    // pthread_join(thread_id[1], NULL);
    // pthread_join(thread_id[2], NULL);
    // pthread_join(thread_id[3], NULL);
    // pthread_join(thread_id[4], NULL);
    
    s_lizards = pthread_mutex_destroy(&mtx_lizards);
    if (s_lizards != 0) {
        errExitEN(s_lizards, "pthread_mutexattr_destroy");
    }

    s_roaches = pthread_mutex_destroy(&mtx_roaches);
    if (s_roaches != 0) {
        errExitEN(s_roaches, "pthread_mutexattr_destroy");
    }

    s_wasps = pthread_mutex_destroy(&mtx_wasps);
    if (s_wasps != 0) {
        errExitEN(s_wasps, "pthread_mutexattr_destroy");
    }

    s_field = pthread_mutex_destroy(&mtx_field);
    if (s_field != 0) {
        errExitEN(s_field, "pthread_mutexattr_destroy");
    }

    s_publish = pthread_mutex_destroy(&mtx_publish);
    if (s_publish != 0) {
        errExitEN(s_publish, "pthread_mutexattr_destroy");
    }

    free_exit();
    
    return 0;
}


