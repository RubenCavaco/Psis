#include <zmq.h>
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "zhelpers.h"
#include "common.h"

/**
 * Function: deserialize_window
 * ----------------------------
 * Deserializes a buffer into a window's contents.
 *
 * win: A pointer to the window to be updated.
 * buffer: A buffer containing the serialized window content.
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
 * Function: main
 * --------------
 * Entry point of the program. Initializes ZeroMQ context and requester socket,
 * sets up ncurses, and continuously updates the display windows with data received from the server.
 *
 * argc: The number of command-line arguments.
 * argv: An array of command-line arguments.
 *
 * Returns 0 on successful execution.
 */
int main(int argc, char *argv[])
{
    // Initialize ZeroMQ context and requester socket
    void *context = NULL;
    void *requester = initialize_zmq_socket(&context, ZMQ_SUB, "tcp://localhost:5555", false);
    zmq_setsockopt(requester, ZMQ_SUBSCRIBE, "", 0);

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

    curs_set(0);
    while (1)
    {
        int rows, cols;
        getmaxyx(score_win, rows, cols);
        char score_buffer[rows * cols];

        getmaxyx(board_win, rows, cols);
        char board_buffer[rows * cols];



        zmq_recv(requester, score_buffer, rows * cols, 0);
        zmq_recv(requester, board_buffer, rows * cols, 0);

        deserialize_window(&score_win, score_buffer);
        deserialize_window(&board_win, board_buffer);

        wrefresh(score_win);
        wrefresh(board_win);
    }

    // Clean up
    delwin(board_win);
    delwin(score_win);
    delwin(numbers);
    zmq_close(requester);
    zmq_ctx_destroy(context);
    endwin();

    return 0;
}