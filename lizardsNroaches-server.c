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

#include "lists.h"
#include "fifo.h"
#include "remote-char.h"

#define WINDOW_SIZE 20 
#define MAX_LIZARDS 26 
#define TAIL_SIZE 5
#define RESPAWN_TIME 5

WINDOW *my_win;
WINDOW *debug_win;
list_element ***field;
fifo_element *roaches_killed;

void new_position(int* x, int *y, direction_t direction);
int find_ch_info(ch_info_t char_data[], int n_char, int ch);
void split_health(list_element *head, int index_client, pos_lizards *client_lizards, int element_type);
void search_and_destroy_roaches(list_element *head, int index_client, pos_lizards *client_lizards, int element_type, pos_roaches *client_roaches);
void display_in_field(char ch, int x, int y, int index_client, int index_roaches, int element_type, pos_lizards *client_lizards, pos_roaches *client_roaches);
void tail(direction_t direction, int x, int y, bool delete, int index_client, pos_lizards *client_lizards, pos_roaches *client_roaches);
bool check_head_in_square(list_element *head);
char check_prioritary_element(list_element *head, pos_lizards *client_lizards, pos_roaches *client_roaches);
void *free_safe (void *aux);
list_element ***allocate3DArray();
void free3DArray(list_element ***table);
void ressurect_roaches(pos_roaches *client_roaches);

