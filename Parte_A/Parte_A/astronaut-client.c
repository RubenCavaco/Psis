#include <ncurses.h>
#include <zmq.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "zhelpers.h"
#include "remote-char.h"
#include "common.h"

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
    curs_set(0);          // Hide the cursor

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
            // Disconnect from the server
            zmq_close(requester);
            zmq_ctx_destroy(context);
            endwin(); /* End curses mode		  */
            exit(0);
        }
    } while (key != 27);
}
