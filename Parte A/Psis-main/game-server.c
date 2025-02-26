#include <ncurses.h>
#include "remote-char.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zmq.h>
#include "zhelpers.h"
#include <unistd.h>
#include "common.h"

// Alien space - line >=3  && line <= 18 && column >=3 && column <= 18
#define IS_ALIEN_SPACE(line, column) (line >= 3 && line <= 18 && column >= 3 && column <= 18)

// Playing Areas
#define IS_AREA_A(line, column) (column == 1 && line >= 3 && line <= 18)
#define IS_AREA_B(line, column) (line == 19 && column >= 3 && column <= 18)
#define IS_AREA_C(line, column) (line == 20 && column >= 3 && column <= 18)
#define IS_AREA_D(line, column) (column == 19 && line >= 3 && line <= 18)
#define IS_AREA_E(line, column) (line == 1 && column >= 3 && column <= 18)
#define IS_AREA_F(line, column) (column == 20 && line >= 3 && column <= 18)
#define IS_AREA_G(line, column) (line == 2 && column >= 3 && column <= 18)
#define IS_AREA_H(line, column) (column == 2 && line >= 3 && line <= 18)

/**
 * Function: random_direction
 * --------------------------
 * Generates a random direction for alien movement.
 *
 * Returns a random direction_t value.
 */
direction_t random_direction()
{
    return (direction_t)(rand() % 4);
}

/**
 * Function: serialize_window
 * --------------------------
 * Serializes the contents of a window into a buffer.
 *
 * win: A pointer to the window to be serialized.
 *
 * Returns a pointer to the buffer containing the serialized window content.
 */
char *serialize_window(WINDOW *win)
{
    int rows, cols;
    getmaxyx(win, rows, cols);
    char *buffer = (char *)malloc(rows * cols * sizeof(char));
    memset(buffer, 0, rows * cols * sizeof(char) + 1);

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            buffer[y * cols + x] = mvwinch(win, y, x) & A_CHARTEXT;
        }
    }
    buffer[rows * cols] = '\0';
    return buffer;
}

/**
 * Function: send_to_subscribers
 * -----------------------------
 * Serializes the contents of the score and board windows and sends them to subscribers.
 *
 * publisher: A pointer to the ZeroMQ publisher socket.
 * score_win: A pointer to the window representing the score.
 * board_win: A pointer to the window representing the game board.
 *
 * This function does not return a value.
 */
void send_to_subscribers(void *publisher, void *score_win, void *board_win)
{
    char *board_buffer = serialize_window(board_win);
    char *score_buffer = serialize_window(score_win);

    s_send(publisher, score_buffer);
    s_send(publisher, board_buffer);
    free(score_buffer);
    free(board_buffer);
}

/**
 * Function: new_position
 * ----------------------
 * Updates the coordinates based on the given direction.
 *
 * x: A pointer to the x-coordinate.
 * y: A pointer to the y-coordinate.
 * direction: The direction to move.
 *
 * This function does not return a value.
 */
void new_position(int *x, int *y, direction_t direction)
{
    switch (direction)
    {
    case UP:
        (*x)--;
        if (*x == 0)
            *x = 1;
        break;
    case DOWN:
        (*x)++;
        if (*x == WINDOW_SIZE - 1)
            *x = WINDOW_SIZE - 2;
        break;
    case LEFT:
        (*y)--;
        if (*y == 0)
            *y = 1;
        break;
    case RIGHT:
        (*y)++;
        if (*y == WINDOW_SIZE - 1)
            *y = WINDOW_SIZE - 2;
        break;
    default:
        break;
    }
}

/**
 * Function: draw_score
 * --------------------
 * Draws the score window with the current scores of the clients.
 *
 * score_win: A pointer to the window representing the score.
 * clients: An array of client information structures.
 * client_count: The number of clients connected.
 *
 * This function does not return a value.
 */
void draw_score(WINDOW *score_win, ch_info_t clients[], int client_count)
{

    wclear(score_win);    // Clear the score window
    box(score_win, 0, 0); // Redraw the border
    mvwprintw(score_win, 1, 3, "Score");

    for (int i = 0; i < client_count; i++)
    {
        mvwprintw(score_win, i + 2, 3, "%c - %d", clients[i].ch, clients[i].score);
    }

    wrefresh(score_win); // Atualiza a janela para exibir o conteúdo
}

