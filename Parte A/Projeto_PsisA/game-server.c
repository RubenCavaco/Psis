// Este programa é o servidor e que vai receber comunicações via ZeroMQ e manter o estado do jogo.

// Regras:
// 1. O jogo é jogado no maximo por 8 jogadores.
// 2. O jogo é jogado em um tabuleiro de 20x20.
// 3. Cada jogador tem um personagem que é representado por um caractere que é escolhido quando o jogador connecta-se ao server.
// 4. O personagem de cada jogador é colocado em uma posição no tabuleiro.
// 5. Os aliens movem-se aleatoriamente pelo tabuleiro e de 1 em 1 segundo.

#include <ncurses.h>
#include "remote-char.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zmq.h>
#include "zhelpers.h"
#include <unistd.h> // For usleep
#include "common.h"

// ALien space - line >=3  && line <= 18 && column >=3 && column <= 18
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

direction_t random_direction() // Para movimentar os aliens
{
    return random() % 4;
}

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

void send_to_subscribers(void *publisher, void *score_win, void *board_win)
{
    char *board_buffer = serialize_window(board_win);
    char *score_buffer = serialize_window(score_win);

    s_send(publisher, score_buffer);
    s_send(publisher, board_buffer);
    free(score_buffer);
    free(board_buffer);
}

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

// adicionar clientes
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

// remover clientes
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

bool are_coords_in_same_area(int line1, int column1, int line2, int column2)
{
    char area1 = get_player_area(line1, column1);
    char area2 = get_player_area(line2, column2);
    return area1 != '\0' && area1 == area2;
}

int ChoosePlayerArea(bool areas_occupied[])
{
    int area = (rand() % 8);

    while (areas_occupied[area] == true)
    {
        area = (rand() % 8);
    }

    return area;
}
// Funçao para escolher a posiçao do jogador - return x,y
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

void Shoot(WINDOW *board_win, WINDOW *score_win, void *publisher, int x, int y, char ch, int client_count, ch_info_t clients[])
{
    // Disparo na horizontal
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
                // adicionar pontos ao que disparou;
                clients[find_ch_info(clients, client_count, ch)].score += 1;
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
                // adicionar pontos ao que disparou
                clients[player].score += 1;
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
}

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

