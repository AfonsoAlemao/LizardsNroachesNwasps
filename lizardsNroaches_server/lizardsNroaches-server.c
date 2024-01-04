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

WINDOW *my_win;
WINDOW *debug_win;
WINDOW *stats_win;
list_element ***field;
fifo_element *roaches_killed;
pos_roaches *client_roaches;
pos_lizards client_lizards[MAX_LIZARDS];

#define MAX_PORT_STR_LEN 6 /* Max port number "65535" plus null terminator */
#define ADDRESS_PREFIX_LEN 10 // Length of "tcp://*:" including null terminator
#define FULL_ADDRESS_LEN (MAX_PORT_STR_LEN + ADDRESS_PREFIX_LEN)

int end_game;
void *context;
void *responder;
void *publisher;
msg msg_publisher;
char *password = NULL;

void new_position(int* x, int *y, direction_t direction);
void split_health(list_element *head, int index_client);
void search_and_destroy_roaches(list_element *head, int index_client);
list_element *display_in_field(char ch, int x, int y, int index_client, int index_roaches, int element_type, list_element *head);
void tail(direction_t direction, int x, int y, bool delete, int index_client);
bool check_head_in_square(list_element *head);
char check_prioritary_element(list_element *head);
void *free_safe (void *aux);
list_element ***allocate3DArray();
void free3DArray(list_element ***table);
void ressurect_roaches();
void free_exit();

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

    client_lizards[index_client].score = avg;
    client_lizards[current->data.index_client].score = avg;
    
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

                /* Increase lizard score */
                client_lizards[index_client].score += client_roaches[current->data.index_client].char_data[current->data.index_roaches].ch - '0';

                /* Disable cockroach */
                client_roaches[current->data.index_client].active[current->data.index_roaches] = false;

                /* Insert roach in inactive roaches list with associated death time */
                r.death_time = s_clock();
                assert(r.death_time >= 0);
                r.index_client = current->data.index_client;
                r.index_roaches = current->data.index_roaches;
                check = push_fifo(&roaches_killed, r);
                if (!check) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free_exit();
                    exit(0);
                }

                /* Remove roach from field */
                s.element_type = current->data.element_type;
                s.index_client = current->data.index_client;
                s.index_roaches = current->data.index_roaches;
                deletelist_element(&head, s);
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
        msg_publisher.lizards[i].score = client_lizards[i].score;
        msg_publisher.lizards[i].char_data.ch = client_lizards[i].char_data.ch;
    }
    return;
}

/* Deal with the display in field of an element in a new position */
list_element *display_in_field(char ch, int x, int y, int index_client, 
                    int index_roaches, int element_type, list_element *head) {
    square new_data;
    size_t send1, send2;
    bool check;

    /* The display field becomes static after the game ends */          
    if (end_game < 2) {
        new_data.element_type = element_type;
        new_data.index_client = index_client;
        new_data.index_roaches = index_roaches;
        if (ch == ' ') {
            /* Delete from list position xy in field */
            deletelist_element(&head, new_data);
            if (head != NULL) {
                /* Display an empty space or a previously hidden element in position xy*/
                ch = check_prioritary_element(head); 
            }
        }
        else {
            /* Add to list position xy in field */
            check = insertBegin(&head, new_data);
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
        wmove(my_win, x, y);
        waddch(my_win, ch | A_BOLD);
        wrefresh(my_win);

        /* Publish message to the subscribers regarding the previous display update */
        msg_publisher.field[x][y] = ch;
        msg_publisher.x_upd = x;
        msg_publisher.y_upd = y;
        update_lizards();
        send1 = zmq_send(publisher, password, strlen(password), ZMQ_SNDMORE);
        assert(send1 != -1);
        send2 = zmq_send(publisher, &msg_publisher, sizeof(msg), 0);  
        assert(send2 != -1);
    }
    return head;
}

/* Insert tail in new position of the playing field */
void tail(direction_t direction, int x, int y, bool delete, int index_client) {
    char display;
    int index_roaches = -1;  
    int element_type = 0;

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
                field[x][y + kk] = display_in_field(display, x, y + kk, index_client, 
                    index_roaches, element_type, field[x][y + kk]);
            }
        }
        break;

    case RIGHT:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (y - kk > 1) {
                field[x][y - kk] = display_in_field(display, x, y - kk, index_client, 
                    index_roaches, element_type, field[x][y - kk]);
            }
        }
        break;

    case DOWN:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (x - kk > 1) {
                field[x - kk][y] = display_in_field(display, x - kk, y, index_client, 
                    index_roaches, element_type, field[x - kk][y]);
            }
        }
        break;

    case UP:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (x + kk < WINDOW_SIZE - 1) {
                field[x + kk][y] = display_in_field(display, x + kk, y, index_client, 
                    index_roaches, element_type, field[x + kk][y]);
            }
        }
        break;
        
    default:
        break;
    }
}

