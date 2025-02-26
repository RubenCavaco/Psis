#include <time.h>

/**
 * Enum: direction_t
 * -----------------
 * Represents the possible directions for movement.
 */
typedef enum direction_t
{
    UP,
    DOWN,
    LEFT,
    RIGHT
} direction_t;

/**
 * Struct: remote_char_t
 * ---------------------
 * Represents a remote character and its actions.
 *
 * msg_type: The type of message (0 - join, 1 - move, 2 - firing, 3 - leave).
 * ch: The character representing the player.
 * ticket: The ticket string for the client.
 * direction: The direction of movement.
 */
typedef struct remote_char_t
{
    int msg_type; // 0 - join, 1 - move, 2 - Firing, 3 - leave
    char ch;
    char ticket[7];
    direction_t direction;
    /* data */
} remote_char_t;

/**
 * Struct: ch_info_t
 * -----------------
 * Contains information about a character (player) in the game.
 *
 * ch: The character representing the player.
 * pos_x, pos_y: The x and y coordinates of the player's position.
 * score: The player's score.
 * move: Boolean indicating if the player can move.
 * shoot: Boolean indicating if the player can shoot.
 * ticket: The ticket string for the client.
 * hit_time: The time when the player was last hit.
 * shoot_time: The time when the player last shot.
 */
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
