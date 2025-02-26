#ifndef __COMMON_H_INCLUDED__
#define __COMMON_H_INCLUDED__

#define WINDOW_SIZE 22 // o tabuleiro é de 20x20, mas o tamanho da janela é 22x22 para desenhar a borda

#define BOARD_WIDTH 20
#define BOARD_HEIGHT 20
#define SCORE_WIDTH 15
#define MAX_CLIENTS 8
#define MAX_ALIENS 16 * 16 / 3

void draw_board(WINDOW *board_win);
void send_message(void *socket, void *buffer, size_t size);
void receive_message(void *socket, void *buffer, size_t size);
void* initialize_zmq_socket(void **context, int socket_type, const char *endpoint, bool is_bind);

#endif  // __COMMON_H_INCLUDED__