/* Check if there is a head in a square */
bool check_head_in_square(list_element *head) {
    list_element *current = head;
    list_element *nextNode;
    while (current != NULL) {
        if (current->data.element_type == 1) { /* Head of lizard */
            return true;
        }
        nextNode = current->next;
        current = nextNode;
    }
    return false;
}

/* Regarding a field position, check which element in that position is the most prioritary:
Priority order: lizard head, roach, tail */
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
        else if (current->data.element_type == 2 && (winner_type == -1 || winner_type == 0)) {
            winner = client_roaches[current->data.index_client].char_data[current->data.index_roaches].ch;
            winner_type = 2;    
        }
        else if (current->data.element_type == 0 && winner_type == -1) {
            if (end_game == 1) {
                winner = '*';
            }
            else {
                winner = '.';
            }
            winner_type = 0;
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
            client_roaches[roaches_killed->data.index_client].active[roaches_killed->data.index_roaches] = true;
        
            /* Compute its new random field position */
            while (1) {
                client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                /* Verify if new position matches the position of anothers' head lizard */
                if (check_head_in_square(field[client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_x]
                    [client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_y]) == false) {
                    break;
                }
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
            mvwprintw(stats_win, j, 1, "\t\t\t\t\t");
            wrefresh(stats_win);
            if (client_lizards[j].valid) {
                mvwprintw(stats_win, i, 1, "Player: %c, Score: %lf", client_lizards[j].char_data.ch, client_lizards[j].score);
                wrefresh(stats_win);
                i++;
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
    zmq_close (responder);
    zmq_close(publisher);
    int rc = zmq_ctx_destroy (context);
    assert(rc == 0);
    client_roaches = free_safe(client_roaches);
    free3DArray(field);
    password = free_safe(password);
}

int main(int argc, char *argv[]) {
    char *port_display, *port_client;
    char full_address_display[FULL_ADDRESS_LEN], full_address_client[FULL_ADDRESS_LEN];
    remote_char_t m;
    size_t send1, send2, bufsize = 100, send, recv;
    struct termios oldt, newt;
    int rc, rc2, i, j, ch, jj = 0;
    int max_roaches = floor(((WINDOW_SIZE - 2) * (WINDOW_SIZE - 2)) / 3);
    int n_clients_roaches = 0, total_roaches = 0, total_lizards = 0;
    int ok = 1, index_client, index_roaches, element_type, index_of_position_to_insert;
    int pos_x_roaches, pos_x_lizards, pos_y_roaches, pos_y_lizards;
    int pos_x_roaches_aux, pos_y_roaches_aux, pos_x_lizards_aux, pos_y_lizards_aux;
    uint32_t index_client_roaches_id, index_client_lizards_id;

    /* Check if the correct number of arguments is provided */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <client-port> <display-port>\n", argv[0]);
        return 1;
    }

    end_game = 0;

    field = allocate3DArray();
    if (field == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_exit();
        exit(0);
    }

    port_client = argv[1];
    /* Validate the port client number */
    if (strlen(port_client) > 5 || atoi(port_client) <= 0 || atoi(port_client) > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        free_exit();
        exit(0);
    }
    /* Format the full address for the client */
    snprintf(full_address_client, sizeof(full_address_client), "tcp://*:%s", port_client);

    port_display = argv[2];
    /* Validate the port number */
    if (strlen(port_display) > 5 || atoi(port_display) <= 0 || atoi(port_display) > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        free_exit();
        exit(0);
    }
    /* Format the full address for the display */
    snprintf(full_address_display, sizeof(full_address_display), "tcp://*:%s", port_display);

    printf("%s, %s", full_address_client, full_address_display);

    context = zmq_ctx_new ();
    assert(context != NULL);
    responder = zmq_socket (context, ZMQ_REP);
    assert(responder != NULL);
    rc = zmq_bind (responder, full_address_client);
    assert (rc == 0);

    publisher = zmq_socket (context, ZMQ_PUB);
    assert(publisher != NULL);
    rc2 = zmq_bind (publisher, full_address_display);
    assert(rc2 == 0);

    /* Initialize message regarding communication with display-app */
    msg_publisher.x_upd = -1;
    msg_publisher.y_upd = -1;
    for (i = 0; i < WINDOW_SIZE; i++) {
        for (int j = 0; j < WINDOW_SIZE; j++) {
            msg_publisher.field[i][j] = ' ';
        }
    }
    j = 0;

    initscr();		    	
	cbreak();				
    keypad(stdscr, true);   
	noecho();	

    password = NULL;
    password = (char *)malloc(bufsize * sizeof(char));
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
    printw("Display-app pass: ");
    refresh();
    i = 0; 
    while(i < 99 && (ch = getch()) != '\n') {
        password[i++] = ch;
    }
    password[i] = '\0'; /* Null-terminate the string */

    /* Publish message to the subscribers regarding the initial display */
    send1 = zmq_send(publisher, password, strlen(password), ZMQ_SNDMORE);
    assert(send1 != -1);
    send2 = zmq_send(publisher, &msg_publisher, sizeof(msg), 0);  
    assert(send2 != -1);

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
    
    /* In worst case scenario each client has one roach and there are max_roaches */
    client_roaches = (pos_roaches*) calloc(max_roaches, sizeof(pos_roaches));
    if (client_roaches == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_exit();
        exit(0);
    }

    srand(time(NULL));

    /* Initialize client_lizards and client_roaches */
    for (i = 0; i < MAX_LIZARDS; i++) {
        client_lizards[i].id = -1;
        client_lizards[i].valid = false;
        client_lizards[i].score = 0;
        client_lizards[i].char_data.ch = '?';
        
    }
    for (i = 0; i < max_roaches; i++) {
        client_roaches[i].id = -1;
        client_roaches[i].nChars = 0;
        for (j = 0; j < MAX_ROACHES_PER_CLIENT; j++) {
            client_roaches[i].active[j] = false;
        } 
    }

    while (1) {
        /* Deal with ressurection of dead roaches (after respawn time) */
        ressurect_roaches(client_roaches);

        /* Receive roach or lizard message */
        recv = zmq_recv (responder, &m, sizeof(remote_char_t), 0);
        assert(recv != -1);        

        if (m.msg_type == 0) { /* Connection from roach client */
            /* Testing if roach max number allows new roach clien */
            if (m.nChars + total_roaches > max_roaches) {
                ok = 0;
                send = zmq_send (responder, &ok, sizeof(int), 0);
                assert(send != -1);
                ok = 1;
                continue;
            }
            /* Check if new client id is already in use */
            for (int iii = 0; iii < n_clients_roaches; iii++) {
                if (client_roaches[iii].id == m.id) {
                    ok = (int) '?';
                    send = zmq_send (responder, &ok, sizeof(int), 0);
                    assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }
            for (int iii = 0; iii < MAX_LIZARDS; iii++) {
                if (client_lizards[iii].valid && client_lizards[iii].id == m.id) {
                    ok = (int) '?';
                    send = zmq_send (responder, &ok, sizeof(int), 0);
                    assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }

            /* Send successful response to client */
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;

            /* Find roach client slot available to insert a new one */
            index_of_position_to_insert = -1;
            for (jj = 0; jj < MAX_ROACHES_PER_CLIENT; jj++) {
                if (!client_roaches[jj].valid) {
                    index_of_position_to_insert = jj;
                    break;
                }
            }
            
            /* Roaches are never destroyed, so insert in n_client_roaches index */
            client_roaches[index_of_position_to_insert].id = m.id;
            client_roaches[index_of_position_to_insert].nChars = m.nChars;
            client_roaches[index_of_position_to_insert].valid = true;

            for (i = 0; i < m.nChars; i++) {
                /* Insert in server each roach information */
                client_roaches[index_of_position_to_insert].char_data[i].ch = m.ch[i];
                client_roaches[index_of_position_to_insert].active[i] = true;

                /* Compute roach initial random position */
                while (1) {
                    client_roaches[index_of_position_to_insert].char_data[i].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                    client_roaches[index_of_position_to_insert].char_data[i].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                    /* Verify if new position matches the position of anothers' head lizard */
                    if (check_head_in_square(field[client_roaches[index_of_position_to_insert].char_data[i].pos_x][ 
                        client_roaches[index_of_position_to_insert].char_data[i].pos_y]) == false) {
                        break;
                    }
                }
                
                pos_x_roaches = client_roaches[index_of_position_to_insert].char_data[i].pos_x;
                pos_y_roaches = client_roaches[index_of_position_to_insert].char_data[i].pos_y;
                ch = client_roaches[index_of_position_to_insert].char_data[i].ch;

                index_client = index_of_position_to_insert;
                index_roaches = i;  
                element_type = 2;

                /* Insert roach into the playing field */
                field[pos_x_roaches][pos_y_roaches] = display_in_field(ch, pos_x_roaches, pos_y_roaches, index_client, 
                    index_roaches, element_type, field[pos_x_roaches][pos_y_roaches]);   
            }
            /* Increase number of client_roaches and number of roaches*/
            n_clients_roaches++;
            total_roaches += m.nChars;
        }
        else if (m.msg_type == 1) { /* Movements from roach client */
            /* Send successful response to client */
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;

            /* Find roach in client_roaches from its id */
            index_client_roaches_id = 0;
            for (int jjj = 0; jjj < n_clients_roaches;jjj++) {
                if (client_roaches[jjj].id == m.id) {
                    index_client_roaches_id = jjj;
                    break;
                }
            }

            for (i = 0; i < m.nChars; i++) {
                if (client_roaches[index_client_roaches_id].active[i] == true) {
                    /* For each active roach, compute its new position */
                    pos_x_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_x;
                    pos_y_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_y;
                    ch = client_roaches[index_client_roaches_id].char_data[i].ch;
                    pos_x_roaches_aux = pos_x_roaches;
                    pos_y_roaches_aux = pos_y_roaches;
                    new_position(&pos_x_roaches_aux, &pos_y_roaches_aux, m.direction[i]);

                    /* Check if new position is inside the playing field */
                    if (!(pos_x_roaches_aux < 0 || pos_y_roaches_aux < 0  || pos_x_roaches_aux >= WINDOW_SIZE || pos_y_roaches_aux  >= WINDOW_SIZE)) {
                        /* Verify if new position matches the position of anothers' head lizard */
                        if (check_head_in_square(field[pos_x_roaches_aux][pos_y_roaches_aux]) == false) {
                            
                            index_client = index_client_roaches_id;
                            index_roaches = i;  
                            element_type = 2;

                            /* Remove roach from the playing field in old position */
                            field[pos_x_roaches][pos_y_roaches] = display_in_field(' ', pos_x_roaches, pos_y_roaches, index_client, 
                                index_roaches, element_type, field[pos_x_roaches][pos_y_roaches]);

                            /* Updates roach position */
                            pos_x_roaches = pos_x_roaches_aux;
                            pos_y_roaches = pos_y_roaches_aux;
                            client_roaches[index_client_roaches_id].char_data[i].pos_x = pos_x_roaches;
                            client_roaches[index_client_roaches_id].char_data[i].pos_y = pos_y_roaches;

                            /* Insert roach into the playing field in new position */
                            field[pos_x_roaches][pos_y_roaches] = display_in_field(ch, pos_x_roaches, pos_y_roaches, index_client, 
                                index_roaches, element_type, field[pos_x_roaches][pos_y_roaches]);
                        }
                    }
                }
            }
        }
        else if (m.msg_type == 2) { /* Connection from lizard */
            /* Check if there are still lizard slots available */
            if (total_lizards + 1 > MAX_LIZARDS) {
                ok = (int) '?'; /* In case the pool is full of lizards */
                send = zmq_send (responder, &ok, sizeof(int), 0);
                assert(send != -1);
                ok = 1;
                continue;
            }

            /* Check if new client id is already in use */
            for (int iii = 0; iii < n_clients_roaches; iii++) {
                if (client_roaches[iii].id == m.id) {
                    ok = (int) '?';
                    send = zmq_send (responder, &ok, sizeof(int), 0);
                    assert(send != -1);
                    break;
                }
            }
            if (ok == (int) '?') {
                ok = 1;
                continue;
            }
            for (int iii = 0; iii < MAX_LIZARDS; iii++) {
                if (client_lizards[iii].valid && client_lizards[iii].id == m.id) {
                    ok = (int) '?';
                    send = zmq_send (responder, &ok, sizeof(int), 0);
                    assert(send != -1);
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
            ok = (int) 'a' + index_of_position_to_insert;

            /* Send lizard attributed name to client */
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;

            /* Initialize lizard attributes */
            client_lizards[index_of_position_to_insert].id = m.id;
            client_lizards[index_of_position_to_insert].score = 0;
            client_lizards[index_of_position_to_insert].valid = true;

            /* Compute lizard initial random position */
            while (1) {
                client_lizards[index_of_position_to_insert].char_data.pos_x =  rand() % (WINDOW_SIZE - 2) + 1;
                client_lizards[index_of_position_to_insert].char_data.pos_y =  rand() % (WINDOW_SIZE - 2) + 1;

                /* Verify if new position matches the position of anothers' head lizard */
                if (check_head_in_square(field[client_lizards[index_of_position_to_insert].char_data.pos_x]
                    [client_lizards[index_of_position_to_insert].char_data.pos_y]) == false) {
                    break;
                }
            }

            client_lizards[index_of_position_to_insert].char_data.ch = (char) ((int) 'a' + index_of_position_to_insert);

            pos_x_lizards = client_lizards[index_of_position_to_insert].char_data.pos_x;
            pos_y_lizards = client_lizards[index_of_position_to_insert].char_data.pos_y;
            ch = client_lizards[index_of_position_to_insert].char_data.ch;

            /* Initial direction is random */
            m.direction[0] = rand() % 4;
            
            /* Insert lizard tail into the playing field */
            tail(m.direction[0], pos_x_lizards, pos_y_lizards, 
                false, index_of_position_to_insert);

            client_lizards[index_of_position_to_insert].prevdirection = m.direction[0];

            index_client = index_of_position_to_insert;
            index_roaches = -1;  
            element_type = 1;

            /* Insert lizard into the playing field */
            field[pos_x_lizards][pos_y_lizards] = display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);

            total_lizards++;
        } 
        else if (m.msg_type == 3) { /* Movement from lizard */
            /* Find lizard in client_lizards from its id */
            index_client_lizards_id = 0;
            for (int jjj = 0; jjj < MAX_LIZARDS; jjj++) {
                if (client_lizards[jjj].id == m.id && client_lizards[jjj].valid) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }

            pos_x_lizards = client_lizards[index_client_lizards_id].char_data.pos_x;
            pos_y_lizards = client_lizards[index_client_lizards_id].char_data.pos_y;
            ch = client_lizards[index_client_lizards_id].char_data.ch;

            pos_x_lizards_aux = pos_x_lizards;
            pos_y_lizards_aux = pos_y_lizards;

            /* Compute lizard new position */
            new_position(&pos_x_lizards_aux, &pos_y_lizards_aux, m.direction[0]);
            
            /* Check if new position is inside the playing field */
            if (!(pos_x_lizards_aux < 0 || pos_y_lizards_aux < 0  || pos_x_lizards_aux >= WINDOW_SIZE || pos_y_lizards_aux  >= WINDOW_SIZE)) {
                /* Verify if new position matches the position of anothers' head lizard */
                if (check_head_in_square(field[pos_x_lizards_aux][pos_y_lizards_aux]) == false) {

                    /* Remove lizard tail from the playing field in the old position */
                    tail(client_lizards[index_client_lizards_id].prevdirection, pos_x_lizards, pos_y_lizards, 
                        true, index_client_lizards_id);

                    /* Insert lizard tail into the playing field in new position */
                    tail(m.direction[0], pos_x_lizards_aux, pos_y_lizards_aux, false, 
                        index_client_lizards_id);

                    index_client = index_client_lizards_id;
                    index_roaches = -1;  
                    element_type = 1;              

                    /* Remove lizard from the playing field in old position */
                    field[pos_x_lizards][pos_y_lizards] = display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                        index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);

                    /* Calculates new mark position */
                    pos_x_lizards = pos_x_lizards_aux;
                    pos_y_lizards = pos_y_lizards_aux;
                    client_lizards[index_client_lizards_id].char_data.pos_x = pos_x_lizards;
                    client_lizards[index_client_lizards_id].char_data.pos_y = pos_y_lizards;

                    /* Insert lizard into the playing field in new position */
                    field[pos_x_lizards][pos_y_lizards] = display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                        index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);
                    
                    client_lizards[index_client_lizards_id].prevdirection = m.direction[0];

                    /* Check if lizard won the game */
                    if (client_lizards[index_client].score >= POINTS_TO_WIN && end_game == 0) {
                        /* Update lizard tail to '*' and flag the game as finished */
                        end_game = 1;
                        tail(m.direction[0], pos_x_lizards, pos_y_lizards, false, 
                            index_client_lizards_id);

                        end_game = 2;
                    }
                }
                else {
                    /* In case lizard new position matches the position of anothers' head lizard, average their scores */
                    split_health(field[pos_x_lizards_aux][pos_y_lizards_aux], index_client_lizards_id);
                }
                
                /* Send lizard score to client */
                send = zmq_send (responder, &client_lizards[index_client_lizards_id].score, sizeof(double), 0);
                assert(send != -1);
            }
        }
        else if (m.msg_type == 4) { /* Disconnection from lizard */
            /* Send successful response to client */
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;

            /* Find lizard in client_lizards from its id */
            index_client_lizards_id = 0;
            for (int jjj = 0; jjj < MAX_LIZARDS; jjj++) {
                if (client_lizards[jjj].id == m.id && client_lizards[jjj].valid) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }

            pos_x_lizards = client_lizards[index_client_lizards_id].char_data.pos_x;
            pos_y_lizards = client_lizards[index_client_lizards_id].char_data.pos_y;
            ch = client_lizards[index_client_lizards_id].char_data.ch;

            /* Remove lizard tail from the playing field in the old position */
            tail(client_lizards[index_client_lizards_id].prevdirection, pos_x_lizards, pos_y_lizards, 
                true, index_client_lizards_id);

            index_client = index_client_lizards_id;
            index_roaches = -1;  
            element_type = 1;

            /* Remove roach from the playing field in old position */
            field[pos_x_lizards][pos_y_lizards] = display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);

            /* Inactivate lizard, and decrement number of lizards */
            client_lizards[index_client_lizards_id].valid = false;
            total_lizards--;
        } //5-disconnect roaches, 6-join wasp, 7-move wasp, 8-disconnect wasp
        else if (m.msg_type == 5) { /* Disconnection from roaches */
            /* Send successful response to client */
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;

            /* Find roach in client_roaches from its id */
            index_client_roaches_id = 0;
            for (int jjj = 0; jjj < MAX_ROACHES_PER_CLIENT; jjj++) {
                if (client_roaches[jjj].id == m.id && client_roaches[jjj].valid) {
                    index_client_roaches_id = jjj;
                    break;
                }
            }


            for (i = 0; i < m.nChars; i++) {
                if (client_roaches[index_client_roaches_id].active[i] == true) {
                    /* For each active roach, compute its new position */
                    pos_x_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_x;
                    pos_y_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_y;
                    ch = client_roaches[index_client_roaches_id].char_data[i].ch;

                    index_client = index_client_roaches_id;
                    index_roaches = i;  
                    element_type = 2;

                    /* Remove roach from the playing field in old position */
                    field[pos_x_roaches][pos_y_roaches] = display_in_field(' ', pos_x_roaches, pos_y_roaches, index_client, 
                        index_roaches, element_type, field[pos_x_roaches][pos_y_roaches]);
                }
            }
            
            /* Inactivate roach, and decrement number of roaches */
            client_roaches[index_client_roaches_id].valid = false;
            n_clients_roaches--;
            total_roaches -= m.nChars;
        }


        /* Display roachs stats: name and score */
        display_stats();
    }

    free_exit();
	return 0;
}