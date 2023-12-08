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

#define MAX_PORT_STR_LEN 6 // Max port number "65535" plus null terminator
#define ADDRESS_PREFIX_LEN 10 // Length of "tcp://*:" including null terminator
#define FULL_ADDRESS_LEN (MAX_PORT_STR_LEN + ADDRESS_PREFIX_LEN)

int end_game;

void *context;
void *responder;
void *publisher;
msg msg_publisher;
char *password = NULL;

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

                //increase lizard score
                client_lizards[index_client].score += client_roaches[current->data.index_client].char_data[current->data.index_roaches].ch - '0';

                client_roaches[current->data.index_client].active[current->data.index_roaches] = false;

                // insert roach in inactive roaches list with associated start_time
                r.death_time = s_clock();

                r.index_client = current->data.index_client;
                r.index_roaches = current->data.index_roaches;

                check = push_fifo(&roaches_killed, r);
                if (!check) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free_exit();
                    exit(0);
                }

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

void update_lizards() {
    for (int i = 0; i < MAX_LIZARDS; i++) {
        msg_publisher.lizards[i].valid = client_lizards[i].valid;
        msg_publisher.lizards[i].score = client_lizards[i].score;
        msg_publisher.lizards[i].char_data.ch = client_lizards[i].char_data.ch;
    }
    return;
}

