#include <ncurses.h>
#include "remote-char.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zmq.h>
#include <pthread.h>
#include <unistd.h>
#include "zhelpers.h"
#include "common.h"
#include "score_update.pb-c.h"

// Alien space - line >=3  && line <= 18 && column >=3 && column <= 18
#define IS_ALIEN_SPACE(line, column) (line >= 3 && line <= 18 && column >= 3 && column <= 18)

// Playing Areas
#define IS_AREA_A(line, column) (column == 1 && line >= 3 && line <= 18)
#define IS_AREA_B(line, column) (line == 19 && column >= 3 && column <= 18)
#define IS_AREA_C(line, column) (line == 20 && column >= 3 && column <= 18)
#define IS_AREA_D(line, column) (column == 19 && line >= 3 && line <= 18)
#define IS_AREA_E(line, column) (line == 1 && column >= 3 && column <= 18)
#define IS_AREA_F(line, column) (column == 20 && line >= 3 && line <= 18)
#define IS_AREA_G(line, column) (line == 2 && column >= 3 && column <= 18)
#define IS_AREA_H(line, column) (column == 2 && line >= 3 && line <= 18)

/**
 * Struct: zap_info
 * ----------------
 * Contains information about a zap (shot) event.
 *
 * board_win: Pointer to the window representing the game board.
 * score_win: Pointer to the window representing the score display.
 * publisher: Pointer to the ZeroMQ publisher socket.
 * x: The x-coordinate of the zap.
 * y: The y-coordinate of the zap.
 * is_horizontal: Boolean indicating if the zap is horizontal.
 */
typedef struct zap_info
{
    WINDOW *board_win;
    WINDOW *score_win;
    void *publisher;
    int x;
    int y;
    bool is_horizontal;
} zap_info;

/**
 * Struct: alien_trial_t
 * ---------------------
 * Contains information about the alien movement trial.
 *
 * board_win: Pointer to the window representing the game board.
 * score_win: Pointer to the window representing the score display.
 * aliens_alive: Pointer to the number of alive aliens.
 * publisher: Pointer to the ZeroMQ publisher socket.
 */
typedef struct alien_trial_t
{
    WINDOW *board_win;
    WINDOW *score_win;
    int *aliens_alive;
    void *publisher;
} alien_trial_t;

// create a mutex
pthread_mutex_t mutex;

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
    // Serialize the content of the windows
    char *board_buffer = serialize_window(board_win);
    char *score_buffer = serialize_window(score_win);

    // Send the serialized content to the subscribers
    s_send(publisher, score_buffer);
    s_send(publisher, board_buffer);

    // Free the allocated memory for the serialized buffers
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
void draw_score(WINDOW *score_win, ch_info_t clients[], int client_count, void *zmq_socket)
{
    wclear(score_win);    // Clear the score window
    box(score_win, 0, 0); // Redraw the border
    mvwprintw(score_win, 1, 3, "Score");
    curs_set(0); // Hide the cursor

    // Print the scores of each client
    for (int i = 0; i < client_count; i++)
    {
        mvwprintw(score_win, i + 2, 3, "%c - %d", clients[i].ch, clients[i].score);
    }

    wrefresh(score_win); // Refresh the window to display the content

    // Initialize the Protobuf message for all scores
    ScoreUpdates updates = SCORE_UPDATES__INIT;
    updates.scores = malloc(client_count * sizeof(ScoreUpdate *));
    updates.n_scores = client_count;

    for (int i = 0; i < client_count; i++)
    {
        updates.scores[i] = malloc(sizeof(ScoreUpdate));
        score_update__init(updates.scores[i]);
        updates.scores[i]->ch = clients[i].ch;
        updates.scores[i]->score = clients[i].score;
    }

    // Serialize the message
    size_t len = score_updates__get_packed_size(&updates);
    void *buffer = malloc(len);
    score_updates__pack(&updates, buffer);

    // Send the message to the ZeroMQ socket
    zmq_send(zmq_socket, "scores ", 7, ZMQ_SNDMORE); // Topic
    zmq_send(zmq_socket, buffer, len, 0);            // Message

    // Free the memory used for the buffer and scores
    free(buffer);
    for (int i = 0; i < client_count; i++)
    {
        free(updates.scores[i]);
    }
    free(updates.scores);
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
            // Shift the remaining clients to fill the gap
            for (int j = i; j < *client_count - 1; j++)
            {
                clients[j] = clients[j + 1];
            }
            (*client_count)--; // Decrement the client count
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
    return -1; // Return -1 if the client is not found
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

    // Keep generating a random area until an unoccupied one is found
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

    // Area A - Left
    case 0:
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 1;
        break;
    // Area B - Bottom
    case 1:
        *x = 19;
        *y = (rand() % (18 - 3 + 1)) + 3;
        break;
    // Area C - Bottom
    case 2:
        *x = 20;
        *y = (rand() % (18 - 3 + 1)) + 3;
        break;
    // Area D - Right
    case 3:
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 19;
        break;
    // Area E - Top
    case 4:
        *x = 1;
        *y = (rand() % (18 - 3 + 1)) + 3;
        break;
    // Area F - Right
    case 5:
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 20;
        break;
    // Area G - Top
    case 6:
        *x = 2;
        *y = (rand() % (18 - 3 + 1)) + 3;
        break;
    // Area H - Left
    case 7:
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 2;
        break;
    default:
        break;
    }
}