/**
 * Function: add_client
 * --------------------
 * Adds a new client to the clients array.
 *
 * clients: An array of client information structures.
 * client_count: A pointer to the number of clients connected.
 * ch: The character representing the client.
 * pos_x: The x-coordinate of the client's position.
 * pos_y: The y-coordinate of the client's position.
 * ticket: The ticket string for the client.
 *
 * This function does not return a value.
 */
void add_client(ch_info_t clients[], int *client_count, int ch, int pos_x, int pos_y, char ticket[7])
{
    clients[*client_count].ch = ch;
    clients[*client_count].pos_x = pos_x;
    clients[*client_count].pos_y = pos_y;
    clients[*client_count].score = 0;
    clients[*client_count].move = true;
    clients[*client_count].shoot = true;
    (*client_count)++;
}

/**
 * Function: remove_client
 * -----------------------
 * Removes a client from the clients array.
 *
 * clients: An array of client information structures.
 * client_count: A pointer to the number of clients connected.
 * ch: The character representing the client to be removed.
 *
 * This function does not return a value.
 */
void remove_client(ch_info_t clients[], int *client_count, int ch)
{
    for (int i = 0; i < *client_count; i++)
    {
        if (clients[i].ch == ch)
        {
            for (int j = i; j < *client_count - 1; j++)
            {
                clients[j] = clients[j + 1];
            }
            (*client_count)--;
            break;
        }
    }
}

/**
 * Function: find_ch_info
 * ----------------------
 * Finds the index of a client in the clients array based on the character.
 *
 * clients: An array of client information structures.
 * client_count: The number of clients connected.
 * ch: The character representing the client.
 *
 * Returns the index of the client if found, -1 otherwise.
 */
int find_ch_info(ch_info_t clients[], int client_count, int ch)
{
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].ch == ch)
        {
            return i;
        }
    }
    return -1;
}

/**
 * Function: get_player_area
 * -------------------------
 * Determines the area of the board where the given coordinates are located.
 *
 * line: The x-coordinate.
 * column: The y-coordinate.
 *
 * Returns a character representing the area.
 */
char get_player_area(int line, int column)
{
    if (IS_AREA_A(line, column))
        return 'A';
    if (IS_AREA_B(line, column))
        return 'B';
    if (IS_AREA_C(line, column))
        return 'C';
    if (IS_AREA_D(line, column))
        return 'D';
    if (IS_AREA_E(line, column))
        return 'E';
    if (IS_AREA_F(line, column))
        return 'F';
    if (IS_AREA_G(line, column))
        return 'G';
    if (IS_AREA_H(line, column))
        return 'H';
    return '\0'; // Return null character if not in any area
}

/**
 * Function: are_coords_in_same_area
 * ---------------------------------
 * Checks if two sets of coordinates are in the same area.
 *
 * line1: The x-coordinate of the first position.
 * column1: The y-coordinate of the first position.
 * line2: The x-coordinate of the second position.
 * column2: The y-coordinate of the second position.
 *
 * Returns true if the coordinates are in the same area, false otherwise.
 */
bool are_coords_in_same_area(int line1, int column1, int line2, int column2)
{
    char area1 = get_player_area(line1, column1);
    char area2 = get_player_area(line2, column2);
    return area1 != '\0' && area1 == area2;
}

/**
 * Function: ChoosePlayerArea
 * --------------------------
 * Chooses a random player area that is not occupied.
 *
 * areas_occupied: An array indicating which areas are occupied.
 *
 * Returns the index of the chosen area.
 */
int ChoosePlayerArea(bool areas_occupied[])
{
    int area = (rand() % 8);

    while (areas_occupied[area] == true)
    {
        area = (rand() % 8);
    }

    return area;
}

/**
 * Function: ChoosePlayerPosition
 * ------------------------------
 * Chooses a random position within the given area.
 *
 * area: The index of the area.
 * x: A pointer to the x-coordinate.
 * y: A pointer to the y-coordinate.
 *
 * This function does not return a value.
 */