bool is_alien_move(WINDOW *board_win, int x, int y)
{
    if (IS_ALIEN_SPACE(x, y) && mvwinch(board_win, x, y) == ' ')
    {
        return true;
    }
    return false;
}

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
// Refazer esta funçao
void generate_ticket(char *ticket, size_t size)
{
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (size_t i = 0; i < size - 1; i++)
    {
        ticket[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    ticket[size - 1] = '\0';
}

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
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    curs_set(0);

    // Cria janelas para o tabuleiro e o placar
    WINDOW *numbers = newwin(BOARD_HEIGHT + 3, BOARD_WIDTH + 3, 0, 0); // +3 para incluir bordas e o numero das coordenadas
    WINDOW *board_win = derwin(numbers, BOARD_HEIGHT + 2, BOARD_WIDTH + 2, 1, 1);
    WINDOW *score_win = newwin(BOARD_HEIGHT + 2, SCORE_WIDTH, 1, BOARD_WIDTH + 4);

    // Inicializa tabuleiro e placar
    draw_board(numbers);
    box(board_win, 0, 0);
    draw_score(score_win, NULL, 0);
    wrefresh(board_win);

    pid_t pid = fork();

    if (pid < 0)
    {
        printf("Error creating the process\n");
        exit(1);
    }
    else if (pid == 0)
    {
        // movimentar aliens
        srand(time(NULL));
        void *context1 = NULL;
        void *requester1 = initialize_zmq_socket(&context1, ZMQ_REQ, "ipc:///tmp/s1", false);

        while (1)
        {
            remote_char_t buffer;
            buffer.msg_type = 1;
            buffer.ch = '*';

            send_message(requester1, &buffer, sizeof(buffer));
            receive_message(requester1, &buffer, sizeof(buffer));
            usleep(1000000);
        }
    }
    else
    {
        void *context = NULL;
        void *requester = initialize_zmq_socket(&context, ZMQ_REP, "ipc:///tmp/s1", true);
        void *publisher = initialize_zmq_socket(&context, ZMQ_PUB, "tcp://*:5555", true);

        // Inicializa ncurses
        srand(time(NULL));
        ch_info_t clients[MAX_CLIENTS];
        bool areas_occupied[8] = {false, false, false, false, false, false, false, false};
        int client_count = 0;
        int area = -1;

        // spawn alies
        spawn_aliens(board_win);

        while (1)
        {
            remote_char_t buffer;
            int pos_x, pos_y;

            receive_message(requester, &buffer, sizeof(buffer));
            time_t current_time = time(NULL);

            // Verificar se o jogador se pode mexer
            update_client_status(clients, client_count, current_time);

            // Verifica o tipo de mensagem Tipo 0 - join, 1 - move, 2 - Firing, 3 - leave
            if (buffer.msg_type == 0)
            {
                if (client_count == MAX_CLIENTS)
                {
                    strcpy(buffer.ticket, "FULL");
                    send_message(requester, &buffer, sizeof(buffer));
                }
                else
                {
                    area = ChoosePlayerArea(areas_occupied);
                    areas_occupied[area] = true;
                    ChoosePlayerPosition(area, &pos_x, &pos_y);

                    char ch_client = area + 'A';
                    generate_ticket(clients[client_count].ticket, sizeof(clients[client_count].ticket));
                    add_client(clients, &client_count, ch_client, pos_x, pos_y, clients[client_count].ticket);

                    // A contagem foi incrementada, então o índice do cliente é client_count - 1
                    strcpy(buffer.ticket, clients[client_count - 1].ticket);
                    buffer.ch = ch_client;

                    wmove(board_win, pos_x, pos_y);
                    waddch(board_win, ch_client | A_BOLD);
                    send_message(requester, &buffer, sizeof(buffer));
                }
            }
            else if (buffer.msg_type == 1)
            {
                // Verificar se o jogador pode se mover e é valido
                if (buffer.ch == '*')
                {
                    move_alien(board_win);
                }
                else if (validate_ticket(clients, client_count, buffer.ch, buffer.ticket))
                {
                    move_player(board_win, client_count, clients, buffer);
                }
            }
            else if (buffer.msg_type == 2 && validate_ticket(clients, client_count, buffer.ch, buffer.ticket))
            {
                int index = find_ch_info(clients, client_count, buffer.ch);
                int x = clients[index].pos_x;
                int y = clients[index].pos_y;
                // Registra o tempo do tiro

                if (clients[index].shoot == true)
                {
                    Shoot(board_win, score_win, publisher, x, y, buffer.ch, client_count, clients);
                    clients[index].shoot_time = time(NULL);
                    clients[index].shoot = false; // Impede que o jogador atire novamente até que o tiro anterior seja concluído
                }
            }
            else if (buffer.msg_type == 3 && validate_ticket(clients, client_count, buffer.ch, buffer.ticket))
            {
                int index = find_ch_info(clients, client_count, buffer.ch);
                wmove(board_win, clients[index].pos_x, clients[index].pos_y);
                waddch(board_win, ' ');
                areas_occupied[get_player_area(clients[index].pos_x, clients[index].pos_y) - 'A'] = false;
                remove_client(clients, &client_count, buffer.ch);
            }

            // send the score back to the client
            // s_send(requester, "OK");
            char score[10];
            int teste = find_ch_info(clients, client_count, buffer.ch);
            sprintf(score, "%d", clients[teste].score);
            s_send(requester, score);

            // Atualiando o placar e o tabuleiro
            draw_score(score_win, clients, client_count);
            wrefresh(board_win);

            // Envia o tabuleiro e o placar para os subscribers
            send_to_subscribers(publisher, score_win, board_win);
        }
        // Finaliza ncurses
        delwin(board_win);
        delwin(numbers);
        delwin(score_win);
        endwin();
    }

    return 0;
}