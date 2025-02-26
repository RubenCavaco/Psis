#include <ncurses.h>
#include <zmq.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include "zhelpers.h"
#include "remote-char.h"
#include "common.h"

typedef struct disp_info
{
    WINDOW *board_win;
    WINDOW *score_win;
} disp_info;

int aliens_alive = MAX_ALIENS;
time_t last_move_time = 0;

pthread_mutex_t mutex;

/**
 * Function: processKeyBoard
 * -------------------------
 * Processes keyboard input and updates the remote_char_t structure accordingly.
 *
 * key: The key pressed by the user.
 * m: A pointer to a remote_char_t structure that will be updated based on the key pressed.
 *
 * This function identifies the key pressed and sets the message type and direction
 * in the remote_char_t structure. Unsupported keys are handled with a default case
 * that sets an invalid message type.
 */
void processKeyBoard(int key, remote_char_t *m)
{
    switch (key)
    {
    case KEY_LEFT:
        m->msg_type = 1;
        m->direction = LEFT;
        break;
    case KEY_RIGHT:
        m->msg_type = 1;
        m->direction = RIGHT;
        break;
    case KEY_DOWN:
        m->msg_type = 1;
        m->direction = DOWN;
        break;
    case KEY_UP:
        m->msg_type = 1;
        m->direction = UP;
        break;
    case 'q':
        m->msg_type = 3;
        break;
    case ' ':
        m->msg_type = 2;
        break;
    default:
        key = '?';
        m->msg_type = -1;
        break;
    }
}

/**
 * Function: deserialize_window
 * ----------------------------
 * Deserializes a buffer into the content of a window.
 *
 * win: A pointer to the window to be updated.
 * buffer: A buffer containing the serialized content of the window.
 *
 * This function does not return a value.
 */
void deserialize_window(WINDOW **win, char *buffer)
{
    int rows, cols;
    getmaxyx(*win, rows, cols);
    for (int y = 1; y < rows - 1; y++)
    {
        for (int x = 1; x < cols - 1; x++)
        {
            mvwaddch(*win, y, x, buffer[y * cols + x]);
        }
    }
}

/**
 * Function: display
 * -----------------
 * Thread function that displays the content of the windows.
 *
 * arg: Argument passed to the thread.
 *
 * This function does not return a value.
 */
void *display(void *arg)
{
    // Initialize ncurses and create windows
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();

    WINDOW *numbers = newwin(BOARD_HEIGHT + 3, BOARD_WIDTH + 3, 0, 0); // +3 to include borders and coordinate numbers
    WINDOW *board_win = derwin(numbers, BOARD_HEIGHT + 2, BOARD_WIDTH + 2, 1, 1);
    WINDOW *score_win = newwin(BOARD_HEIGHT + 2, SCORE_WIDTH, 1, BOARD_WIDTH + 4);

    // Draw initial board structure
    draw_board(numbers);
    box(board_win, 0, 0);
    box(score_win, 0, 0);
    wrefresh(board_win);
    wrefresh(score_win);

    void *context = NULL;
    void *subscriber = initialize_zmq_socket(&context, ZMQ_SUB, "tcp://localhost:5555", false);
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);

    int score_rows, score_cols;
    getmaxyx(score_win, score_rows, score_cols);
    char score_buffer[score_rows * score_cols];

    int board_rows, board_cols;
    getmaxyx(board_win, board_rows, board_cols);
    char board_buffer[board_rows * board_cols];

    curs_set(0);
    while (1)
    {

        zmq_recv(subscriber, score_buffer, score_rows * score_cols, 0);
        zmq_recv(subscriber, board_buffer, board_rows * board_cols, 0);

        deserialize_window(&score_win, score_buffer);
        deserialize_window(&board_win, board_buffer);

        wrefresh(score_win);
        wrefresh(board_win);
    }
}

int main()
{
    // Initialize ZeroMQ context and socket
    void *context = NULL;
    void *requester = initialize_zmq_socket(&context, ZMQ_REQ, "ipc:///tmp/s1", false);

    remote_char_t m, response;
    m.msg_type = 0;

    send_message(requester, &m, sizeof(m));
    receive_message(requester, &response, sizeof(response));

    if (strcmp(response.ticket, "FULL") == 0)
    {
        printf("Server is full\n");
        zmq_close(requester);
        zmq_ctx_destroy(context);
        exit(1);
    }

    m.ch = response.ch;
    strcpy(m.ticket, response.ticket);

    // Initialize ncurses
    initscr();            /* Start curses mode 		*/
    cbreak();             /* Line buffering disabled	*/
    keypad(stdscr, TRUE); /* We get arrows etc...*/
    noecho();             /* Don't echo() while we do getch */

    // Initialize the mutex
    if (pthread_mutex_init(&mutex, NULL) != 0)
    {
        perror("Mutex initialization failed");
        return EXIT_FAILURE;
    }

    pthread_t display_thread;
    pthread_create(&display_thread, NULL, display, NULL);

    curs_set(0); // Hide the cursor

    int key;
    do
    {
        key = getch();
        processKeyBoard(key, &m);

        if (m.msg_type != -1)
        {
            send_message(requester, &m, sizeof(m));
            receive_message(requester, &m, sizeof(m));
        }

        if (key == 'q')
        {

            // destroy the thread
            pthread_cancel(display_thread);

            // Disconnect from the server
            zmq_close(requester);
            zmq_ctx_destroy(context);
            endwin(); /* End curses mode		  */
            exit(0);
        }
    } while (key != 27); // Continue while the key pressed is not ESC (code 27)
}