void ChoosePlayerPosition(int area, int *x, int *y)
{
    switch (area)
    {

    // Area A    1-Esquerda
    case 0:
        /**x = 3;*/
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 1;
        break;
    // Area B    1-Baixo
    case 1:
        *x = 19;
        //*x = (rand() % (18 - 3 + 1)) + 3;
        *y = (rand() % (18 - 3 + 1)) + 3;
        break;
    // Area C    2-Baixo
    case 2:
        *x = 20;
        *y = (rand() % (18 - 3 + 1)) + 3;
        break;
    // Area D    1-Direita
    case 3:
        /**x = 18;*/
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 19;
        break;
    // Area E    1-Cima
    case 4:
        *x = 1;
        *y = (rand() % (18 - 3 + 1)) + 3;
        /**y = 3;*/
        break;
    // Area F    2-Direita
    case 5:
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 20;
        break;
    // Area G    2-Cima
    case 6:
        *x = 2;
        *y = (rand() % (18 - 3 + 1)) + 3;
        /**y = 3;*/
        break;
        // Area H    2-Esquerda

    case 7:
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 2;
        /**y = 2;*/
        break;
    default:
        break;
    }
}

/**
 * Function: Shoot
 * ---------------
 * Handles the shooting logic for a player.
 *
 * board_win: A pointer to the window representing the game board.
 * score_win: A pointer to the window representing the score.
 * publisher: A pointer to the ZeroMQ publisher socket.
 * x: The x-coordinate of the player.
 * y: The y-coordinate of the player.
 * ch: The character representing the player.
 * client_count: The number of clients connected.
 * clients: An array of client information structures.
 * aliens_alive: The number of aliens currently alive.
 *
 * Returns the updated number of aliens alive.
 */
int Shoot(WINDOW *board_win, WINDOW *score_win, void *publisher, int x, int y, char ch, int client_count, ch_info_t clients[], int aliens_alive)
{
    // Disparo na horizontal
    char *buffer;
    if (IS_AREA_A(x, y) || IS_AREA_D(x, y) || IS_AREA_F(x, y) || IS_AREA_H(x, y))
    {
        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, x, i) == ' ')
            {
                mvwaddch(board_win, x, i, '-');
            }
        }
        wrefresh(board_win);

        send_to_subscribers(publisher, score_win, board_win);
        // remove every '-' after 0.5 seconds
        usleep(500000);

        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, x, i) == '*')
            {
                clients[find_ch_info(clients, client_count, ch)].score += 1; // add points to what shot
                aliens_alive--;
            }
        }

        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, x, i) == '-' || mvwinch(board_win, x, i) == '*')
            {
                mvwaddch(board_win, x, i, ' ');
            }
        }

        send_to_subscribers(publisher, score_win, board_win);

        int player = find_ch_info(clients, client_count, ch);

        for (int i = 0; i < client_count; i++)
        {
            if (clients[i].pos_x == x && i != player) // Verifica o outro cliente está na mesma coluna.
            {
                clients[i].move = false;
                clients[i].shoot = false;
                clients[i].hit_time = time(NULL);
            }
        }
    }
    // Disparo na vertical
    else if (IS_AREA_B(x, y) || IS_AREA_C(x, y) || IS_AREA_E(x, y) || IS_AREA_G(x, y))
    {
        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, i, y) == ' ')
            {
                mvwaddch(board_win, i, y, '|');
            }
        }
        wrefresh(board_win);

        send_to_subscribers(publisher, score_win, board_win);
        // remove every '-' after 0.5 seconds
        usleep(500000);

        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, i, y) == '*')
            {
                // adicionar pontos ao que disparou;
                clients[find_ch_info(clients, client_count, ch)].score += 1;
                aliens_alive--; // Reduz o número de aliens vivos
            }
        }

        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, i, y) == '|' || mvwinch(board_win, i, y) == '*')
            {
                mvwaddch(board_win, i, y, ' ');
            }
        }

        send_to_subscribers(publisher, score_win, board_win);
        int player = find_ch_info(clients, client_count, ch);
        for (int i = 0; i < client_count; i++)
        {
            if (clients[i].pos_y == y && i != player) // Verifica o outro cliente está na mesma coluna.
            {
                // adicionar pontos ao que disparou

                clients[player].score += 1;
                clients[i].move = false;
                clients[i].shoot = false;
                clients[i].hit_time = time(NULL);
            }
        }
    }
    return aliens_alive;
}

