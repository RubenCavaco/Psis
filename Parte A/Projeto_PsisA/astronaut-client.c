#include <ncurses.h>
#include <zmq.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "zhelpers.h"
#include "remote-char.h"
#include "common.h"

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

int main()
{
    // Initialize ZeroMQ context and socket
    void *context = NULL;
    void *requester = initialize_zmq_socket(&context, ZMQ_REQ, "ipc:///tmp/s1", false);

    remote_char_t m, response; // Message to send to the server
    m.msg_type = 0;            // Join message

    send_message(requester, &m, sizeof(m));                  // Send join message to the server
    receive_message(requester, &response, sizeof(response)); // Receive response from the server

    if (strcmp(response.ticket, "FULL") == 0) // Check if the server is full
    {
        printf("Server is full\n");
        zmq_close(requester);     // Close the socket
        zmq_ctx_destroy(context); // Destroy the context
        exit(1);
    }

    m.ch = response.ch;                // Set the character
    strcpy(m.ticket, response.ticket); // Set the ticket
    // Initialize ncurses
    initscr();            /* Start curses mode 		*/
    cbreak();             /* Line buffering disabled	*/
    keypad(stdscr, TRUE); /* We get arrows etc...*/
    noecho();             /* Don't echo() while we do getch */
    curs_set(0);          // Hide the cursor

    int key;
    do
    {
        key = getch();            // Get the key pressed by the user
        processKeyBoard(key, &m); // Process the key pressed by the user

        // Don't send the message if the key pressed is not valid
        if (m.msg_type != -1)
        {
            send_message(requester, &m, sizeof(m));
        }
        char *buffer = s_recv(requester);           // Receive the score from the server
        mvprintw(0, 0, "Player Score: %s", buffer); // Print the score
        refresh();
        free(buffer);

        if (key == 'q') // Quit the game
        {
            // Disconnect from the server
            zmq_close(requester);     // Close the socket
            zmq_ctx_destroy(context); // Destroy the context
            endwin();                 /* End curses mode		  */
            exit(0);
        }
    } while (key != 27); // Exit the game when the user presses the ESC key
}
