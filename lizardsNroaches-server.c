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
pos_roaches *client_roaches;
pos_lizards client_lizards[MAX_LIZARDS];

void new_position(int* x, int *y, direction_t direction);
int find_ch_info(ch_info_t char_data[], int n_char, int ch);
void split_health(list_element *head, int index_client, int element_type);
void search_and_destroy_roaches(list_element *head, int index_client);
list_element *display_in_field(char ch, int x, int y, int index_client, int index_roaches, int element_type, list_element *head);
void tail(direction_t direction, int x, int y, bool delete, int index_client);
bool check_head_in_square(list_element *head);
char check_prioritary_element(list_element *head);
void *free_safe (void *aux);
list_element ***allocate3DArray();
void free3DArray(list_element ***table);
void ressurect_roaches();

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

void split_health(list_element *head, int index_client, int element_type) {
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
            avg += client_lizards[i].score;
        }
        else {
            to_split[i] = FALSE;
        }
    }

    while (current != NULL) {
        if (element_type == 0) {
            if (current->data.element_type == 1 && current->data.index_client != index_client) { // head of lizard
                avg += client_lizards[current->data.index_client].score;
                to_split[current->data.index_client] = TRUE;
                counter++;
                break;
            }
        }
        if (element_type == 1) {
            if (current->data.element_type == 0 && current->data.index_client != index_client) { // head of lizard
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

void search_and_destroy_roaches(list_element *head, int index_client) {
    // having a tail, we will iterate through the list searching for heads
    list_element *current = head;
    list_element *nextNode;
    bool end = FALSE;
    dead_roach r;
    square s;

    while (!end) {
        while (current != NULL) {
            
            if (current->data.element_type == 2) { //found roach
                // mvwprintw(debug_win, 1, 1, "Index Client %d, Index Roaches %d", current->data.index_client, current->data.index_roaches);
                // wrefresh(debug_win);

                //increase lizard score
                client_lizards[index_client].score += client_roaches[current->data.index_client].char_data[current->data.index_roaches].ch - '0';

                client_roaches[current->data.index_client].active[current->data.index_roaches] = FALSE;

                // mvwprintw(debug_win, 2, 1, "Index Client %d, Index Roaches %d", current->data.index_client, current->data.index_roaches);
                // wrefresh(debug_win);

                // insert roach in inactive roaches list with associated start_time
                r.death_time = clock();

                // mvwprintw(debug_win, 3, 1, "Index Client %d, Index Roaches %d", current->data.index_client, current->data.index_roaches);
                // wrefresh(debug_win);

                r.index_client = current->data.index_client;
                r.index_roaches = current->data.index_roaches;

                // mvwprintw(debug_win, 4, 1, "Index Client %d, Index Roaches %d", r.index_client, r.index_roaches);
                // wrefresh(debug_win);

                ///////
                //printar a lista antes de eliminar a barata
                // fifo_element* temp;
                // int kkkk=1;

                // mvwprintw(debug_win, kkkk, 1, "antes de apagar barata r death_time %lf, Index Client %d, Index Roaches %d", r.death_time,
                //     r.index_client, r.index_roaches);
                // wrefresh(debug_win);
                // kkkk++;

                // temp=roaches_killed;
                // // kkkk=1;
                // while (temp != NULL) {
                //     mvwprintw(debug_win, kkkk, 1, "death_time %lf, Index Client %d, Index Roaches %d", temp->data.death_time,
                //     temp->data.index_client, temp->data.index_roaches);
                //     wrefresh(debug_win);
                //     kkkk+=1;
                //     temp = temp->next;
                // }

                push_fifo(&roaches_killed, r);

                //printar a lista antes de eliminar a barata
                // mvwprintw(debug_win, kkkk, 1, "depois de apagar barata");
                // wrefresh(debug_win);
                // kkkk++;

                // temp=roaches_killed;
                // while (temp != NULL) {
                //     mvwprintw(debug_win, kkkk, 1, "death_time %lf, Index Client %d, Index Roaches %d", temp->data.death_time,
                //     temp->data.index_client, temp->data.index_roaches);
                //     wrefresh(debug_win);
                //     kkkk+=1;
                //     temp = temp->next;
                // }

                /////

                s.element_type = current->data.element_type;
                s.index_client = current->data.index_client;
                s.index_roaches = current->data.index_roaches;

                ///
                ///////
                //printar a lista antes de eliminar a barata
                // list_element* temp2;
                // int kkkk=0;

                // mvwprintw(debug_win, kkkk, 1, "antes de apagar barata");
                // wrefresh(debug_win);
                // kkkk++;

                // temp2=head;
                // kkkk=1;
                // while (temp2 != NULL) {
                //     mvwprintw(debug_win, kkkk, 1, "death_time %d, Index Client %d, Index Roaches %d", temp2->data.element_type,
                //     temp2->data.index_client, temp2->data.index_roaches);
                //     wrefresh(debug_win);
                //     kkkk+=1;
                //     temp2 = temp2->next;
                // }

                deletelist_element(&head, s);

                // //printar a lista antes de eliminar a barata
                // mvwprintw(debug_win, kkkk, 1, "depois de apagar barata");
                // wrefresh(debug_win);
                // kkkk++;

                // temp2=head;
                // while (temp2 != NULL) {
                //     mvwprintw(debug_win, kkkk, 1, "death_time %d, Index Client %d, Index Roaches %d", temp2->data.element_type,
                //     temp2->data.index_client, temp2->data.index_roaches);
                //     wrefresh(debug_win);
                //     kkkk+=1;
                //     temp2 = temp2->next;
                // }


                ///

                break;
            }
            nextNode = current->next;
            current = nextNode;
            if (current == NULL) {
                end = TRUE;
            }
        
        }
    }


    return;
}

list_element *display_in_field(char ch, int x, int y, int index_client, 
                    int index_roaches, int element_type, list_element *head) {

    square new_data;
    new_data.element_type = element_type;
    new_data.index_client = index_client;
    new_data.index_roaches = index_roaches;

    if (ch == ' ') {
        // delete from list position xy in field
        // if (element_type == 0) {
        //     mvwprintw(debug_win, 1, 1, "%d-%d", head->next.data.element_type, element_type);
        //     wrefresh(debug_win);
        //     mvwprintw(debug_win, 2, 1, "%d-%d", head->next.index_client, index_client);
        //     wrefresh(debug_win);
        //     mvwprintw(debug_win, 3, 1, "%d-%d", head->next.index_roaches, index_roaches);
        //     wrefresh(debug_win);
        // }   
        deletelist_element(&head, new_data);

        // verificar se nesta posiçao existem mais elementos? se existirem display = True
        if (head != NULL) {
            ch = check_prioritary_element(head); 
            // if (element_type == 0) {
            //     mvwprintw(debug_win, 4, 1, "%d", head->data.element_type);
            //     wrefresh(debug_win);
            //     mvwprintw(debug_win, 5, 1, "%d", head->data.index_client);
            //     wrefresh(debug_win);
            //     mvwprintw(debug_win, 6, 1, "%d", head->data.index_roaches);
            //     wrefresh(debug_win);
            // }       
        }
        // else {
        //     if (element_type == 0) {
        //         mvwprintw(debug_win, 4, 1, "NULL");
        //         wrefresh(debug_win);
        //     }     
        // }

        
        
    }
    else {
        // add to list position xy in field    
        insertBegin(&head, new_data);

        if (head != NULL && element_type == 0) {
            ch = check_prioritary_element(head);
        }

        // se for um lizard:
        //  verificar se a tua cabeça coincide com:
        //      - uma barata (comer barata), ou seja vai aumentar o seu score, e a barata desaparece 5 segundos e reaparece aleatoriamente
        //      - uma tail: repartir os pontos de ambos os lizards
        if (element_type == 1) {
            search_and_destroy_roaches(head, index_client);
            split_health(head, index_client, element_type);
        }

        // se for uma tail:
        //  verificar se coincide com:
        //      - uma head de um lizard: repartir os pontos de ambos os lizards
        if (element_type == 0) {
            split_health(head, index_client, element_type);
        }
    }
    

    wmove(my_win, x, y);
    waddch(my_win, ch | A_BOLD);
    wrefresh(my_win);

    return head;
}

void tail(direction_t direction, int x, int y, bool delete, int index_client) {
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

char check_prioritary_element(list_element *head) {
    list_element *current = head;
    list_element *nextNode;
    char winner = '?';
    int winner_type = -1;
    while (current != NULL) {
        // ta a entrar em loop com element_type=0, winner_type=0
        if (current->data.element_type == 1) { // head of lizard
            winner = client_lizards[current->data.index_client].char_data.ch;
            
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

    // mvwprintw(debug_win, 1, 1, "%c", winner);
    // wrefresh(debug_win);
    
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

void ressurect_roaches() {
    double end_time, inactive_time;

    while (roaches_killed != NULL) {
        end_time = clock();
        inactive_time = 1000 * ((double) (end_time - roaches_killed->data.death_time)) / CLOCKS_PER_SEC; 
        mvwprintw(debug_win, 1, 1, "%lf", inactive_time);
        wrefresh(debug_win);
        
        if (inactive_time >= RESPAWN_TIME) {

            client_roaches[roaches_killed->data.index_client].active[roaches_killed->data.index_roaches] = TRUE;
        
            
            while (1) {
                client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                // verify if new position matches the position of anothers' head lizard
                if(check_head_in_square(field[client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_x]
                    [client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_y]) == FALSE) {
                    break;
                }
            }

            ///////
            //printar a lista antes de eliminar a barata
            fifo_element* temp;
            int kkkk=1;

            mvwprintw(debug_win, kkkk, 1, "antes de apagar 1ª barata");
            wrefresh(debug_win);
            kkkk++;

            temp=roaches_killed;
            // kkkk=1;
            while (temp != NULL) {
                mvwprintw(debug_win, kkkk, 1, "death_time %lf, Index Client %d, Index Roaches %d", temp->data.death_time,
                temp->data.index_client, temp->data.index_roaches);
                wrefresh(debug_win);
                kkkk+=1;
                temp = temp->next;
            }

            pop_fifo(&roaches_killed);

            //printar a lista antes de eliminar a barata
            mvwprintw(debug_win, kkkk, 1, "depois de apagar barata");
            wrefresh(debug_win);
            kkkk++;

            temp=roaches_killed;
            while (temp != NULL) {
                mvwprintw(debug_win, kkkk, 1, "death_time %lf, Index Client %d, Index Roaches %d", temp->data.death_time,
                temp->data.index_client, temp->data.index_roaches);
                wrefresh(debug_win);
                kkkk+=1;
                temp = temp->next;
            }

            /////
            

        }      
        else {
            break;
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
    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
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
                field[pos_x_roaches][pos_y_roaches] = display_in_field(ch, pos_x_roaches, pos_y_roaches, index_client, 
                    index_roaches, element_type, field[pos_x_roaches][pos_y_roaches]);
                
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
                if (client_roaches[index_client_roaches_id].active[i] == TRUE) {
                    
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
                            field[pos_x_roaches][pos_y_roaches] = display_in_field(' ', pos_x_roaches, pos_y_roaches, index_client, 
                                index_roaches, element_type, field[pos_x_roaches][pos_y_roaches]);

                            /* calculates new mark position */
                            pos_x_roaches = pos_x_roaches_aux;
                            pos_y_roaches = pos_y_roaches_aux;
                            client_roaches[index_client_roaches_id].char_data[i].pos_x = pos_x_roaches;
                            client_roaches[index_client_roaches_id].char_data[i].pos_y = pos_y_roaches;

                            /* draw mark on new position */
                            field[pos_x_roaches][pos_y_roaches] = display_in_field(ch, pos_x_roaches, pos_y_roaches, index_client, 
                                index_roaches, element_type, field[pos_x_roaches][pos_y_roaches]);
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
                FALSE, total_lizards);

            client_lizards[total_lizards].prevdirection = m.direction[0];

            index_client = total_lizards;
            index_roaches = -1;  
            element_type = 1;

            /* draw mark on new position */
            field[pos_x_lizards][pos_y_lizards] = display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);

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
                        TRUE, index_client_lizards_id);

                    // new tail
                    tail(m.direction[0], pos_x_lizards_aux, pos_y_lizards_aux, FALSE, 
                        index_client_lizards_id);

                    index_client = index_client_lizards_id;
                    index_roaches = -1;  
                    element_type = 1;              

                    /*deletes old place */
                    field[pos_x_lizards][pos_y_lizards] = display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                        index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);

                    /* calculates new mark position */
                    pos_x_lizards = pos_x_lizards_aux;
                    pos_y_lizards = pos_y_lizards_aux;
                    client_lizards[index_client_lizards_id].char_data.pos_x = pos_x_lizards;
                    client_lizards[index_client_lizards_id].char_data.pos_y = pos_y_lizards;

                    index_client = index_client_lizards_id;
                    index_roaches = -1;  
                    element_type = 1;
                    /* draw mark on new position */
                    field[pos_x_lizards][pos_y_lizards] = display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                        index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);
                    

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
                TRUE, index_client_lizards_id);

            index_client = index_client_lizards_id;
            index_roaches = -1;  
            element_type = 1;

            /* deletes old place */
            field[pos_x_lizards][pos_y_lizards] = display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);

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