/**
 * Function: spawn_aliens
 * ----------------------
 * Spawns aliens at random positions on the board.
 *
 * board_win: A pointer to the window representing the game board.
 *
 * This function does not return a value.
 */
void spawn_aliens(WINDOW *board_win)
{
    int x, y;
    int count = 0;
    while (count < MAX_ALIENS)
    {
        x = (rand() % (18 - 3 + 1)) + 3;
        y = (rand() % (18 - 3 + 1)) + 3;
        if (IS_ALIEN_SPACE(x, y) && mvwinch(board_win, x, y) == ' ')
        {
            mvwaddch(board_win, x, y, '*');
            count++;
        }
    }
    wrefresh(board_win);
}

/**
 * Function: is_alien_move
 * -----------------------
 * Checks if an alien can move to the given coordinates.
 *
 * board_win: A pointer to the window representing the game board.
 * x: The x-coordinate.
 * y: The y-coordinate.
 *
 * Returns true if the alien can move to the coordinates, false otherwise.
 */
bool is_alien_move(WINDOW *board_win, int x, int y)
{
    if (IS_ALIEN_SPACE(x, y) && mvwinch(board_win, x, y) == ' ')
    {
        return true;
    }
    return false;
}

/**
 * Function: move_alien
 * --------------------
 * Moves aliens to new positions on the board.
 *
 * board_win: A pointer to the window representing the game board.
 *
 * This function does not return a value.
 */
void move_alien(WINDOW *board_win)
{
    for (int x = 3; x <= 18; x++)
    {
        for (int y = 3; y <= 18; y++)
        {
            if (mvwinch(board_win, x, y) == '*')
            {
                direction_t direction = random_direction();
                int x_new = x;
                int y_new = y;
                new_position(&x_new, &y_new, direction);
                if (is_alien_move(board_win, x_new, y_new))
                {
                    wmove(board_win, x, y);
                    waddch(board_win, ' ');
                    wmove(board_win, x_new, y_new);
                    waddch(board_win, '*');
                }
            }
        }
    }
}

/**
 * Function: move_player
 * ---------------------
 * Moves a player on the board based on the direction provided in the buffer.
 *
 * board_win: A pointer to the window representing the game board.
 * client_count: The number of clients connected.
 * clients: An array of client information structures.
 * buffer: A structure containing the character and direction information.
 *
 * This function does not return a value.
 */
void move_player(WINDOW *board_win, int client_count, ch_info_t clients[], remote_char_t buffer)
{
    int index = find_ch_info(clients, client_count, buffer.ch);
    if (index != -1 && clients[index].move)
    {
        int pos_x, pos_y;
        pos_x = clients[index].pos_x;
        pos_y = clients[index].pos_y;
        wmove(board_win, pos_x, pos_y);
        waddch(board_win, ' ');
        new_position(&pos_x, &pos_y, buffer.direction);
        if (!are_coords_in_same_area(pos_x, pos_y, clients[index].pos_x, clients[index].pos_y))
        {
            pos_x = clients[index].pos_x;
            pos_y = clients[index].pos_y;
        }

        // Atualiza a posiçao do jogador
        clients[index].pos_x = pos_x;
        clients[index].pos_y = pos_y;

        wmove(board_win, pos_x, pos_y);
        waddch(board_win, buffer.ch | A_BOLD);
    }
}

/**
 * Function: generate_ticket
 * -------------------------
 * Generates a random ticket string.
 *
 * ticket: A pointer to the buffer where the ticket will be stored.
 * size: The size of the ticket buffer.
 *
 * This function does not return a value.
 */
void generate_ticket(char *ticket, size_t size)
{
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (size_t i = 0; i < size - 1; i++)
    {
        ticket[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    ticket[size - 1] = '\0';
}

/**
 * Function: validate_ticket
 * -------------------------
 * Validates a ticket for a given character.
 *
 * clients: An array of client information structures.
 * client_count: The number of clients connected.
 * ch: The character to validate.
 * ticket: The ticket string to validate.
 *
 * Returns true if the ticket is valid, false otherwise.
 */
bool validate_ticket(ch_info_t clients[], int client_count, char ch, char ticket[7])
{
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].ch == ch)
        {
            return strcmp(clients[i].ticket, ticket) == 0; // Retorna verdadeiro se o ticket corresponder
        }
    }
    return false; // Caractere não encontrado ou ticket inválido
}