/**
 * Function: remove_bullets
 * ------------------------
 * Removes bullets from the board after a delay.
 *
 * arg: A pointer to a zap_info structure containing information about the zap.
 *
 * This function does not return a value.
 *
 * The function runs as a separate thread and waits for 500 milliseconds before
 * removing bullets or aliens from the board. It then refreshes the game board
 * and notifies subscribers of the update.
 */
void *remove_bullets(void *arg)
{
    zap_info *info = (zap_info *)arg;
    WINDOW *board_win = info->board_win;
    WINDOW *score_win = info->score_win;
    void *publisher = info->publisher;
    int x = info->x;
    int y = info->y;
    bool is_horizontal = info->is_horizontal;
    usleep(500000); // Wait for 500 milliseconds

    for (int i = 1; i <= 20; i++)
    {

        if (is_horizontal)
        {
            if (mvwinch(board_win, x, i) == '-' || mvwinch(board_win, x, i) == '*')
            {
                mvwaddch(board_win, x, i, ' '); // Remove the bullet or alien
            }
        }
        else
        {
            if (mvwinch(board_win, i, y) == '|' || mvwinch(board_win, i, y) == '*')
            {
                mvwaddch(board_win, i, y, ' '); // Remove the bullet or alien
            }
        }
    }
    wrefresh(board_win);                                  // Refresh the game board to show updates
    send_to_subscribers(publisher, score_win, board_win); // Notify subscribers of the update
}

/**
 * Function: update_clients
 * ------------------------
 * Updates the status of clients based on the zap effect.
 *
 * board_win: A pointer to the window representing the game board.
 * x: The x-coordinate of the zap.
 * y: The y-coordinate of the zap.
 * ch: The character representing the client.
 * client_count: The number of clients connected.
 * clients: An array of client information structures.
 * is_horizontal: A boolean indicating if the zap is horizontal.
 *
 * This function does not return a value.
 */
void update_clients(WINDOW *board_win, int x, int y, char ch, int client_count, ch_info_t clients[], bool is_horizontal)
{
    // Find the index of the client who fired the zap
    int player = find_ch_info(clients, client_count, ch);
    for (int i = 0; i < client_count; i++)
    {
        // Check if the client is in the line of the zap
        if ((is_horizontal && clients[i].pos_x == x) || (!is_horizontal && clients[i].pos_y == y))
        {
            if (i != player)
            {
                // Disable movement and shooting for the hit client
                clients[i].move = false;
                clients[i].shoot = false;
                // Record the time the client was hit
                clients[i].hit_time = time(NULL);
            }
        }
    }
}

