#include <stdlib.h>
#include <zmq.h>
#include "zhelpers.h"
#include <string.h>
#include <stdio.h>
#include <ncurses.h>
#include <unistd.h>
#include "remote-char.h"
#include "common.h"


void draw_board(WINDOW *board_win)
{
    wclear(board_win); // Limpa a janela antes de desenhar

    // Adiciona coordenadas no topo (na borda superior)
    for (int i = 1; i <= BOARD_WIDTH; i++)
    {
        mvwprintw(board_win, 0, i + 1, "%d", i % 10); // Posição Y=0 para ficar na borda superior
    }

    // Adiciona coordenadas na lateral (na borda lateral esquerda)
    for (int i = 1; i <= BOARD_HEIGHT; i++)
    {
        mvwprintw(board_win, i + 1, 0, "%d", i % 10); // Posição X=0 para ficar na borda lateral esquerda
    }

    wrefresh(board_win); // Atualiza a janela para exibir o conteúdo
}

void send_message(void *socket, void *buffer, size_t size) {
    if (zmq_send(socket, buffer, size, 0) == -1) {
        perror("Error sending the message");
        exit(1);
    }
}

void receive_message(void *socket, void *buffer, size_t size) {
    if (zmq_recv(socket, buffer, size, 0) == -1) {
        perror("Error receiving the message");
        exit(1);
    }
}

void* initialize_zmq_socket(void **context, int socket_type, const char *endpoint, bool is_bind) {
    if (*context == NULL) {
        *context = zmq_ctx_new();
    }
    void *socket = zmq_socket(*context, socket_type);
    int rc;
    if (is_bind) {
        rc = zmq_bind(socket, endpoint);
    } else {
        rc = zmq_connect(socket, endpoint);
    }
    if (rc != 0) {
        printf("Error %s to endpoint %s: %d\n", is_bind ? "binding" : "connecting", endpoint, rc);
        zmq_close(socket);
        zmq_ctx_destroy(*context);
        exit(1);
    }
    return socket;
}