#include <time.h>

typedef enum direction_t
{
    UP,
    DOWN,
    LEFT,
    RIGHT
} direction_t;

typedef struct remote_char_t
{
    int msg_type; // 0 - join, 1 - move, 2 - Firing, 3 - leave
    char ch;
    char ticket[7];
    direction_t direction;
    /* data */
} remote_char_t;

typedef struct ch_info_t
{
    int ch;
    int pos_x, pos_y;
    int score;
    bool move;
    bool shoot;
    char ticket[7];
    time_t hit_time;
    time_t shoot_time;
} ch_info_t;