void new_position(int* x, int *y, direction_t direction){
    switch (direction) {
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
        case NONE:
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

void split_health(list_element *head, int index_client, pos_lizards *client_lizards, int element_type) {
    // having a tail, we will iterate through the list searching for heads
    list_element *current = head;
    list_element *nextNode;

    double avg = 0;
    bool to_split[MAX_LIZARDS];
    int counter = 0;

    for (int i = 0; i < MAX_LIZARDS; i++) {
        if (i == index_client) {
            to_split[i] = TRUE;
            counter++;
        }
        else {
            to_split[i] = FALSE;
        }
    }

    while (current != NULL) {
        if (element_type == 0) {
            if (current->data.element_type == 1) { // head of lizard
                avg += client_lizards[current->data.index_client].score;
                to_split[current->data.index_client] = TRUE;
                counter++;
                break;
            }
        }
        if (element_type == 1) {
            if (current->data.element_type == 0) { // head of lizard
                avg += client_lizards[current->data.index_client].score;
                to_split[current->data.index_client] = TRUE;
                counter++;
            }
        }
    
        nextNode = current->next;
        current = nextNode;
    }

    avg = avg / counter;

    for (int i = 0; i < MAX_LIZARDS; i++) {
        if (to_split[i]) {
            client_lizards[i].score = avg;
        }
    }

    return;
}

void search_and_destroy_roaches(list_element *head, int index_client, pos_lizards *client_lizards, int element_type, pos_roaches *client_roaches) {
    // having a tail, we will iterate through the list searching for heads
    list_element *current = head;
    list_element *nextNode;
    bool flag = FALSE;
    dead_roach r;

    while (current != NULL) {
        
        if (current->data.element_type == 2) { //found roach
            //increase lizard score
            client_lizards[index_client].score += client_roaches[current->data.index_client].char_data[current->data.index_roaches].ch;
            //destroy roach and 5 seconds later respawn it in a random position
            display_in_field(' ', client_roaches[current->data.index_client].char_data[current->data.index_roaches].pos_x, 
                client_roaches[current->data.index_client].char_data[current->data.index_roaches].pos_y, 
                index_client, current->data.index_roaches, current->data.element_type, 
                client_lizards, client_roaches);

            client_roaches[current->data.index_client].active[current->data.index_roaches] = FALSE;

            // insert roach in inactive roaches list with associated start_time
            r.death_time = clock();
            r.index_client = current->data.index_client;
            r.index_roaches = current->data.index_roaches;
            push_fifo(&roaches_killed, r);

            current = deletelist_element(&current, current->data);
            flag = TRUE;
            
        }
    
        if (!flag) {
            nextNode = current->next;
            current = nextNode;
        }
    }


    return;
}

void display_in_field(char ch, int x, int y, int index_client, 
                    int index_roaches, int element_type, pos_lizards *client_lizards, pos_roaches *client_roaches) {
    
    list_element *head;
    head = field[x][y];

    square new_data;
    new_data.element_type = element_type;
    new_data.index_client = index_client;
    new_data.index_roaches = index_roaches;

    if (ch == ' ') {
        // delete from list position xy in field
        head = deletelist_element(&head, new_data);
        field[x][y] = head;

        // verificar se nesta posiçao existem mais elementos? se existirem display = True
        if (head != NULL) {
            ch = check_prioritary_element(head, client_lizards, client_roaches);          
        }
    }
    else {
        // add to list position xy in field    
        head = insertBegin(&head, new_data);
        field[x][y] = head;

        if (head != NULL && element_type == 0) {
            ch = check_prioritary_element(head, client_lizards, client_roaches);
        }
    }


    // se for um lizard:
    //  verificar se a tua cabeça coincide com:
    //      - uma barata (comer barata), ou seja vai aumentar o seu score, e a barata desaparece 5 segundos e reaparece aleatoriamente
    //      - uma tail: repartir os pontos de ambos os lizards
    if (element_type == 1) {
        search_and_destroy_roaches(head, index_client, client_lizards, element_type, client_roaches);
        split_health(head, index_client, client_lizards, element_type);
    }


    // se for uma tail:
    //  verificar se coincide com:
    //      - uma head de um lizard: repartir os pontos de ambos os lizards
    if (element_type == 0) {
        split_health(head, index_client, client_lizards, element_type);
    }

    wmove(my_win, x, y);
    waddch(my_win, ch | A_BOLD);
    wrefresh(my_win);
}

void tail(direction_t direction, int x, int y, bool delete, int index_client, pos_lizards *client_lizards, pos_roaches *client_roaches) {
    char display;
    if (delete == TRUE) {
        display = ' ';
    }
    else {
        display = '.';
    }

    int index_roaches = -1;  
    int element_type = 0;
    
    switch (direction)
    {
    case LEFT:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (y + kk < WINDOW_SIZE - 1) {
                display_in_field(display, x, y + kk, index_client, 
                    index_roaches, element_type, client_lizards, client_roaches);
            }
        }
        break;

    case RIGHT:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (y - kk > 1) {
                display_in_field(display, x, y - kk, index_client, 
                    index_roaches, element_type, client_lizards, client_roaches);
            }
        }
        break;

    case DOWN:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (x - kk > 1) {
                display_in_field( display, x - kk, y, index_client, 
                    index_roaches, element_type, client_lizards, client_roaches);
            }
        }
        break;

    case UP:
        for (int kk = 1; kk <= TAIL_SIZE; kk++) {
            if (x + kk < WINDOW_SIZE - 1) {
                display_in_field(display, x + kk, y, index_client, 
                    index_roaches, element_type, client_lizards, client_roaches);
            }
        }
        break;
        
    default:
        break;
    }
}

bool check_head_in_square(list_element *head) {
    list_element *current = head;
    list_element *nextNode;
    while (current != NULL) {
        if (current->data.element_type == 1) { // head of lizard
            return TRUE;
        }
        nextNode = current->next;
        current = nextNode;
    }
    return FALSE;
}

char check_prioritary_element(list_element *head, pos_lizards *client_lizards, pos_roaches *client_roaches) {
    list_element *current = head;
    list_element *nextNode;
    char winner = '?';
    int winner_type = -1;

    while (current != NULL) {
        
        if (current->data.element_type == 1) { // head of lizard
            winner = client_lizards[current->data.index_client].char_data.ch;
            mvwprintw(debug_win, 1, 1, "%c", winner);
            wrefresh(debug_win);
            return winner;
        }
        else if (current->data.element_type == 2 && (winner_type == -1 || winner_type == 0)) {
            winner = client_roaches[current->data.index_client].char_data[current->data.index_roaches].ch;
            winner_type = 2;    
        }
        else if (current->data.element_type == 0 && winner_type == -1) {
            winner = '.';
            winner_type = 0;
        }
        nextNode = current->next;
        current = nextNode;
    }

    mvwprintw(debug_win, 1, 1, "%c", winner);
    wrefresh(debug_win);
    
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
        return; // Nothing to free
    }

    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (table[i] != NULL) {
            for (int j = 0; j < WINDOW_SIZE; j++) {
                // Free the third dimension if it was allocated
                freeList(table[i][j]);
            }
            // Free the second dimension
            table[i] = free_safe(table[i]);
        }
    }

    // Finally, free the first dimension
    table = free_safe(table);
}

