#include <stdlib.h>
#include <zmq.h>
#include "zhelpers.h"
#include <string.h>
#include <stdio.h>
#include <ncurses.h>
#include <unistd.h>
#include "remote-char.h"
#include "common.h"

/**
 * Function: draw_board
 * --------------------
 * Draws the game board on the provided ncurses window.
 *
 * board_win: Pointer to the ncurses window where the board will be drawn.
 *
 * This function clears the window, adds coordinate numbers on the top and
 * left borders, and refreshes the window to display the updates.
 */
void draw_board(WINDOW *board_win)
{
    wclear(board_win); // Clears the window before drawing.

    // Adds coordinates on the top border.
    for (int i = 1; i <= BOARD_WIDTH; i++)
    {
        mvwprintw(board_win, 0, i + 1, "%d", i % 10); // Y=0 for the top border.
    }

    // Adds coordinates on the left border.
    for (int i = 1; i <= BOARD_HEIGHT; i++)
    {
        mvwprintw(board_win, i + 1, 0, "%d", i % 10); // X=0 for the left border.
    }

    wrefresh(board_win); // Refreshes the window to show the content.
}

/**
 * Function: send_message
 * ----------------------
 * Sends a message through a ZeroMQ socket.
 *
 * socket: The ZeroMQ socket used to send the message.
 * buffer: Pointer to the data to be sent.
 * size: The size of the data to be sent.
 *
 * If the message cannot be sent, the function displays an error message and exits.
 */
void send_message(void *socket, void *buffer, size_t size)
{
    if (zmq_send(socket, buffer, size, 0) == -1)  
    { 
        perror("Error sending the message"); 
        exit(1); // Exits on error.
    }
}   

/**
 * Function: receive_message
 * -------------------------
 * Receives a message through a ZeroMQ socket.
 *
 * socket: The ZeroMQ socket used to receive the message.
 * buffer: Pointer to the buffer where the received data will be stored.
 * size: The size of the buffer.
 *
 * If the message cannot be received, the function displays an error message and exits.
 */
void receive_message(void *socket, void *buffer, size_t size)
{   
    if (zmq_recv(socket, buffer, size, 0) == -1)
    {
        perror("Error receiving the message");
        exit(1); // Exits on error.
    }  
}    
/**
 * Function: initialize_zmq_socket
 * -------------------------------
 * Initializes a ZeroMQ socket and connects or binds it to the specified endpoint.
 *
 * context: Pointer to a ZeroMQ context. If NULL, a new context will be created.
 * socket_type: The type of the socket (e.g., ZMQ_REQ, ZMQ_REP, ZMQ_PUB, etc.).
 * endpoint: The endpoint to connect to or bind to (e.g., "tcp://*:5555").
 * is_bind: A boolean flag indicating whether the socket should bind (true) or connect (false).
 *
 * Returns a pointer to the initialized ZeroMQ socket.
 * If there is an error during initialization, the function displays an error message and exits.
 */
void *initialize_zmq_socket(void **context, int socket_type, const char *endpoint, bool is_bind)
{
    if (*context == NULL)
    {
        *context = zmq_ctx_new(); // Creates a new ZeroMQ context if it doesn't already exist.
    }
    void *socket = zmq_socket(*context, socket_type); // Creates a new ZeroMQ socket.
    int rc;

    if (is_bind)
    {
        rc = zmq_bind(socket, endpoint); // Binds the socket to the endpoint.
    }
    else
    {
        rc = zmq_connect(socket, endpoint); // Connects the socket to the endpoint.
    }

    if (rc != 0)
    {
        // Displays an error message and cleans up resources if the operation fails.
        printf("Error %s to endpoint %s: %d\n", is_bind ? "binding" : "connecting", endpoint, rc);
        zmq_close(socket);
        zmq_ctx_destroy(*context);
        exit(1); // Exits on error.
    }

    return socket; // Returns the initialized socket.
}