list_element *display_in_field(char ch, int x, int y, int index_client, 
                    int index_roaches, int element_type, list_element *head) {
                    
    if (end_game < 2) {
        square new_data;
        new_data.element_type = element_type;
        new_data.index_client = index_client;
        new_data.index_roaches = index_roaches;
        size_t send1, send2;
        bool check;

        if (ch == ' ') {
            // delete from list position xy in field
            deletelist_element(&head, new_data);

            // verificar se nesta posiçao existem mais elementos? se existirem display = True
            if (head != NULL) {
                ch = check_prioritary_element(head); 
            }
            
            
        }
        else {
            // add to list position xy in field    
            check = insertBegin(&head, new_data);
            if (!check) {
                fprintf(stderr, "Memory allocation failed\n");
                free_exit();
                exit(0);
            }

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


void tail(direction_t direction, int x, int y, bool delete, int index_client) {
    char display;
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
    int64_t end_time, inactive_time;

    while (roaches_killed != NULL) {
        end_time = s_clock();
        inactive_time = (end_time - roaches_killed->data.death_time); 
        // mvwprintw(stats_win, 1, 1, "%ld", inactive_time);
        // wrefresh(stats_win);
        
        if (inactive_time >= RESPAWN_TIME*1000) {

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

            pop_fifo(&roaches_killed);

        }      
        else {
            break;
        }
        
    }

    return;
}

void display_stats() {
    int i = 0;

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

void free_exit() {
    delwin(stats_win);
    delwin(debug_win);
    delwin(my_win);
    endwin();
    zmq_close (responder);
    zmq_close(publisher);
    zmq_ctx_destroy (context);
    client_roaches = free_safe(client_roaches);
    free3DArray(field);
    password = free_safe(password);
}

int main() {
    end_game = 0;
    
    remote_char_t m;

    size_t send1, send2;

    field = allocate3DArray();
    if (field == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_exit();
        exit(0);
    }

    //

    char port_display[MAX_PORT_STR_LEN], port_client[MAX_PORT_STR_LEN];
    char full_address_display[FULL_ADDRESS_LEN], full_address_client[FULL_ADDRESS_LEN];

    // Ask the user to enter the port for the client app
    printf("Port for client app: ");
    if (fgets(port_client, sizeof(port_client), stdin) != NULL) {
        // Remove the newline character from the end, if it exists
        port_client[strcspn(port_client, "\n")] = 0;

        // Display the entered string
        printf("Port Client: %s\n", port_client);
    } else {
        // Handle input error
        fprintf(stderr, "Error reading input.\n");
        free_exit();
        exit(EXIT_FAILURE);
    }

    // Validate the port number
    if (strlen(port_client) > 5 || atoi(port_client) <= 0 || atoi(port_client) > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        free_exit();
        exit(EXIT_FAILURE);
    }

    // Format the full address for the client
    snprintf(full_address_client, sizeof(full_address_client), "tcp://*:%s", port_client);

    // Ask the user to enter the port for the display app
    printf("Port for display app: ");
    if (fgets(port_display, sizeof(port_display), stdin) != NULL) {
        // Remove the newline character from the end, if it exists
        port_display[strcspn(port_display, "\n")] = 0;

        // Display the entered string
        printf("Port Display: %s\n", port_display);
    } else {
        // Handle input error
        fprintf(stderr, "Error reading input.\n");
        free_exit();
        exit(EXIT_FAILURE);
    }

    // Validate the port number
    if (strlen(port_display) > 5 || atoi(port_display) <= 0 || atoi(port_display) > 65535) {
        fprintf(stderr, "Invalid port number. Please provide a number between 1 and 65535.\n");
        free_exit();
        exit(EXIT_FAILURE);
    }

    // Format the full address for the display
    snprintf(full_address_display, sizeof(full_address_display), "tcp://*:%s", port_display);

    // At this point, the full_address_client and full_address_display contain the formatted strings
    printf("Full address for client app: %s\n", full_address_client);
    printf("Full address for display app: %s\n", full_address_display);

    context = zmq_ctx_new ();
    assert(context != NULL);
    responder = zmq_socket (context, ZMQ_REP);
    assert(responder != NULL);
    int rc = zmq_bind (responder, full_address_client);
    assert (rc == 0);

    publisher = zmq_socket (context, ZMQ_PUB);
    assert(publisher != NULL);
    int rc2 = zmq_bind (publisher, full_address_display);
    assert(rc2 == 0);

    
    msg_publisher.x_upd = -1;
    msg_publisher.y_upd = -1;
    for (int vv = 0; vv < WINDOW_SIZE; vv++) {
        for(int uu = 0; uu < WINDOW_SIZE; uu++) {
            msg_publisher.field[vv][uu] = ' ';
        }
    }

	struct termios oldt, newt;
    
    size_t bufsize = 100;
    int ch;

    initscr();		    	
	cbreak();				
    keypad(stdscr, true);   
	noecho();	

    password = NULL;
    // Allocate memory for the password
    password = (char *)malloc(bufsize * sizeof(char));
    if(password == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        free_exit();
        exit(0);
    }

    // Turn off echoing of characters
    tcgetattr(STDIN_FILENO, &oldt); // get current terminal attributes
    newt = oldt;
    newt.c_lflag &= ~(ECHO); // turn off ECHO
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Read password
    printw("Display-app pass: ");
    refresh();

    int i = 0, j = 0;
    while(i < 99 && (ch = getch()) != '\n') {
        password[i++] = ch;
        // addch('*'); // Display an asterisk for each character
    }
    password[i] = '\0'; // Null-terminate the string

    send1 = zmq_send(publisher, password, strlen(password), ZMQ_SNDMORE);
    assert(send1 != -1);
    send2 = zmq_send(publisher, &msg_publisher, sizeof(msg), 0);  
    assert(send2 != -1);

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);	    

    /* creates a window and draws a border */
    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0, 0);
	wrefresh(my_win);

    // creates a window for stats
    stats_win = newwin(MAX_LIZARDS + 1, 100, WINDOW_SIZE + 1, 0);

    // creates a window for debug
    debug_win = newwin(20, 100, WINDOW_SIZE + 1 + MAX_LIZARDS + 1, 0);

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

                    if (client_lizards[index_client].score >= POINTS_TO_WIN && end_game == 0) {
                        // new tail
                        end_game = 1;
                        tail(m.direction[0], pos_x_lizards, pos_y_lizards, false, 
                            index_client_lizards_id);

                        end_game = 2;
                    }
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
    free_exit();

	return 0;
}