/**
 * Function: update_client_status
 * ------------------------------
 * Updates the status of clients based on the current time.
 *
 * clients: An array of client information structures.
 * client_count: The number of clients connected.
 * current_time: The current time.
 *
 * This function does not return a value.
 */
void update_client_status(ch_info_t clients[], int client_count, time_t current_time)
{
    for (int i = 0; i < client_count; i++)
    {
        if (!clients[i].move && (current_time - clients[i].hit_time) >= 10)
        {
            clients[i].move = true;  // Allow movement after 10 seconds
            clients[i].shoot = true; // Allow shooting after 10 seconds
        }

        if (!clients[i].shoot && (current_time - clients[i].shoot_time) >= 3 && clients[i].move)
        {
            clients[i].shoot = true; // Allow shooting after 3 seconds
        }
    }
}

int main()
{
    // Initialize ncurses
    initscr();            // Initializes the ncurses library.
    keypad(stdscr, TRUE); // Enables the use of special keys like arrow keys.
    noecho();             // Disables automatic echoing of typed characters.
    cbreak();             // Disables input buffering, making characters immediately available.
    curs_set(0);          // Makes the cursor invisible.

    int aliens_alive = MAX_ALIENS; // Keeps track of how many aliens are still alive.

    // Create windows for the board and score display
    WINDOW *numbers = newwin(BOARD_HEIGHT + 3, BOARD_WIDTH + 3, 0, 0);             // Window to display the board with margins for borders and coordinates.
    WINDOW *board_win = derwin(numbers, BOARD_HEIGHT + 2, BOARD_WIDTH + 2, 1, 1);  // Subwindow inside 'numbers' for the game board.
    WINDOW *score_win = newwin(BOARD_HEIGHT + 2, SCORE_WIDTH, 1, BOARD_WIDTH + 4); // Window for displaying the score.

    // Initialize the board and score
    draw_board(numbers);            // Draws the initial game board.
    box(board_win, 0, 0);           // Adds a border around the board window.
    draw_score(score_win, NULL, 0); // Draws the initial score display.
    wrefresh(board_win);            // Refreshes the board window to show updates.

    // Fork a new process
    pid_t pid = fork(); // Creates a new process.

    if (pid < 0)
    {
        // Handle error if fork fails
        printf("Error creating the process\n");
        exit(1);
    }
    else if (pid == 0)
    {
        // Child process: handles alien movement
        srand(time(NULL)); // Seed the random number generator for alien movement.
        void *context1 = NULL;
        void *requester1 = initialize_zmq_socket(&context1, ZMQ_REQ, "ipc:///tmp/s1", false); // Initializes a ZeroMQ REQ socket.

        while (1)
        {
            int x, y;
            remote_char_t buffer;
            buffer.msg_type = 1; // Message type for alien movement.
            buffer.ch = '*';     // Represents the alien character.

            send_message(requester1, &buffer, sizeof(buffer));    // Sends a message to the server.
            receive_message(requester1, &buffer, sizeof(buffer)); // Receives a response.
            usleep(1000000);                                      // Pauses for 1 second between movements.
        }
    }
    else
    {
        // Parent process: handles player input and game logic
        void *context = NULL;
        void *requester = initialize_zmq_socket(&context, ZMQ_REP, "ipc:///tmp/s1", true); // Initializes a ZeroMQ REP socket.
        void *publisher = initialize_zmq_socket(&context, ZMQ_PUB, "tcp://*:5555", true);  // Initializes a ZeroMQ PUB socket.

        // Initialize ncurses
        srand(time(NULL));                                                                 // Seed the random number generator for player actions.
        ch_info_t clients[MAX_CLIENTS];                                                    // Array to store client information.
        bool areas_occupied[8] = {false, false, false, false, false, false, false, false}; // Tracks whether each area is occupied.
        int client_count = 0;                                                              // Number of active clients.
        int area = -1;                                                                     // Area assigned to a new player.

        // Spawn aliens on the board
        spawn_aliens(board_win); // Places aliens on the game board.

        while (1)
        {
            // Receive messages from clients
            remote_char_t buffer;
            int pos_x, pos_y;
            bool finish = false;

            if (aliens_alive == 0) // Check if all aliens are defeated
            {
                // Determine the player with the highest score
                int max_score = 0;
                char winner_ch;
                for (int i = 0; i < client_count; i++)
                {
                    if (clients[i].score > max_score)
                    {
                        max_score = clients[i].score;
                        winner_ch = clients[i].ch;
                    }
                }

                wrefresh(score_win);
                wrefresh(numbers);

                // Print the winner on the board
                mvwprintw(board_win, 1, 1, "Player %c wins", winner_ch);
                wrefresh(board_win);
                send_to_subscribers(publisher, score_win, board_win); // Send final board state to subscribers.

                // Terminate the server
                zmq_close(requester);
                zmq_close(publisher);
                zmq_ctx_destroy(context);
                zmq_ctx_destroy(context);
                finish = true;
            }
            if (finish == true)
                continue;

            receive_message(requester, &buffer, sizeof(buffer)); // Receives a message from the client.
            time_t current_time = time(NULL);                    // Get the current time.

            // Update player statuses to check if they can move
            update_client_status(clients, client_count, current_time);

            // Process message types: 0 - join, 1 - move, 2 - fire, 3 - leave
            if (buffer.msg_type == 0)
            {
                if (client_count == MAX_CLIENTS) // Check if the maximum number of clients is reached
                {
                    strcpy(buffer.ticket, "FULL");
                    send_message(requester, &buffer, sizeof(buffer));
                }
                else
                {
                    area = ChoosePlayerArea(areas_occupied); // Assign a free area to the new player.
                    areas_occupied[area] = true;
                    ChoosePlayerPosition(area, &pos_x, &pos_y); // Select a position in the assigned area.

                    char ch_client = area + 'A';                                                               // Assign a character based on the area.
                    generate_ticket(clients[client_count].ticket, sizeof(clients[client_count].ticket));       // Generate a unique ticket for the client.
                    add_client(clients, &client_count, ch_client, pos_x, pos_y, clients[client_count].ticket); // Add the client to the list.

                    // Since the count has been incremented, the current client is at index client_count - 1.
                    strcpy(buffer.ticket, clients[client_count - 1].ticket);
                    buffer.ch = ch_client;

                    wmove(board_win, pos_x, pos_y);        // Move to the player's position.
                    waddch(board_win, ch_client | A_BOLD); // Display the player's character on the board.
                    send_message(requester, &buffer, sizeof(buffer));
                }
            }
            if (buffer.msg_type == 1)
            {
                // Check if the player can move and if the move is valid
                if (buffer.ch == '*')
                {
                    move_alien(board_win); // Move the alien.
                }
                else if (validate_ticket(clients, client_count, buffer.ch, buffer.ticket))
                {
                    move_player(board_win, client_count, clients, buffer); // Move the player.
                }
            }
            else if (buffer.msg_type == 2 && validate_ticket(clients, client_count, buffer.ch, buffer.ticket))
            {
                int index = find_ch_info(clients, client_count, buffer.ch);
                int x = clients[index].pos_x;
                int y = clients[index].pos_y;

                if (clients[index].shoot == true) // Check if the player can shoot
                {
                    aliens_alive = Shoot(board_win, score_win, publisher, x, y, buffer.ch, client_count, clients, aliens_alive);
                    clients[index].shoot_time = time(NULL); // Record the shoot time.
                    clients[index].shoot = false;           // Prevent the player from shooting again immediately.
                }
            }
            else if (buffer.msg_type == 3 && validate_ticket(clients, client_count, buffer.ch, buffer.ticket))
            {
                int index = find_ch_info(clients, client_count, buffer.ch);
                wmove(board_win, clients[index].pos_x, clients[index].pos_y);
                waddch(board_win, ' ');                                                                    // Clear the player's position.
                areas_occupied[get_player_area(clients[index].pos_x, clients[index].pos_y) - 'A'] = false; // Mark the area as unoccupied.
                remove_client(clients, &client_count, buffer.ch);                                          // Remove the client from the list.
            }

            s_send(requester, "OK"); // Send acknowledgment to the client.

            // Update the score and board
            draw_score(score_win, clients, client_count);
            wrefresh(board_win);

            // Send the board and score to subscribers
            send_to_subscribers(publisher, score_win, board_win);
        }
        // Finalize ncurses
        delwin(board_win);
        delwin(numbers);
        delwin(score_win);
        endwin();
    }

    return 0;
}