void ressurect_roaches(pos_roaches *client_roaches) {
    fifo_element *current = roaches_killed;
    fifo_element *nextNode;
    double end_time = clock(), inactive_time;
    bool flag = FALSE;

    while (current != NULL) {
        inactive_time = ((double) (end_time - current->data.death_time)) / CLOCKS_PER_SEC;
        if (inactive_time >= RESPAWN_TIME) {
            client_roaches[current->data.index_client].active[current->data.index_roaches] = TRUE;
            
            current = pop_fifo(&current);
            
            while (1) {
                client_roaches[current->data.index_client].char_data[current->data.index_roaches].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                client_roaches[current->data.index_client].char_data[current->data.index_roaches].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                // verify if new position matches the position of anothers' head lizard
                if(check_head_in_square(field[client_roaches[current->data.index_client].char_data[current->data.index_roaches].pos_x]
                    [client_roaches[current->data.index_client].char_data[current->data.index_roaches].pos_y]) == FALSE) {
                    break;
                }
            }

            flag = TRUE;
        }
        else {
            break;
        }

        if (!flag) {
            nextNode = current->next;
            current = nextNode;
        }

        
    }
    return;

}

int main() {
    remote_char_t m;

    field = allocate3DArray();

    void *context = zmq_ctx_new ();
    assert(context != NULL);
    void *responder = zmq_socket (context, ZMQ_REP);
    assert(responder != NULL);
    int rc = zmq_bind (responder, "tcp://*:5560");
    assert (rc == 0);

	initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();			    

    /* creates a window and draws a border */
    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0, 0);
	wrefresh(my_win);

    // creates a window for debug
    debug_win = newwin(20, 100, WINDOW_SIZE + 1, 0);
    // mvwprintw(debug_win, 1, 1, "Your text here");
    // wrefresh(debug_win);

    int ch;
    int i = 0;
    int max_roaches = floor(((WINDOW_SIZE - 2)*(WINDOW_SIZE - 2))/3);
    int n_clients_roaches = 0;
    int total_roaches = 0;
    int total_lizards = 0;
    int ok = 1;
    int index_client, index_roaches, element_type;
    size_t send, recv;
    pos_roaches *client_roaches;
    pos_lizards client_lizards[MAX_LIZARDS];
    
    int pos_x_roaches, pos_x_lizards, pos_y_roaches, pos_y_lizards;
    int pos_x_roaches_aux, pos_y_roaches_aux, pos_x_lizards_aux, pos_y_lizards_aux;
    
    //in worst case scenario each client has one roach and there are (WINDOW_SIZE*WINDOW_SIZE)/3 roaches
    client_roaches = (pos_roaches*) calloc(max_roaches, sizeof(pos_roaches));

    srand(time(NULL));

    while (1) {
        // ressurect roaches
        ressurect_roaches(client_roaches);

        recv = zmq_recv (responder, &m, sizeof(remote_char_t), 0);
        assert(recv != -1);
        if (m.nChars + total_roaches > max_roaches && m.msg_type == 0) {
            ok = 0;
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;
            continue;
        }
        else if (m.msg_type == 2) {
            if (total_lizards + 1 > MAX_LIZARDS) {
                ok = (int) '?'; // in case the pool is full of lizards
            }
            else {
                ok = (int) 'a' + total_lizards;
            }
            
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;
        }
        else if (m.msg_type != 3) {
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
        }
        
        ok = 1;

        if(m.msg_type == 0) {
            client_roaches[n_clients_roaches].id = m.id;
            client_roaches[n_clients_roaches].nChars = m.nChars;

            for (i = 0; i < m.nChars; i++) {
                client_roaches[n_clients_roaches].char_data[i].ch = m.ch[i];
                client_roaches[n_clients_roaches].active[i] = TRUE;

                while (1) {
                    client_roaches[n_clients_roaches].char_data[i].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                    client_roaches[n_clients_roaches].char_data[i].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                    // verify if new position matches the position of anothers' head lizard
                    if(check_head_in_square(field[client_roaches[n_clients_roaches].char_data[i].pos_x][ 
                        client_roaches[n_clients_roaches].char_data[i].pos_y]) == FALSE) {
                        break;
                    }
                }
                
                pos_x_roaches = client_roaches[n_clients_roaches].char_data[i].pos_x;
                pos_y_roaches = client_roaches[n_clients_roaches].char_data[i].pos_y;
                ch = client_roaches[n_clients_roaches].char_data[i].ch;

                index_client = n_clients_roaches;
                index_roaches = i;  
                element_type = 2;

                /* draw mark on new position */
                display_in_field(ch, pos_x_roaches, pos_y_roaches, index_client, 
                    index_roaches, element_type, client_lizards, client_roaches);
                
            }

            n_clients_roaches++;
            total_roaches += m.nChars;

        }
        else if (m.msg_type == 1){

            uint32_t index_client_roaches_id = 0;

            for (int jjj = 0; jjj < n_clients_roaches;jjj++) {
                if(client_roaches[jjj].id == m.id) {
                    index_client_roaches_id = jjj;
                    break;
                }
            }

            for (i = 0; i < m.nChars; i++) {
                if (client_roaches[n_clients_roaches].active[i] == TRUE) {

                    pos_x_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_x;
                    pos_y_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_y;
                    ch = client_roaches[index_client_roaches_id].char_data[i].ch;

                    pos_x_roaches_aux = pos_x_roaches;
                    pos_y_roaches_aux = pos_y_roaches;

                    new_position(&pos_x_roaches_aux, &pos_y_roaches_aux, m.direction[i]);
                
                    if (!(pos_x_roaches_aux < 0 || pos_y_roaches_aux < 0  || pos_x_roaches_aux >= WINDOW_SIZE || pos_y_roaches_aux  >= WINDOW_SIZE)) {
                        // verify if new position matches the position of anothers' head lizard
                        if (check_head_in_square(field[pos_x_roaches_aux][pos_y_roaches_aux]) == FALSE) {
                            
                            index_client = index_client_roaches_id;
                            index_roaches = i;  
                            element_type = 2;

                            /* draw mark on new position */
                            display_in_field(' ', pos_x_roaches, pos_y_roaches, index_client, 
                                index_roaches, element_type, client_lizards, client_roaches);

                            /* calculates new mark position */
                            pos_x_roaches = pos_x_roaches_aux;
                            pos_y_roaches = pos_y_roaches_aux;
                            client_roaches[index_client_roaches_id].char_data[i].pos_x = pos_x_roaches;
                            client_roaches[index_client_roaches_id].char_data[i].pos_y = pos_y_roaches;

                            index_client = index_client_roaches_id;
                            index_roaches = i;  
                            element_type = 2;
                            /* draw mark on new position */
                            display_in_field(ch, pos_x_roaches, pos_y_roaches, index_client, 
                                index_roaches, element_type, client_lizards, client_roaches);
                        }
                    }
                }
            }
        }
        else if (m.msg_type == 2){
            // TODO: em vez de acrescentar na posiçao (total_lizards), acrescentar na primeira posiçao com .valid=False
            client_lizards[total_lizards].id = m.id;
            client_lizards[total_lizards].score = 0;
            client_lizards[total_lizards].valid = TRUE;

            while (1) {
                client_lizards[total_lizards].char_data.pos_x =  rand() % (WINDOW_SIZE - 2) + 1;
                client_lizards[total_lizards].char_data.pos_y =  rand() % (WINDOW_SIZE - 2) + 1;

                // verify if new position matches the position of anothers' head lizard
                if (check_head_in_square(field[client_lizards[total_lizards].char_data.pos_x]
                    [client_lizards[total_lizards].char_data.pos_y]) == FALSE) {
                    break;
                }
            }

            client_lizards[total_lizards].char_data.ch = (char) ((int) 'a' + total_lizards);

            pos_x_lizards = client_lizards[total_lizards].char_data.pos_x;
            pos_y_lizards = client_lizards[total_lizards].char_data.pos_y;
            ch = client_lizards[total_lizards].char_data.ch;

            m.direction[0] = rand() % 4;
            
            tail(m.direction[0], pos_x_lizards, pos_y_lizards, 
                FALSE, total_lizards, client_lizards, client_roaches);

            client_lizards[total_lizards].prevdirection = m.direction[0];

            index_client = total_lizards;
            index_roaches = -1;  
            element_type = 1;

            /* draw mark on new position */
            display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                index_roaches, element_type, client_lizards, client_roaches);

            total_lizards++;

        } 
        else if(m.msg_type == 3){
            uint32_t index_client_lizards_id = 0;

            for(int jjj = 0; jjj < total_lizards;jjj++) {
                if(client_lizards[jjj].id == m.id) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }

            pos_x_lizards = client_lizards[index_client_lizards_id].char_data.pos_x;
            pos_y_lizards = client_lizards[index_client_lizards_id].char_data.pos_y;
            ch = client_lizards[index_client_lizards_id].char_data.ch;

            pos_x_lizards_aux = pos_x_lizards;
            pos_y_lizards_aux = pos_y_lizards;

            new_position(&pos_x_lizards_aux, &pos_y_lizards_aux, m.direction[0]);
            // verify if new position matches the position of anothers' head lizard TODO
            
            if (!(pos_x_lizards_aux < 0 || pos_y_lizards_aux < 0  || pos_x_lizards_aux >= WINDOW_SIZE || pos_y_lizards_aux  >= WINDOW_SIZE)) {
                if(check_head_in_square(field[pos_x_lizards_aux][pos_y_lizards_aux]) == FALSE) {

                    // delete old tail
                    tail(client_lizards[index_client_lizards_id].prevdirection, pos_x_lizards, pos_y_lizards, 
                        TRUE, index_client_lizards_id, client_lizards, client_roaches);

                    index_client = index_client_lizards_id;
                    index_roaches = -1;  
                    element_type = 1;
                    
                    /*deletes old place */
                    display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                        index_roaches, element_type, client_lizards, client_roaches);

                    /* calculates new mark position */
                    pos_x_lizards = pos_x_lizards_aux;
                    pos_y_lizards = pos_y_lizards_aux;
                    client_lizards[index_client_lizards_id].char_data.pos_x = pos_x_lizards;
                    client_lizards[index_client_lizards_id].char_data.pos_y = pos_y_lizards;

                    index_client = index_client_lizards_id;
                    index_roaches = -1;  
                    element_type = 1;
                    /* draw mark on new position */
                    display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                        index_roaches, element_type, client_lizards, client_roaches);


                    tail(m.direction[0], pos_x_lizards, pos_y_lizards, FALSE, 
                        index_client_lizards_id, client_lizards, client_roaches);

                    client_lizards[index_client_lizards_id].prevdirection = m.direction[0];
                }
                
                send = zmq_send (responder, &client_lizards[index_client_lizards_id].score, sizeof(double), 0);
                assert(send != -1);
                
            }
        }
        else if(m.msg_type == 4){
            uint32_t index_client_lizards_id = 0;

            for(int jjj = 0; jjj < total_lizards; jjj++) {
                if(client_lizards[jjj].id == m.id) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }

            pos_x_lizards = client_lizards[index_client_lizards_id].char_data.pos_x;
            pos_y_lizards = client_lizards[index_client_lizards_id].char_data.pos_y;
            ch = client_lizards[index_client_lizards_id].char_data.ch;

            /* delete old tail */
            tail(client_lizards[index_client_lizards_id].prevdirection, pos_x_lizards, pos_y_lizards, 
                TRUE, index_client_lizards_id, client_lizards, client_roaches);

            index_client = index_client_lizards_id;
            index_roaches = -1;  
            element_type = 1;

            /* deletes old place */
            display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                index_roaches, element_type, client_lizards, client_roaches);

            client_lizards[total_lizards].valid = FALSE;
        }
        	
    }
    /* End curses mode */
  	endwin();
    zmq_close (responder);
    zmq_ctx_destroy (context);
    client_roaches = free_safe(client_roaches);
    free3DArray(field);

	return 0;
}