/**
 * Function: zap_effect
 * --------------------
 * Applies the zap effect on the board and updates the score.
 *
 * board_win: A pointer to the window representing the game board.
 * x: The x-coordinate of the zap.
 * y: The y-coordinate of the zap.
 * aliens_alive: A pointer to the number of aliens alive.
 * clients: An array of client information structures.
 * client_count: The number of clients connected.
 * ch: The character representing the client.
 *
 * Returns true if the zap is horizontal, false otherwise.
 */
bool zap_effect(WINDOW *board_win, int x, int y, int *aliens_alive, ch_info_t clients[], int client_count, char ch)
{

    // verify if shot is horizontal or vertical
    bool is_horizontal = IS_AREA_A(x, y) || IS_AREA_D(x, y) || IS_AREA_F(x, y) || IS_AREA_H(x, y);
    if (is_horizontal)
    {
        // Horizontal zap
        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, x, i) == '*')
            {
                // Increment the score of the client who fired the zap
                clients[find_ch_info(clients, client_count, ch)].score += 1;
                (*aliens_alive)--; // Decrement the number of alive aliens
            }
            if (mvwinch(board_win, x, i) == ' ' || mvwinch(board_win, x, i) == '*')
            {
                // Display the zap effect
                mvwaddch(board_win, x, i, '-');
            }
        }
    }
    else
    {
        // Vertical zap
        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, i, y) == '*')
            {
                // Increment the score of the client who fired the zap
                clients[find_ch_info(clients, client_count, ch)].score += 1;
                (*aliens_alive)--; // Decrement the number of alive aliens
            }
            if (mvwinch(board_win, i, y) == ' ' || mvwinch(board_win, i, y) == '*')
            {
                // Display the zap effect
                mvwaddch(board_win, i, y, '|');
            }
        }
    }
    wrefresh(board_win);  // Refresh the game board to show the zap effect
    return is_horizontal; // Return whether the zap was horizontal or vertical
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
void spawn_aliens(WINDOW *board_win, int number_of_aliens)
{
    int x, y;
    int count = 0;
    while (count < number_of_aliens)
    {
        // Generate random coordinates within the specified range
        x = (rand() % (18 - 3 + 1)) + 3;
        y = (rand() % (18 - 3 + 1)) + 3;

        // Check if the space is valid for aliens and is empty
        if (IS_ALIEN_SPACE(x, y) && mvwinch(board_win, x, y) == ' ')
        {
            // Place an alien at the generated coordinates
            mvwaddch(board_win, x, y, '*');
            count++;
        }
    }
    // Refresh the game board to show the newly spawned aliens
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
 * Function: update_aliens_alive
 * -----------------------------
 * Updates the number of alive aliens on the board.
 *
 * aliens_alive: Pointer to the current number of alive aliens.
 * last_aliens_alive: Pointer to the last recorded number of alive aliens.
 * iterations: Pointer to the number of iterations since the last change in the number of alive aliens.
 * board_win: Pointer to the window representing the game board.
 *
 * This function checks if the number of alive aliens has changed. If it has, it resets the iteration count.
 * If the number of alive aliens has not changed for 10 iterations, it spawns new aliens based on 10% of the current
 * number of alive aliens, with a minimum of 1 and a maximum of 256 aliens.
 */
void update_aliens_alive(int *aliens_alive, int *last_aliens_alive, int *iterations, WINDOW *board_win)
{
    if (*aliens_alive != *last_aliens_alive)
    {
        // If the number of alive aliens has changed, update the last recorded number and reset iterations
        *last_aliens_alive = *aliens_alive;
        *iterations = 0;
    }
    else
    {
        // If the number of alive aliens has not changed, increment the iteration count
        (*iterations)++;
        if (*iterations >= 10)
        {
            // If the number of iterations reaches 10, calculate the number of new aliens to spawn
            int increment = (int)(*aliens_alive * 0.1);
            if (increment < 1)
                increment = 1;
            if (*aliens_alive + increment > 256)
                increment = 256 - *aliens_alive;

            // Spawn new aliens and update the number of alive aliens
            spawn_aliens(board_win, increment);
            *aliens_alive += increment;

            // Update the last recorded number of alive aliens and reset iterations
            *last_aliens_alive = *aliens_alive;
            *iterations = 0;
        }
    }
}

/**
 * Function: move_alien
 * --------------------
 * Thread function that moves aliens on the game board.
 *
 * arg: Pointer to an alien_trial_t structure containing information about the game board and aliens.
 *
 * This function continuously moves aliens on the game board, updates the display, and sends updates to subscribers.
 */
void *move_alien(void *arg)
{

    // Cast the argument to an alien_trial_t structure
    alien_trial_t *info = (alien_trial_t *)arg;
    WINDOW *board_win = info->board_win;
    WINDOW *score_win = info->score_win;
    void *publisher = info->publisher;

    int iterations = 0;
    int last_aliens_alive = *info->aliens_alive;
    while (1)
    {
        // Iterate over the game board to find and move aliens
        for (int x = 3; x <= 18; x++)
        {
            for (int y = 3; y <= 18; y++)
            {
                pthread_mutex_lock(&mutex);
                if (mvwinch(board_win, x, y) == '*')
                {
                    // Determine a new position for the alien based on a random direction
                    direction_t direction = random_direction();
                    int x_new = x;
                    int y_new = y;
                    new_position(&x_new, &y_new, direction);
                    if (is_alien_move(board_win, x_new, y_new))
                    {
                        // Move the alien to the new position
                        wmove(board_win, x, y);
                        waddch(board_win, ' ');
                        wmove(board_win, x_new, y_new);
                        waddch(board_win, '*');
                    }
                }
                pthread_mutex_unlock(&mutex);
            }
        }
        // Refresh the game board display
        pthread_mutex_lock(&mutex);
        wrefresh(board_win);
        pthread_mutex_unlock(&mutex);

        // Send updates to subscribers
        send_to_subscribers(publisher, score_win, board_win);
        usleep(1000000);

        // Update the number of alive aliens
        update_aliens_alive(info->aliens_alive, &last_aliens_alive, &iterations, board_win);
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
    // Find the index of the client based on the character in the buffer
    int index = find_ch_info(clients, client_count, buffer.ch);
    if (index != -1 && clients[index].move)
    {
        int pos_x, pos_y;
        pos_x = clients[index].pos_x;
        pos_y = clients[index].pos_y;

        // Move to the current position and clear the character
        wmove(board_win, pos_x, pos_y);
        waddch(board_win, ' ');

        // Calculate the new position based on the direction
        new_position(&pos_x, &pos_y, buffer.direction);

        // Ensure the player stays within the same area
        if (!are_coords_in_same_area(pos_x, pos_y, clients[index].pos_x, clients[index].pos_y))
        {

            pos_x = clients[index].pos_x;
            pos_y = clients[index].pos_y;
        }

        // Update the player's position
        clients[index].pos_x = pos_x;
        clients[index].pos_y = pos_y;

        // Move to the new position and draw the character
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
    ticket[size - 1] = '\0'; // Null-terminate the string
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
            return strcmp(clients[i].ticket, ticket) == 0; // Returns true if the ticket matches
        }
    }
    return false; // Character not found or invalid ticket
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

    // Create windows for the board and score display
    WINDOW *numbers = newwin(BOARD_HEIGHT + 3, BOARD_WIDTH + 3, 0, 0);             // Window to display the board with margins for borders and coordinates.
    WINDOW *board_win = derwin(numbers, BOARD_HEIGHT + 2, BOARD_WIDTH + 2, 1, 1);  // Subwindow inside 'numbers' for the game board.
    WINDOW *score_win = newwin(BOARD_HEIGHT + 2, SCORE_WIDTH, 1, BOARD_WIDTH + 4); // Window for displaying the score.

    srand(time(NULL)); // Seed the random number generator for alien movement.

    // Initialize ZeroMQ sockets
    void *context = NULL;
    void *requester = initialize_zmq_socket(&context, ZMQ_REP, "ipc:///tmp/s1", true); // Initializes a ZeroMQ REP socket.
    void *publisher = initialize_zmq_socket(&context, ZMQ_PUB, "tcp://*:5555", true);  // Initializes a ZeroMQ PUB socket.

    // Initialize the board and score
    draw_board(numbers);                       // Draws the initial game board.
    box(board_win, 0, 0);                      // Adds a border around the board window.
    draw_score(score_win, NULL, 0, publisher); // Draws the initial score display.
    wrefresh(board_win);

    // Initialize player and game state
    srand(time(NULL));                                                                 // Seed the random number generator for player actions.
    ch_info_t clients[MAX_CLIENTS];                                                    // Array to store client information.
    bool areas_occupied[8] = {false, false, false, false, false, false, false, false}; // Tracks whether each area is occupied.
    int client_count = 0;                                                              // Number of active clients.
    int area = -1;
    int aliens_alive = MAX_ALIENS; // Keeps track of how many aliens are still alive.

    // Spawn aliens on the board
    spawn_aliens(board_win, aliens_alive); // Places aliens on the game board.

    // Initialize the mutex
    if (pthread_mutex_init(&mutex, NULL) != 0)
    {
        perror("Mutex initialization failed");
        return EXIT_FAILURE;
    }

    // Create a thread to move aliens
    pthread_t aliens_thread;
    pthread_t shoot_thread;

    alien_trial_t info = {board_win, score_win, &aliens_alive, publisher};
    int result = pthread_create(&aliens_thread, NULL, move_alien, &info); // Creates a thread to move aliens.
    if (result != 0)
    {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    curs_set(0); // Makes the cursor invisible.
    while (1)
    {
        // Receive messages from clients
        int pos_x, pos_y;
        bool finish = false;
        remote_char_t buffer;

        if (aliens_alive == 0) // Check if all aliens are defeated
        {
            pthread_cancel(aliens_thread); // Cancel the aliens thread
            wclear(board_win);             // Clear the board window
            box(board_win, 0, 0);          // Redraw the border

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
            finish = true;
        }

        if (finish)
        {
            s_sleep(5000); // Sleep for 5 seconds before exiting
            break;
        }

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
            if (validate_ticket(clients, client_count, buffer.ch, buffer.ticket))
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
                int aux_aliens = aliens_alive;

                bool is_horizontal = zap_effect(board_win, x, y, &aliens_alive, clients, client_count, buffer.ch);

                send_to_subscribers(publisher, score_win, board_win);
                zap_info info = {board_win, score_win, publisher, x, y, is_horizontal};
                pthread_create(&shoot_thread, NULL, remove_bullets, &info);
                update_clients(board_win, x, y, buffer.ch, client_count, clients, is_horizontal);
                draw_score(score_win, clients, client_count, publisher); // Update the score.

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
            draw_score(score_win, clients, client_count, publisher);
        }

        pthread_mutex_lock(&mutex);
        wrefresh(board_win); // Refresh the board window to show updates.
        pthread_mutex_unlock(&mutex);
        s_send(requester, "OK"); // Send a response to the client.
        send_to_subscribers(publisher, score_win, board_win);
    }
    // Finalize ncurses
    delwin(board_win);
    delwin(numbers);
    delwin(score_win);
    endwin();

    return 0;
}
