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
#define MAX_LIZARDS 2
#define TAIL_SIZE 5
#define RESPAWN_TIME 5

WINDOW *my_win;
WINDOW *debug_win;
WINDOW *stats_win;
list_element ***field;
fifo_element *roaches_killed;
pos_roaches *client_roaches;
pos_lizards client_lizards[MAX_LIZARDS];
void *context;
void *responder;

void new_position(int* x, int *y, direction_t direction);
int find_ch_info(ch_info_t char_data[], int n_char, int ch);
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

void new_position(int *x, int *y, direction_t direction){
    switch (direction) {
        case UP:
            (*x) --;
            if(*x == 0)
                *x = 2;
            break;
        case DOWN:
            (*x) ++;
            if(*x == WINDOW_SIZE-1)
                *x = WINDOW_SIZE-3;
            break;
        case LEFT:
            (*y) --;
            if(*y ==0)
                *y = 2;
            break;
        case RIGHT:
            (*y) ++;
            if(*y == WINDOW_SIZE-1)
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

void split_health(list_element *head, int index_client) {
    // having a tail, we will iterate through the list searching for heads
    list_element *current = head;
    list_element *nextNode;
    double avg = 0;

    avg = client_lizards[index_client].score;

    while (current != NULL) {
        if (current->data.element_type == 1) { // head of lizard
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

void search_and_destroy_roaches(list_element *head, int index_client) {
    // having a tail, we will iterate through the list searching for heads
    list_element *current = head;
    list_element *nextNode;
    bool end = false;
    bool check;
    dead_roach r;
    square s;

    while (!end) {
        while (current != NULL) {
            
            if (current->data.element_type == 2) { //found roach
                // mvwprintw(debug_win, 1, 1, "Index Client %d, Index Roaches %d", current->data.index_client, current->data.index_roaches);
                // wrefresh(debug_win);

                //increase lizard score
                client_lizards[index_client].score += client_roaches[current->data.index_client].char_data[current->data.index_roaches].ch - '0';

                client_roaches[current->data.index_client].active[current->data.index_roaches] = false;

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

                check = push_fifo(&roaches_killed, r);
                if (!check) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free_exit();
                    exit(0);
                }

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
                end = true;
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
        }
    }
    

    wmove(my_win, x, y);
    waddch(my_win, ch | A_BOLD);
    wrefresh(my_win);

    return head;
}

void tail(direction_t direction, int x, int y, bool delete, int index_client) {
    char display;
    if (delete == true) {
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
            return true;
        }
        nextNode = current->next;
        current = nextNode;
    }
    return false;
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
        // TODO improve this
        // mvwprintw(debug_win, 1, 1, "%lf", inactive_time);
        // wrefresh(debug_win);
        
        if (inactive_time >= RESPAWN_TIME) {

            client_roaches[roaches_killed->data.index_client].active[roaches_killed->data.index_roaches] = true;
        
            
            while (1) {
                client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                // verify if new position matches the position of anothers' head lizard
                if(check_head_in_square(field[client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_x]
                    [client_roaches[roaches_killed->data.index_client].char_data[roaches_killed->data.index_roaches].pos_y]) == false) {
                    break;
                }
            }

            ///////
            //printar a lista antes de eliminar a barata
            fifo_element* temp;
            int kkkk=1;

            // mvwprintw(debug_win, kkkk, 1, "antes de apagar 1ª barata");
            // wrefresh(debug_win);
            kkkk++;

            temp=roaches_killed;
            // kkkk=1;
            while (temp != NULL) {
                // mvwprintw(debug_win, kkkk, 1, "death_time %lf, Index Client %d, Index Roaches %d", temp->data.death_time,
                // temp->data.index_client, temp->data.index_roaches);
                // wrefresh(debug_win);
                kkkk+=1;
                temp = temp->next;
            }

            pop_fifo(&roaches_killed);

            //printar a lista antes de eliminar a barata
            // mvwprintw(debug_win, kkkk, 1, "depois de apagar barata");
            // wrefresh(debug_win);
            kkkk++;

            temp=roaches_killed;
            while (temp != NULL) {
                // mvwprintw(debug_win, kkkk, 1, "death_time %lf, Index Client %d, Index Roaches %d", temp->data.death_time,
                // temp->data.index_client, temp->data.index_roaches);
                // wrefresh(debug_win);
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

void display_stats() {
    /* TODO: sort */
    int i = 0;

    for (int j = 0; j <= MAX_LIZARDS; j++) {
        mvwprintw(stats_win, j, 1, "\t\t\t\t\t");
        wrefresh(stats_win);
        if (client_lizards[j].valid) {
            mvwprintw(stats_win, i, 1, "Player: %c, Score: %lf", client_lizards[j].char_data.ch, client_lizards[j].score);
            wrefresh(stats_win);
            i++;
        }
    }
}

void free_exit() {
    delwin(stats_win);
    delwin(debug_win);
    delwin(my_win);
    endwin();
    zmq_close (responder);
    zmq_ctx_destroy (context);
    client_roaches = free_safe(client_roaches);
    free3DArray(field);
}

int main() {
    remote_char_t m;

    field = allocate3DArray();
    if (field == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_exit();
        exit(0);
    }

    context = zmq_ctx_new ();
    assert(context != NULL);
    responder = zmq_socket (context, ZMQ_REP);
    assert(responder != NULL);
    int rc = zmq_bind (responder, "tcp://*:5560");
    assert (rc == 0);

	initscr();		    	
	cbreak();				
    keypad(stdscr, true);   
	noecho();			    

    /* creates a window and draws a border */
    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0, 0);
	wrefresh(my_win);

    // creates a window for stats
    stats_win = newwin(MAX_LIZARDS + 1, 100, WINDOW_SIZE + 1, 0);

    // creates a window for debug
    debug_win = newwin(20, 100, WINDOW_SIZE + 1 + MAX_LIZARDS + 1, 0);
    
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
    int index_of_position_to_insert, jj = 0;
    
    int pos_x_roaches, pos_x_lizards, pos_y_roaches, pos_y_lizards;
    int pos_x_roaches_aux, pos_y_roaches_aux, pos_x_lizards_aux, pos_y_lizards_aux;
    
    //in worst case scenario each client has one roach and there are (WINDOW_SIZE*WINDOW_SIZE)/3 roaches
    client_roaches = (pos_roaches*) calloc(max_roaches, sizeof(pos_roaches));
    if (client_roaches == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_exit();
        exit(0);
    }

    srand(time(NULL));

    while (1) {

        // ressurect roaches
        ressurect_roaches(client_roaches);

        recv = zmq_recv (responder, &m, sizeof(remote_char_t), 0);
        assert(recv != -1);        

        if(m.msg_type == 0) {
            // testing if roach is valid
            if (m.nChars + total_roaches > max_roaches) {
                ok = 0;
                send = zmq_send (responder, &ok, sizeof(int), 0);
                assert(send != -1);
                ok = 1;
                continue;
            }
            // check if new client id is already in use
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
            // check if new client id is already in use
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
            
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;
            
            client_roaches[n_clients_roaches].id = m.id;
            client_roaches[n_clients_roaches].nChars = m.nChars;

            for (i = 0; i < m.nChars; i++) {
                client_roaches[n_clients_roaches].char_data[i].ch = m.ch[i];
                client_roaches[n_clients_roaches].active[i] = true;

                while (1) {
                    client_roaches[n_clients_roaches].char_data[i].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
                    client_roaches[n_clients_roaches].char_data[i].pos_y = rand() % (WINDOW_SIZE - 2) + 1;

                    // verify if new position matches the position of anothers' head lizard
                    if(check_head_in_square(field[client_roaches[n_clients_roaches].char_data[i].pos_x][ 
                        client_roaches[n_clients_roaches].char_data[i].pos_y]) == false) {
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
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;

            uint32_t index_client_roaches_id = 0;

            for (int jjj = 0; jjj < n_clients_roaches;jjj++) {
                if(client_roaches[jjj].id == m.id) {
                    index_client_roaches_id = jjj;
                    break;
                }
            }

            for (i = 0; i < m.nChars; i++) {
                if (client_roaches[index_client_roaches_id].active[i] == true) {
                    
                    pos_x_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_x;
                    pos_y_roaches = client_roaches[index_client_roaches_id].char_data[i].pos_y;
                    ch = client_roaches[index_client_roaches_id].char_data[i].ch;

                    pos_x_roaches_aux = pos_x_roaches;
                    pos_y_roaches_aux = pos_y_roaches;

                    new_position(&pos_x_roaches_aux, &pos_y_roaches_aux, m.direction[i]);
                
                    if (!(pos_x_roaches_aux < 0 || pos_y_roaches_aux < 0  || pos_x_roaches_aux >= WINDOW_SIZE || pos_y_roaches_aux  >= WINDOW_SIZE)) {
                        // verify if new position matches the position of anothers' head lizard
                        if (check_head_in_square(field[pos_x_roaches_aux][pos_y_roaches_aux]) == false) {
                            
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
        else if (m.msg_type == 2) {

            // check if lizard is valid
            if (total_lizards + 1 > MAX_LIZARDS) {
                ok = (int) '?'; // in case the pool is full of lizards
                send = zmq_send (responder, &ok, sizeof(int), 0);
                assert(send != -1);
                ok = 1;
                continue;
            }
            // check if new client id is already in use
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
            // check if new client id is already in use
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



            index_of_position_to_insert = -1;
            for (jj = 0; jj < MAX_LIZARDS; jj++) {
                if (!client_lizards[jj].valid) {
                    index_of_position_to_insert = jj;
                    break;
                }
            }
            ok = (int) 'a' + index_of_position_to_insert;

            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;


            client_lizards[index_of_position_to_insert].id = m.id;
            client_lizards[index_of_position_to_insert].score = 0;
            client_lizards[index_of_position_to_insert].valid = true;

            while (1) {
                client_lizards[index_of_position_to_insert].char_data.pos_x =  rand() % (WINDOW_SIZE - 2) + 1;
                client_lizards[index_of_position_to_insert].char_data.pos_y =  rand() % (WINDOW_SIZE - 2) + 1;

                // verify if new position matches the position of anothers' head lizard
                if (check_head_in_square(field[client_lizards[index_of_position_to_insert].char_data.pos_x]
                    [client_lizards[index_of_position_to_insert].char_data.pos_y]) == false) {
                    break;
                }
            }

            client_lizards[index_of_position_to_insert].char_data.ch = (char) ((int) 'a' + index_of_position_to_insert);

            pos_x_lizards = client_lizards[index_of_position_to_insert].char_data.pos_x;
            pos_y_lizards = client_lizards[index_of_position_to_insert].char_data.pos_y;
            ch = client_lizards[index_of_position_to_insert].char_data.ch;

            m.direction[0] = rand() % 4;
            
            tail(m.direction[0], pos_x_lizards, pos_y_lizards, 
                false, index_of_position_to_insert);

            client_lizards[index_of_position_to_insert].prevdirection = m.direction[0];

            index_client = index_of_position_to_insert;
            index_roaches = -1;  
            element_type = 1;

            /* draw mark on new position */
            field[pos_x_lizards][pos_y_lizards] = display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);

            total_lizards++;

        } 
        else if(m.msg_type == 3){
            uint32_t index_client_lizards_id = 0;

            for(int jjj = 0; jjj < MAX_LIZARDS;jjj++) {
                if(client_lizards[jjj].id == m.id && client_lizards[jjj].valid) {
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
            
            if (!(pos_x_lizards_aux < 0 || pos_y_lizards_aux < 0  || pos_x_lizards_aux >= WINDOW_SIZE || pos_y_lizards_aux  >= WINDOW_SIZE)) {
                if(check_head_in_square(field[pos_x_lizards_aux][pos_y_lizards_aux]) == false) {

                    // delete old tail
                    tail(client_lizards[index_client_lizards_id].prevdirection, pos_x_lizards, pos_y_lizards, 
                        true, index_client_lizards_id);

                    // new tail
                    tail(m.direction[0], pos_x_lizards_aux, pos_y_lizards_aux, false, 
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

                    field[pos_x_lizards][pos_y_lizards] = display_in_field(ch, pos_x_lizards, pos_y_lizards, index_client, 
                        index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);
                    

                    client_lizards[index_client_lizards_id].prevdirection = m.direction[0];
                }
                else {
                    // mvwprintw(debug_win, 1, 0, "%d", field[pos_x_lizards_aux][pos_y_lizards_aux]->data.index_client);
                    // wrefresh(debug_win);
                    
                    split_health(field[pos_x_lizards_aux][pos_y_lizards_aux], index_client_lizards_id);
                }
                
                send = zmq_send (responder, &client_lizards[index_client_lizards_id].score, sizeof(double), 0);
                assert(send != -1);
                
            }
        }
        else if(m.msg_type == 4){
            send = zmq_send (responder, &ok, sizeof(int), 0);
            assert(send != -1);
            ok = 1;

            uint32_t index_client_lizards_id = 0;

            for(int jjj = 0; jjj < MAX_LIZARDS; jjj++) {
                if(client_lizards[jjj].id == m.id && client_lizards[jjj].valid) {
                    index_client_lizards_id = jjj;
                    break;
                }
            }

            

            pos_x_lizards = client_lizards[index_client_lizards_id].char_data.pos_x;
            pos_y_lizards = client_lizards[index_client_lizards_id].char_data.pos_y;
            ch = client_lizards[index_client_lizards_id].char_data.ch;

            /* delete old tail */
            tail(client_lizards[index_client_lizards_id].prevdirection, pos_x_lizards, pos_y_lizards, 
                true, index_client_lizards_id);

            index_client = index_client_lizards_id;
            index_roaches = -1;  
            element_type = 1;

            /* deletes old place */
            field[pos_x_lizards][pos_y_lizards] = display_in_field(' ', pos_x_lizards, pos_y_lizards, index_client, 
                index_roaches, element_type, field[pos_x_lizards][pos_y_lizards]);

            client_lizards[index_client_lizards_id].valid = false;
            total_lizards--;

            
        }

        display_stats();
        	
    }
    /* End curses mode */
    delwin(stats_win);
    delwin(debug_win);
    delwin(my_win);
  	endwin();
    zmq_close (responder);
    zmq_ctx_destroy (context);
    client_roaches = free_safe(client_roaches);
    free3DArray(field);

	return 0;
}