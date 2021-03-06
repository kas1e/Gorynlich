/*
 * This game is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This game is distributed in the hope that it will be fun,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this game; if not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * goblinhack@gmail.com
 */

#include <SDL.h>
#include <time.h>
#include <unistd.h>

#define MAZE_ROOM_NEXT_TO_OTHER_ROOMS_CHANCE        20
#define MAZE_HOW_LONG_TO_SPEND_TRYING_TO_SOLVE_MAZE 100000
#define MAZE_HOW_LIKELY_PERCENT_ARE_FORKS           50
#define MAZE_HOW_LIKELY_PERCENT_ARE_END_CORRIDORS   20
#define MAZE_MIN_DISTANCE_START_AND_END             200
#define MAX_MAP_NUMBER_OF_TIMES_TO_TRY_AND_PLACE_FRAG 50

#undef MAZE_DEBUG_PRINT_EXITS
#undef MAZE_DEBUG_SHOW_AS_GENERATING
#undef MAZE_DEBUG_SHOW_AS_GENERATING_FRAGMENTS
#undef MAZE_DEBUG_SHOW_CONSOLE

#include "main.h"
#include "wid.h"
#include "map_jigsaw.h"
#include "ramdisk.h"
#include "thing_shop.h"
#include "bits.h"
#include "color.h"

/*
 * Creates a map somewhat like this
 *   
 *                                                                     
 *                 xxxxxxxxxxxxx             xxxxxxxxxxxxx             
 *                 xH..m......Hx             x...........x             
 *      ++++++++   xHxxxxxxxxxHx             x...........x   ++++++++  
 *      H,,,,,,H   xHxxxxxx.$xHx             xxxxx.......x   H,,,,,,H  
 *      H++++++H   xH....HxxxxHx             x...x.......x   H++++++H  
 *      H++++++H+++xxxxxxHx...Hx++++++++++++ x...xxxxxxxxx+++H++++++H  
 *      H,,,,,,H,,,......HxH.SH.,E,,,,H,,,,+ x............,,,H,,,,,,H  
 *      H++++++,+++xxxxxxxxHxxxx++++++H+++++ x...xxxxxxxxx+++,++++++H  
 *    ++H++++++,+  xxxxxxx.H...x      H      x....xxxxxxxx  +,++++++H++
 *    +,H,,,H,+++  xxxxx...H...x      H      xx....xxxxxxx  +++,H,,,H,+
 *    ++,+++H++    x.......H...x      H      x...........x    ++H+++,++
 *     +,+  H      x..m....H.m.x      H      x.....m....$x      H  +,+ 
 *     +++  H      xxxxxx.xxxxxx      H      xxxxxxxxxxxxx      H  +++ 
 *          H      xxxxxx.xxxxxx      H      x..xxxxxxxxxxxxxxxx.xxxxxx
 *     xxxxxHxxxxxxxxxxxx......x      H      x..m..H.....xx......xxxxxx
 *     xH...H.....xxxxxxxxx....x      H      xxxxxxHxxTTTxx....xxxxxxxx
 *     xH.xx.xxxx.xxxxxxx....xxx      H      xxxxx.H.xm.$xxxx....xxxxxx
 *     xH.........xxxxxxxx.....x xxxxxHxxx   xxx...H.xxxx.......xxxxxxx
 *     xH.xxxx.xx.xxx....xxx...x+xH..xH..x+++x.....H..........xxx....xx
 *     xH.........xx..m.........,,H...H..,,,,........m.............m..x
 *     xx..xHx...xxx.xxxxxxTTTTx+xHxxx...x+++xTTTTx.xxxxxxxTTTTxxxxxx.x
 *      x.xxHxx.xx x.xxxxxx...xx xH..x..xx   x....x.x....xxx...xxxxxx.x
 *      x.x H x.x  x.xxx$......x xHxxx...x   x....x.x....xx......$xxx.x
 *      xxx H xxx  x...xxxx....x xH...H..x   x....x.x....xx....xxxx...x
 *          H      x..m...xLLLLx xxxxxHxxx   xLLLLx.xWWWWxxLLLLx...m..x
 *          H      xxxxxx.xxxxxx      H      xxxxxx.xxxxxxxxxxxx.xxxxxx
 *          H            H       +++  H            H      ++++++H++++++
 *        ++H+++++  +++++H++     +,+  H            H      +,,,H,H,,,,,+
 *        +,H,,,,+  +,,,,H,+    ++,+++H++       +++H+++   +++,H+,++++++
 *       +++H+++,+  +,+++H+++   +,H,,,H,+++     H,,H,,H    H,,H+,++++  
 *       +,,H,,,H+  +H,,,H,,+   ++H++++++,+     H+++++H    H++++,,,,+  
 *     +++++,+++H++++H+++,+++++   H++++++,++++++H+++++H++++H +++,+++++ 
 *     +,,,,,,,,H,,,,H,,,,,,,,+   H,,,,,,H,,,,,,H,,+,,H,,,,H++,,,,,,,+ 
 *     +++,++++++++++++++++,+++   H++++++H+++++++++++++++++,++++++++++ 
 *       +++              +++     H++++++H                +,+          
 *                                H,,,,,,H                +++          
 *                                ++++++++                             
 *                                                                     
 */

#define tcup(x,y)           printf("\033[%d;%dH", y + 1, x + 1);

static int maze_seed;

/*
 * This is the buffer we print the jigsaw too.
 */
char map_jigsaw_buffer[MAP_WIDTH][MAP_HEIGHT];
static char map_jigsaw_buffer_old[MAP_WIDTH][MAP_HEIGHT];
static uint8_t map_jigsaw_buffer_fg[MAP_WIDTH][MAP_HEIGHT];
static uint8_t map_jigsaw_buffer_bg[MAP_WIDTH][MAP_HEIGHT];
static uint8_t map_jigsaw_buffer_action1[MAP_WIDTH][MAP_HEIGHT];
static uint8_t map_jigsaw_buffer_action2[MAP_WIDTH][MAP_HEIGHT];
static uint8_t map_jigsaw_buffer_action3[MAP_WIDTH][MAP_HEIGHT];
static const char *map_jigsaw_buffer_color[MAP_WIDTH][MAP_HEIGHT];
static uint8_t map_jigsaw_buffer_solved[MAP_WIDTH][MAP_HEIGHT];
static int32_t map_jigsaw_buffer_at_x;
static int32_t map_jigsaw_buffer_at_y;

/*
 * Global modes.
 */
static uint8_t jigpiece_horiz_flip;
static uint8_t jigpiece_vert_flip;
static uint8_t jigpiece_rotatable;

enum {
    TERM_COLOR_BLACK,
    TERM_COLOR_RED,
    TERM_COLOR_GREEN,
    TERM_COLOR_YELLOW,
    TERM_COLOR_BLUE,
    TERM_COLOR_PINK,
    TERM_COLOR_CYAN,
    TERM_COLOR_WHITE,
    TERM_COLOR_RESET,
    TERM_COLOR_MAX,
};

static uint8_t map_fg[MAP_MAX];
static uint8_t map_bg[MAP_MAX];
static uint8_t valid_frag_char[MAP_MAX];
static uint8_t valid_frag_alt_char[MAP_MAX];

enum {
    /*
     * Order is important so 3 - x == opposite dir
     */
    DIR_NORTH,
    DIR_EAST,
    DIR_WEST,
    DIR_SOUTH,
    DIR_MAX,
};

/*
 * A single jigpiece used to build the level.
 *
 * +---------+
 * |xxxxxxxxx|
 * |x.......x|
 * |x.......x|
 * |x.......x|
 * |D.......x|
 * |x.......x|
 * |x.......x|
 * |x.......x|
 * |xxxxxxxxx|
 * +---------+
 */
typedef struct {
    char c[JIGPIECE_WIDTH][JIGPIECE_HEIGHT];

    /*
     * A bitmap of exit directions.
     */
    int32_t exits[DIR_MAX];

    /*
     * What map types are in this piece?
     */
    int32_t has[MAP_MAX];

    uint8_t rotatable:1;
    uint8_t horiz_flip:1;
    uint8_t vert_flip:1;
    uint8_t empty:1;
} jigpiece_t;

/*
 * A solvable maze, e.g.:
 * +---+---+---+---+---+---+---+---+---+---+---+---+
 * |   |                   |       |       |       |
 * +   +   +---+---+---+   +   +   +   +   +   +   +
 * | E |               |   |   |       |   |   |   |
 * +   +   +---+---+   +   +   +---+---+   +   +   +
 * | * |           |   |   |       |   |   |   |   |
 * +   +---+   +---+   +---+---+   +   +   +   +   +
 * | *   * |   |       |       |       |       |   |
 * +---+   +   +   +   +   +   +---+   +---+---+   +
 * |   | * |   |   |   |   |           |           |
 * +   +   +   +   +---+   +---+---+---+   +---+---+
 * | *   * |   |           |           |           |
 * +   +---+   +---+---+---+   +---+   +   +---+   +
 * | *   S |               |   |   |   |   |       |
 * +---+   +---+---+   +---+   +   +   +   +   +---+
 * |   |   |       |   |       |       |   |       |
 * +   +   +   +   +---+   +---+   +---+---+---+   +
 * |   |       |       |   |   |   |               |
 * +   +---+---+---+   +   +   +   +---+   +---+   +
 * |                       |               |       |
 * +---+---+---+---+---+---+---+---+---+---+---+---+
 */
typedef struct maze_cell_t_ {
    struct maze_cell_t_ *exit[4];
    int32_t possible_jigpieces[JIGPIECE_MAX];
    int32_t possible_jigpieces_size;
    int32_t jigpiece_index;
    jigpiece_t *jp;
    int32_t x;
    int32_t y;
    uint8_t exits:4;
    uint8_t walked:1;
    uint8_t dead:1;
    uint8_t start:1;
    uint8_t end:1;
    uint8_t solved:1;
    uint8_t best:1;
} maze_cell_t;

#define MAZE_CELL(c, x, y)          (c + (MAP_JIGSAW_PIECES_ACROSS * (y)) + (x))
#define MAZE_CAN_GO(c, dir)         (c->exit[dir] && !(c->exit[dir]->walked))
#define MAZE_HAS_EXIT(c, x, y, dir) (MAZE_CELL(c, (x), (y))->exits & (1<<(dir)))

#define MAZE_FRAG_DIRECTIONS 8

typedef struct {
    int32_t jigpieces_cnt;
    int32_t jigpieces_cap;
    jigpiece_t *jigpiece;

    /*
     * Fragments
     */
    int32_t frag_cnt;
    int32_t frag_cap;
    jigpiece_t* frag[MAZE_FRAG_DIRECTIONS];
    int32_t frag_to_alt_base[JIGPIECE_MAX];
    int32_t frag_cnt_alts[JIGPIECE_MAX];
    int32_t frag_alt_cnt;
    int32_t frag_alt_cap;
    jigpiece_t* frag_alt[MAZE_FRAG_DIRECTIONS];

    /*
     * Triggers
     */
    int32_t trigger_fragment_cnt;
    int32_t trigger_fragment_cap;
    jigpiece_t* trigger_fragment[MAZE_FRAG_DIRECTIONS];
    int32_t trigger_fragment_to_alt_base[JIGPIECE_MAX];
    int32_t trigger_cnt_alts[JIGPIECE_MAX];
    int32_t action1_cnt_alts[JIGPIECE_MAX];
    int32_t action2_cnt_alts[JIGPIECE_MAX];
    int32_t action3_cnt_alts[JIGPIECE_MAX];
    int32_t trigger_cnt;
    int32_t action1_cnt;
    int32_t action2_cnt;
    int32_t action3_cnt;
    int32_t trigger_cap;
    int32_t action1_cap;
    int32_t action2_cap;
    int32_t action3_cap;
    jigpiece_t* trigger[MAZE_FRAG_DIRECTIONS];
    jigpiece_t* action1[MAZE_FRAG_DIRECTIONS];
    jigpiece_t* action2[MAZE_FRAG_DIRECTIONS];
    jigpiece_t* action3[MAZE_FRAG_DIRECTIONS];

    maze_cell_t maze[MAP_JIGSAW_PIECES_ACROSS * MAP_JIGSAW_PIECES_DOWN];

    /*
     * Co-ords here are in terms of rooms, not characters.
     */
    uint32_t sx;
    uint32_t sy;
    uint32_t ex;
    uint32_t ey;
} dungeon_t;

static dungeon_t* new_dungeon()
{
    dungeon_t *dg = (typeof(dg)) myzalloc(sizeof(*dg), __FUNCTION__);

    return dg;
}
static void delete_dungeon(dungeon_t *dg)
{
    if(!dg) return;
    myfree(dg->jigpiece);
    for (int i=0; i<MAZE_FRAG_DIRECTIONS; ++i) {
        myfree(dg->frag[i]);
        myfree(dg->frag_alt[i]);
        myfree(dg->trigger_fragment[i]);
        myfree(dg->trigger[i]);
        myfree(dg->action1[i]);
        myfree(dg->action2[i]);
        myfree(dg->action3[i]);
    }
    myfree(dg);
}
static void dungeon_resize_jigpiece(dungeon_t *dg, int32_t n)
{
    if(dg->jigpieces_cap>n)
        return;
    int old = dg->jigpieces_cap;
    dg->jigpieces_cap += 10;
    dg->jigpiece = (jigpiece_t*) myrealloc(dg->jigpiece, dg->jigpieces_cap*sizeof(jigpiece_t), __FUNCTION__);
    for (int i=old; i<dg->jigpieces_cap; ++i)
        memset(dg->jigpiece+i, 0, sizeof(jigpiece_t));
}
static void dungeon_resize_frag(dungeon_t *dg, int32_t n)
{
    if(dg->frag_cap>n)
        return;
    int old = dg->frag_cap;
    dg->frag_cap += 10;
    for(int i=0; i<MAZE_FRAG_DIRECTIONS; ++i) {
        dg->frag[i] = (jigpiece_t*) myrealloc(dg->frag[i], dg->frag_cap*sizeof(jigpiece_t), __FUNCTION__);
        for (int j=old; j<dg->frag_cap; ++j)
            memset(&dg->frag[i][j], 0, sizeof(jigpiece_t));
    }
}
static void dungeon_resize_frag_alt(dungeon_t *dg, int32_t n)
{
    if(dg->frag_alt_cap>n)
        return;
    int old = dg->frag_alt_cap;
    dg->frag_alt_cap += 10;
    for(int i=0; i<MAZE_FRAG_DIRECTIONS; ++i) {
        dg->frag_alt[i] = (jigpiece_t*) myrealloc(dg->frag_alt[i], dg->frag_alt_cap*sizeof(jigpiece_t), __FUNCTION__);
        for (int j=old; j<dg->frag_alt_cap; ++j)
            memset(&dg->frag_alt[i][j], 0, sizeof(jigpiece_t));
    }
}
static void dungeon_resize_trigger_fragment(dungeon_t *dg, int32_t n)
{
    if(dg->trigger_fragment_cap>n)
        return;
    int old = dg->trigger_fragment_cap;
    dg->trigger_fragment_cap += 10;
    for(int i=0; i<MAZE_FRAG_DIRECTIONS; ++i) {
        dg->trigger_fragment[i] = (jigpiece_t*) myrealloc(dg->trigger_fragment[i], dg->trigger_fragment_cap*sizeof(jigpiece_t), __FUNCTION__);
        for (int j=old; j<dg->trigger_fragment_cap; ++j)
            memset(&dg->trigger_fragment[i][j], 0, sizeof(jigpiece_t));
    }
}
static void dungeon_resize_trigger(dungeon_t *dg, int32_t n)
{
    if(dg->trigger_cap>n)
        return;
    int old = dg->trigger_cap;
    dg->trigger_cap += 10;
    for(int i=0; i<MAZE_FRAG_DIRECTIONS; ++i) {
        dg->trigger[i] = (jigpiece_t*) myrealloc(dg->trigger[i], dg->trigger_cap*sizeof(jigpiece_t), __FUNCTION__);
        for (int j=old; j<dg->trigger_cap; ++j)
            memset(&dg->trigger[i][j], 0, sizeof(jigpiece_t));
    }
}
static void dungeon_resize_action1(dungeon_t *dg, int32_t n)
{
    if(dg->action1_cap>n)
        return;
    int old = dg->action1_cap;
    dg->action1_cap += 10;
    for(int i=0; i<MAZE_FRAG_DIRECTIONS; ++i) {
        dg->action1[i] = (jigpiece_t*) myrealloc(dg->action1[i], dg->action1_cap*sizeof(jigpiece_t), __FUNCTION__);
        for (int j=old; j<dg->action1_cap; ++j)
            memset(&dg->action1[i][j], 0, sizeof(jigpiece_t));
    }
}
static void dungeon_resize_action2(dungeon_t *dg, int32_t n)
{
    if(dg->action2_cap>n)
        return;
    int old = dg->action2_cap;
    dg->action2_cap += 10;
    for(int i=0; i<MAZE_FRAG_DIRECTIONS; ++i) {
        dg->action2[i] = (jigpiece_t*) myrealloc(dg->action2[i], dg->action2_cap*sizeof(jigpiece_t), __FUNCTION__);
        for (int j=old; j<dg->action2_cap; ++j)
            memset(&dg->action2[i][j], 0, sizeof(jigpiece_t));
    }
}
static void dungeon_resize_action3(dungeon_t *dg, int32_t n)
{
    if(dg->action3_cap>n)
        return;
    int old = dg->action3_cap;
    dg->action3_cap += 10;
    for(int i=0; i<MAZE_FRAG_DIRECTIONS; ++i) {
        dg->action3[i] = (jigpiece_t*) myrealloc(dg->action3[i], dg->action3_cap*sizeof(jigpiece_t), __FUNCTION__);
        for (int j=old; j<dg->action3_cap; ++j)
            memset(&dg->action3[i][j], 0, sizeof(jigpiece_t));
    }
}
/*
 * Globals:
 */
int32_t opt_seed;

#ifdef MAZE_DEBUG_SHOW_AS_GENERATING
static void maze_debug(dungeon_t *dg);
#endif

static char *dieat (int32_t line, int32_t col, const char *why)
{
    DIE("Died at line %u, col %i: %s", line, col, why);
}

/*
 * filetobuf
 *
 * Read a whole file into an allocated buffer.
 */
static char *filetobuf (const char *file)
{
    int32_t len;
    char *buf;

    buf = (typeof(buf)) ramdisk_load(file, &len);

    return (buf);
}

/*
 * map_jigsaw_buffer_goto
 */
static void map_jigsaw_buffer_goto (int32_t x, int32_t y)
{
    map_jigsaw_buffer_at_x = x;
    map_jigsaw_buffer_at_y = y;
}

/*
 * map_jigsaw_buffer_putchar
 */
static void map_jigsaw_buffer_putchar (int32_t m)
{
    if (m == 's') {
        ERR("s");
    }
    if (map_jigsaw_buffer_at_x < 0) {
        return;
    }

    if (map_jigsaw_buffer_at_y < 0) {
        return;
    }

    if (map_jigsaw_buffer_at_x >= MAP_WIDTH) {
        return;
    }

    if (map_jigsaw_buffer_at_y >= MAP_HEIGHT) {
        return;
    }

    map_jigsaw_buffer_fg[map_jigsaw_buffer_at_x][map_jigsaw_buffer_at_y] = map_fg[m];
    map_jigsaw_buffer_bg[map_jigsaw_buffer_at_x][map_jigsaw_buffer_at_y] = map_bg[m];
    map_jigsaw_buffer[map_jigsaw_buffer_at_x++][map_jigsaw_buffer_at_y] = m;
}

/*
 * map_jigsaw_buffer_getchar
 */
uint8_t map_jigsaw_buffer_getchar (int32_t x, int32_t y)
{
    if (x < 0) {
        return (MAP_EMPTY);
    }

    if (y < 0) {
        return (MAP_EMPTY);
    }

    if (x >= MAP_WIDTH) {
        return (MAP_EMPTY);
    }

    if (y >= MAP_HEIGHT) {
        return (MAP_EMPTY);
    }

    return (map_jigsaw_buffer[x][y]);
}

static uint8_t map_jigsaw_buffer_old_getchar (int32_t x, int32_t y)
{
    if (x < 0) {
        return (MAP_EMPTY);
    }

    if (y < 0) {
        return (MAP_EMPTY);
    }

    if (x >= MAP_WIDTH) {
        return (MAP_EMPTY);
    }

    if (y >= MAP_HEIGHT) {
        return (MAP_EMPTY);
    }

    return (map_jigsaw_buffer_old[x][y]);
}

/*
 * map_jigsaw_buffer_set_fgbg
 */
static void map_jigsaw_buffer_set_fgbg (uint8_t fg, uint8_t bg)
{
    static const char *data[] = {
            "[40;30m", "[40;31m", "[40;32m", "[40;33m",
            "[40;34m", "[40;35m", "[40;36m", "[40;37m",
            "[41;30m", "[41;31m", "[41;32m", "[41;33m",
            "[41;34m", "[41;35m", "[41;36m", "[41;37m",
            "[42;30m", "[42;31m", "[42;32m", "[42;33m",
            "[42;34m", "[42;35m", "[42;36m", "[42;37m",
            "[43;30m", "[43;31m", "[43;32m", "[43;33m",
            "[43;34m", "[43;35m", "[43;36m", "[43;37m",
            "[44;30m", "[44;31m", "[44;32m", "[44;33m",
            "[44;34m", "[44;35m", "[44;36m", "[44;37m",
            "[45;30m", "[45;31m", "[45;32m", "[45;33m",
            "[45;34m", "[45;35m", "[45;36m", "[45;37m",
            "[46;30m", "[46;31m", "[46;32m", "[46;33m",
            "[46;34m", "[46;35m", "[46;36m", "[46;37m",
            "[47;30m", "[47;31m", "[47;32m", "[47;33m",
            "[47;34m", "[47;35m", "[47;36m", "[47;37m",
    };

    printf("%s", data[(bg & 7) * 8 + (fg & 7)]);
}

/*
 * map_jigsaw_buffer_print
 */
void map_jigsaw_buffer_print(void);
void map_jigsaw_buffer_print (void)
{
    int32_t need_nl;
    int32_t x;
    int32_t y;
    int32_t fg;
    int32_t bg;

    need_nl = 0;

    tcup(0,0);

    for (y = 0; y < MAP_HEIGHT; y++) {
        for (x = 0; x < MAP_WIDTH; x++) {
            char c;

            c = map_jigsaw_buffer[x][y];
            fg = map_jigsaw_buffer_fg[x][y];
            bg = map_jigsaw_buffer_bg[x][y];

            if (!c) {
                fg = TERM_COLOR_WHITE;
                bg = TERM_COLOR_BLACK;
                c = ' ';
            }

            if ((c == MAP_FLOOR) | (c == MAP_CORRIDOR)) {
                if (map_jigsaw_buffer_solved[x][y]) {
                    c = '*';
                }
            }

            if (x == 0) {
                if (need_nl) {
                    putchar('\n');
                }
                need_nl = 1;
            }

            map_jigsaw_buffer_set_fgbg(fg, bg);
            putchar(c);
        }
    }
}

/*
 * map_jigsaw_buffer_print_file
 */
static void map_jigsaw_buffer_print_file (FILE *fpin)
{
    char tmp[MAXSTR];
    int32_t need_nl;
    int32_t x;
    int32_t y;
    FILE *fp;

    snprintf(tmp, sizeof(tmp) - 1, "maps.small.%u", maze_seed);

    if (!fpin) {
        fp = fopen(tmp, "w");
        if (!fp) {
            MSG_BOX("can't write map file %s", tmp);
            return;
        }
    } else {
        fp = fpin;
    }

    need_nl = 0;

    for (y = 0; y < MAP_HEIGHT; y++) {
        for (x = 0; x < MAP_WIDTH; x++) {
            char c;

            c = map_jigsaw_buffer[x][y];

            if (!c) {
                c = ' ';
            }

            if (x == 0) {
                if (need_nl) {
                    fputc('\n', fp);
                }
                need_nl = 1;
            }

            fputc(c, fp);
        }
    }

    fputc('\n', fp);

    if (!fpin) {
        fclose(fp);
    }
}

/*
 * Read jigpiece data from a buffer.
 */
static void jigpieces_read (dungeon_t *dg, char *buf)
{
    char *c = buf;
    int32_t cnt_cells_per_line;
    int32_t x;
    int32_t n;
    int32_t y;
    int32_t col;
    int32_t line;
    char command[80];
    char *s;
    int32_t reading_jigpieces;
    int32_t reading_frag;
    int32_t reading_frag_alt;
    int32_t reading_trigger_fragment;
    int32_t reading_trigger;
    int32_t reading_action1;
    int32_t reading_action2;
    int32_t reading_action3;

    reading_jigpieces = 0;
    reading_frag = 0;
    reading_frag_alt = 0;
    reading_trigger_fragment = 0;
    reading_trigger = 0;
    reading_action1 = 0;
    reading_action2 = 0;
    reading_action3 = 0;

    col = 1;
    line = 1;

    while (*c) {
        /*
         * Skip comments
         */
        if (*c == '#') {
            while (*c != '\n') {
                c++;
            }
            c++;
            col = 0;
            line++;
            continue;
        }

        /*
         * : begins commands.
         */
        if (*c == ':') {
            c++;
            s = command;

            memset(command, 0, sizeof(command));

            while ((*c != '\n') && (s < command + sizeof(command))) {
                *s++ = *c++;
            }

            if (!strcasecmp(command, "horiz-flip")) {
                jigpiece_horiz_flip =  true;
            } else if (!strcasecmp(command, "vert-flip")) {
                jigpiece_vert_flip = true;
            } else if (!strcasecmp(command, "rotatable")) {
                jigpiece_rotatable =  true;
            } else if (!strcasecmp(command, "reset")) {
                jigpiece_horiz_flip = false;
                jigpiece_vert_flip = false;
                jigpiece_rotatable = false;
            } else {
                reading_jigpieces = 0;
                reading_frag = 0;
                reading_frag_alt = 0;
                reading_trigger_fragment = 0;
                reading_trigger = 0;
                reading_action1 = 0;
                reading_action2 = 0;
                reading_action3 = 0;

                if (!strcasecmp(command, "jigpieces")) {
                    reading_jigpieces = 1;
                } else if (!strcasecmp(command, "fragment")) {
                    reading_frag = 1;
                } else if (!strcasecmp(command, "alternative")) {
                    reading_frag_alt = 1;
                } else if (!strcasecmp(command, "trigger-fragment")) {
                    reading_trigger_fragment = 1;
                } else if (!strcasecmp(command, "trigger")) {
                    reading_trigger = 1;
                } else if (!strcasecmp(command, "action1")) {
                    reading_action1 = 1;
                } else if (!strcasecmp(command, "action2")) {
                    reading_action2 = 1;
                } else if (!strcasecmp(command, "action3")) {
                    reading_action3 = 1;
                } else {
                    dieat(line, col, "unknown command");
                }
            }

            c++;
            col = 0;
            line++;
            continue;
        }

        /*
         * Read jigpiece header
         *
         * +--------+--------+...
         */
        if (*c != '+') {
            dieat(line, col, "expecting start of line");
        }

        if (!reading_jigpieces &&
            !reading_frag &&
            !reading_trigger_fragment &&
            !reading_trigger &&
            !reading_action1 &&
            !reading_action2 &&
            !reading_action3 &&
            !reading_frag_alt) {
            dieat(line, col,
                  "data found when no reading jigpieces or frag");
        }

        c++;
        col++;

        /*
         * Count the number of jigpiece across per line.
         */
        cnt_cells_per_line = 0;

        while (*c != '\n') {
            if (*c == '+') {
                cnt_cells_per_line++;
            }
            c++;
            col++;
        }

        if (*c != '\n') {
            dieat(line, col, "expecing end of line");
        }

        /*
         * Move onto the next line.
         */
        c++;
        col = 0;
        line++;

        if (*c == EOF) {
            return;
        }

        if (dg->jigpieces_cnt + cnt_cells_per_line >= JIGPIECE_MAX) {
            dieat(line, col, "too many jigpieces");
        }

        if (!*c) {
            return;
        }

        /*
         * Read one line of data
         * |........|........|...
         */
        col = 0;
        y = 0;

        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
            for (n = 0; n < cnt_cells_per_line; n++) {

                if (*c != '|') {
                    ERR("Found char 0x%x '%c' when expecting | char", *c, *c);
                    dieat(line, col, "expecting start of jigpiece "
                          "data, found char");
                }

                c++;
                col++;

                for (x = 0; x < JIGPIECE_WIDTH; x++) {

                    if (*c == '\n') {
                        dieat(line, col, "premature end of jigpiece data");
                    }

                    if (reading_jigpieces) {
                        dungeon_resize_jigpiece(dg, dg->jigpieces_cnt + n);
                        dg->jigpiece[dg->jigpieces_cnt + n].c[x][y] = *c;
                        dg->jigpiece[dg->jigpieces_cnt + n].rotatable = 
                                        jigpiece_rotatable;
                        dg->jigpiece[dg->jigpieces_cnt + n].horiz_flip = 
                                        jigpiece_horiz_flip;
                        dg->jigpiece[dg->jigpieces_cnt + n].vert_flip = 
                                        jigpiece_vert_flip;

                    } else if (reading_frag) {
                        dungeon_resize_frag(dg, dg->frag_cnt + n);
                        dg->frag[0][dg->frag_cnt + n].c[x][y] = *c;
                        dg->frag[0][dg->frag_cnt + n].rotatable = 
                                        jigpiece_rotatable;
                        dg->frag[0][dg->frag_cnt + n].horiz_flip = 
                                        jigpiece_horiz_flip;
                        dg->frag[0][dg->frag_cnt + n].vert_flip = 
                                        jigpiece_vert_flip;

                    } else if (reading_frag_alt) {
                        dungeon_resize_frag_alt(dg, dg->frag_alt_cnt + n);
                        dg->frag_alt[0][dg->frag_alt_cnt + n].c[x][y] = *c;
                        dg->frag_alt[0][dg->frag_alt_cnt + n].rotatable = 
                                        jigpiece_rotatable;
                        dg->frag_alt[0][dg->frag_alt_cnt + n].horiz_flip = 
                                        jigpiece_horiz_flip;
                        dg->frag_alt[0][dg->frag_alt_cnt + n].vert_flip = 
                                        jigpiece_vert_flip;

                    } else if (reading_trigger_fragment) {
                        dungeon_resize_trigger_fragment(dg, dg->trigger_fragment_cnt + n);
                        dg->trigger_fragment[0][dg->trigger_fragment_cnt + n].c[x][y] = *c;
                        dg->trigger_fragment[0][dg->trigger_fragment_cnt + n].rotatable = 
                                        jigpiece_rotatable;
                        dg->trigger_fragment[0][dg->trigger_fragment_cnt + n].horiz_flip = 
                                        jigpiece_horiz_flip;
                        dg->trigger_fragment[0][dg->trigger_fragment_cnt + n].vert_flip = 
                                        jigpiece_vert_flip;

                    } else if (reading_trigger) {
                        dungeon_resize_trigger(dg, dg->trigger_cnt + n);
                        dg->trigger[0][dg->trigger_cnt + n].c[x][y] = *c;
                        dg->trigger[0][dg->trigger_cnt + n].rotatable = 
                                        jigpiece_rotatable;
                        dg->trigger[0][dg->trigger_cnt + n].horiz_flip = 
                                        jigpiece_horiz_flip;
                        dg->trigger[0][dg->trigger_cnt + n].vert_flip = 
                                        jigpiece_vert_flip;

                    } else if (reading_action1) {
                        dungeon_resize_action1(dg, dg->action1_cnt + n);
                        dg->action1[0][dg->action1_cnt + n].c[x][y] = *c;
                        dg->action1[0][dg->action1_cnt + n].rotatable = 
                                        jigpiece_rotatable;
                        dg->action1[0][dg->action1_cnt + n].horiz_flip = 
                                        jigpiece_horiz_flip;
                        dg->action1[0][dg->action1_cnt + n].vert_flip = 
                                        jigpiece_vert_flip;

                    } else if (reading_action2) {
                        dungeon_resize_action2(dg, dg->action2_cnt + n);
                        dg->action2[0][dg->action2_cnt + n].c[x][y] = *c;
                        dg->action2[0][dg->action2_cnt + n].rotatable = 
                                        jigpiece_rotatable;
                        dg->action2[0][dg->action2_cnt + n].horiz_flip = 
                                        jigpiece_horiz_flip;
                        dg->action2[0][dg->action2_cnt + n].vert_flip = 
                                        jigpiece_vert_flip;

                    } else if (reading_action3) {
                        dungeon_resize_action3(dg, dg->action3_cnt + n);
                        dg->action3[0][dg->action3_cnt + n].c[x][y] = *c;
                        dg->action3[0][dg->action3_cnt + n].rotatable = 
                                        jigpiece_rotatable;
                        dg->action3[0][dg->action3_cnt + n].horiz_flip = 
                                        jigpiece_horiz_flip;
                        dg->action3[0][dg->action3_cnt + n].vert_flip = 
                                        jigpiece_vert_flip;

                    } else {
                        ERR("bug");
                    }

                    c++;
                    col++;
                }
            }

            /*
             * Should be at the end of the line
             */
            if (*c != '|') {
                dieat(line, col, "premature end of file");
            }

            c++;
            col++;

            if (*c != '\n') {
                dieat(line, col, "expecting end of line");
            }

            /*
             * Move onto the next line.
             */
            c++;
            col = 0;
            line++;
        }

        if (reading_jigpieces) {
            dg->jigpieces_cnt += cnt_cells_per_line;
        } else if (reading_frag) {

            if (!dg->frag_to_alt_base[dg->frag_cnt]) {
                dg->frag_to_alt_base[dg->frag_cnt] = dg->frag_alt_cnt;
            }

            dg->frag_cnt += cnt_cells_per_line;
        } else if (reading_frag_alt) {
            dg->frag_alt_cnt += cnt_cells_per_line;

            dg->frag_cnt_alts[dg->frag_cnt-1] += cnt_cells_per_line;
        } else if (reading_trigger_fragment) {

            if (!dg->trigger_fragment_to_alt_base[dg->trigger_fragment_cnt]) {
                dg->trigger_fragment_to_alt_base[dg->trigger_fragment_cnt] = dg->trigger_cnt;
            }

            dg->trigger_fragment_cnt += cnt_cells_per_line;
        } else if (reading_trigger) {
            dg->trigger_cnt += cnt_cells_per_line;

            dg->trigger_cnt_alts[dg->trigger_fragment_cnt-1] += cnt_cells_per_line;
        } else if (reading_action1) {
            dg->action1_cnt += cnt_cells_per_line;

            dg->action1_cnt_alts[dg->trigger_fragment_cnt-1] += cnt_cells_per_line;
        } else if (reading_action2) {
            dg->action2_cnt += cnt_cells_per_line;

            dg->action2_cnt_alts[dg->trigger_fragment_cnt-1] += cnt_cells_per_line;
        } else if (reading_action3) {
            dg->action3_cnt += cnt_cells_per_line;

            dg->action3_cnt_alts[dg->trigger_fragment_cnt-1] += cnt_cells_per_line;
        }
    }
}

/*
 * jigpiece_char_is_occupiable
 */
static int32_t jigpiece_char_is_occupiable (char c)
{
    return (c == MAP_FLOOR) ||
           (c == MAP_CORRIDOR) ||
           (c == MAP_MONST) ||
           (c == MAP_MONST_OR_MOB) ||
           (c == MAP_GENERATOR) ||
           (c == MAP_TRIGGER_HERO) ||
           (c == MAP_TRIGGER_MONST) ||
           (c == MAP_TREASURE);
}

static int32_t jigpiece_char_is_passable (char c)
{
    return (c == MAP_FLOOR) ||
           (c == MAP_CORRIDOR) ||
           (c == MAP_MONST) ||
           (c == MAP_START) ||
           (c == MAP_END) ||
           (c == MAP_MONST_OR_MOB) ||
           (c == MAP_GENERATOR) ||
           (c == MAP_DOOR) ||
           (c == MAP_WEAPON) ||
           (c == MAP_BRAZIER) ||
           (c == MAP_POTION) ||
           (c == MAP_TRIGGER_HERO) ||
           (c == MAP_TRIGGER_MONST) ||
           (c == MAP_TREASURE);
}

/*
 * jigpiece_char_is_monst
 */
static int32_t jigpiece_char_is_monst (char c)
{
    return ((c == MAP_MONST) ||
            (c == MAP_GENERATOR) ||
            (c == MAP_MONST_OR_MOB));
}

/*
 * jigpiece_char_is_dungeon_floor_or_corridor
 */
static int32_t jigpiece_char_is_dungeon_floor_or_corridor (char c)
{
    return (c == MAP_FLOOR) || (c == MAP_CORRIDOR);
}

/*
 * jigpiece_char_at
 */
static char jigpiece_char_at (dungeon_t *dg, int32_t which, int32_t x, int32_t y)
{
    if (x < 0) {
        return (MAP_PADDING);
    }

    if (y < 0) {
        return (MAP_PADDING);
    }

    if (x >= JIGPIECE_WIDTH) {
        return (MAP_PADDING);
    }

    if (y >= JIGPIECE_HEIGHT) {
        return (MAP_PADDING);
    }

    return (dg->jigpiece[which].c[x][y]);
}

/*
 * jigpiece_create_exits
 */
static void jigpiece_create_exits (dungeon_t *dg)
{
    int32_t which;
    int32_t x;
    int32_t y;

    for (which = 0; which < dg->jigpieces_cnt; which++) {
        if (dg->jigpiece[which].empty) {
            continue;
        }

        /*
         * ?.......
         * ?.......
         * ?.......
         * ?.......
         * ?.......
         * ?.......
         * ?.......
         * ?.......
         */
        x = 0;
        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
            if (jigpiece_char_is_occupiable(
                                        jigpiece_char_at(dg, which, x, y))) {
                dg->jigpiece[which].exits[DIR_WEST] |= 1<<y;
            }
        }

        /*
         * .......?
         * .......?
         * .......?
         * .......?
         * .......?
         * .......?
         * .......?
         * .......?
         */
        x = JIGPIECE_WIDTH-1;
        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
            if (jigpiece_char_is_occupiable(
                                        jigpiece_char_at(dg, which, x, y))) {
                dg->jigpiece[which].exits[DIR_EAST] |= 1<<y;
            }
        }

        /*
         * ????????
         * ........
         * ........
         * ........
         * ........
         * ........
         * ........
         * ........
         */
        y = 0;
        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            if (jigpiece_char_is_occupiable(
                                        jigpiece_char_at(dg, which, x, y))) {
                dg->jigpiece[which].exits[DIR_NORTH] |= 1<<x;
            }
        }

        /*
         * ........
         * ........
         * ........
         * ........
         * ........
         * ........
         * ........
         * ????????
         */
        y = JIGPIECE_HEIGHT-1;
        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            if (jigpiece_char_is_occupiable(
                                        jigpiece_char_at(dg, which, x, y))) {
                dg->jigpiece[which].exits[DIR_SOUTH] |= 1<<x;
            }
        }
    }
}

/*
 * jigpiece_print
 */
void jigpiece_print(dungeon_t *dg, int32_t which);
void jigpiece_print (dungeon_t *dg, int32_t which)
{
    int32_t x;
    int32_t y;

    for (y = 0; y < JIGPIECE_HEIGHT; y++) {
        for (x = 0; x < JIGPIECE_WIDTH; x++) {

#ifdef MAZE_DEBUG_PRINT_EXITS
            if (which) {
                if (x == 0) {
                    if (dg->jigpiece[which].exits[DIR_WEST] & (1 << y)) {
                        putchar(MAP_EXIT_WEST);
                        continue;
                    }
                } else if (x == JIGPIECE_WIDTH-1) {
                    if (dg->jigpiece[which].exits[DIR_EAST] & (1 << y)) {
                        putchar(MAP_EXIT_EAST);
                        continue;
                    }
                }

                if (y == 0) {
                    if (dg->jigpiece[which].exits[DIR_NORTH] & (1 << x)) {
                        putchar(MAP_EXIT_NORTH);
                        continue;
                    }
                } else if (y == JIGPIECE_HEIGHT-1) {
                    if (dg->jigpiece[which].exits[DIR_SOUTH] & (1 << x)) {
                        putchar(MAP_EXIT_SOUTH);
                        continue;
                    }
                }
            }
#endif
            putchar(dg->jigpiece[which].c[x][y]);
        }
        putchar('\n');
    }
    putchar('\n');
}

/*
 * frag_print
 */
void frag_print(dungeon_t *dg, int32_t dir, int32_t which);
void frag_print (dungeon_t *dg, int32_t dir, int32_t which)
{
    int32_t x;
    int32_t y;

    for (y = 0; y < JIGPIECE_HEIGHT; y++) {
        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            putchar(dg->frag[dir][which].c[x][y]);
        }
        putchar('\n');
    }
    putchar('\n');
}

/*
 * jigpiece_printat
 */
static void jigpiece_printat (dungeon_t *dg, 
                              int32_t atx, int32_t aty, int32_t which)
{
    int32_t x;
    int32_t y;

    for (y = 0; y < JIGPIECE_HEIGHT; y++) {
        map_jigsaw_buffer_goto(atx, aty + y);

        for (x = 0; x < JIGPIECE_WIDTH; x++) {

#ifdef MAZE_DEBUG_PRINT_EXITS
            if (which) {
                if (x == 0) {
                    if (dg->jigpiece[which].exits[DIR_WEST] & (1 << y)) {
                        map_jigsaw_buffer_putchar(MAP_EXIT_WEST);
                        continue;
                    }
                } else if (x == JIGPIECE_WIDTH-1) {
                    if (dg->jigpiece[which].exits[DIR_EAST] & (1 << y)) {
                        map_jigsaw_buffer_putchar(MAP_EXIT_EAST);
                        continue;
                    }
                }

                if (y == 0) {
                    if (dg->jigpiece[which].exits[DIR_NORTH] & (1 << x)) {
                        map_jigsaw_buffer_putchar(MAP_EXIT_NORTH);
                        continue;
                    }
                } else if (y == JIGPIECE_HEIGHT-1) {
                    if (dg->jigpiece[which].exits[DIR_SOUTH] & (1 << x)) {
                        map_jigsaw_buffer_putchar(MAP_EXIT_SOUTH);
                        continue;
                    }
                }
            }
#endif

            map_jigsaw_buffer_putchar(dg->jigpiece[which].c[x][y]);
        }
    }
}

/*
 * jigpiece_printat_with_border
 */
void jigpiece_printat_with_border(dungeon_t *dg, int32_t atx, int32_t aty, int32_t which);
void jigpiece_printat_with_border (dungeon_t *dg, int32_t atx, int32_t aty, int32_t which)
{
    int32_t x;
    int32_t y;

    map_jigsaw_buffer_goto(atx, aty);

    for (x = 0; x < JIGPIECE_WIDTH + 2; x++) {
        map_jigsaw_buffer_putchar(MAP_PADDING);
    }

    map_jigsaw_buffer_goto(atx, aty+1);

    for (y = 0; y < JIGPIECE_HEIGHT; y++) {
        map_jigsaw_buffer_goto(atx, aty + y + 1);
        map_jigsaw_buffer_putchar(MAP_PADDING);

        for (x = 0; x < JIGPIECE_WIDTH; x++) {

            if (which == 0) {
                map_jigsaw_buffer_putchar(MAP_PADDING);
                continue;
            }

#ifdef MAZE_DEBUG_PRINT_EXITS
            if (x == 0) {
                if (dg->jigpiece[which].exits[DIR_WEST] & (1 << y)) {
                    map_jigsaw_buffer_putchar(MAP_EXIT_WEST);
                    continue;
                }
            } else if (x == JIGPIECE_WIDTH-1) {
                if (dg->jigpiece[which].exits[DIR_EAST] & (1 << y)) {
                    map_jigsaw_buffer_putchar(MAP_EXIT_EAST);
                    continue;
                }
            }

            if (y == 0) {
                if (dg->jigpiece[which].exits[DIR_NORTH] & (1 << x)) {
                    map_jigsaw_buffer_putchar(MAP_EXIT_NORTH);
                    continue;
                }
            } else if (y == JIGPIECE_HEIGHT-1) {
                if (dg->jigpiece[which].exits[DIR_SOUTH] & (1 << x)) {
                    map_jigsaw_buffer_putchar(MAP_EXIT_SOUTH);
                    continue;
                }
            }
#endif
            map_jigsaw_buffer_putchar(dg->jigpiece[which].c[x][y]);
        }

        map_jigsaw_buffer_putchar(MAP_PADDING);
    }

    map_jigsaw_buffer_goto(atx, aty + y + 1);

    for (x = 0; x < JIGPIECE_WIDTH + 2; x++) {
        map_jigsaw_buffer_putchar(MAP_PADDING);
    }
}

/*
 * jigpiece_count_char_types
 *
 * How many of whatever type of map char are in each which
 */
static void jigpiece_count_char_types (dungeon_t *dg, int32_t which)
{
    int32_t x;
    int32_t y;

    if (dg->jigpiece[which].empty) {
        return;
    }

    for (y = 0; y < JIGPIECE_HEIGHT; y++) {

        for (x = 0; x < JIGPIECE_WIDTH; x++) {

            if (which == 0) {
                continue;
            }

            if (x == 0) {
                if (dg->jigpiece[which].exits[DIR_WEST] & (1 << y)) {
                    dg->jigpiece[which].has[MAP_EXIT_WEST]++;
                    continue;
                }
            } else if (x == JIGPIECE_WIDTH-1) {
                if (dg->jigpiece[which].exits[DIR_EAST] & (1 << y)) {
                    dg->jigpiece[which].has[MAP_EXIT_EAST]++;
                    continue;
                }
            }

            if (y == 0) {
                if (dg->jigpiece[which].exits[DIR_NORTH] & (1 << x)) {
                    dg->jigpiece[which].has[MAP_EXIT_NORTH]++;
                    continue;
                }
            } else if (y == JIGPIECE_HEIGHT-1) {
                if (dg->jigpiece[which].exits[DIR_SOUTH] & (1 << x)) {
                    dg->jigpiece[which].has[MAP_EXIT_SOUTH]++;
                    continue;
                }
            }

            dg->jigpiece[which].has[(int32_t)dg->jigpiece[which].c[x][y]]++;
        }
    }
}

/*
 * maze_print_cells
 */
static inline void maze_print_cells (dungeon_t *dg)
{
    int32_t w = MAP_JIGSAW_PIECES_ACROSS;
    int32_t h = MAP_JIGSAW_PIECES_DOWN;
    char which;
    int32_t y;
    int32_t x;

    printf("\n\n");

    for (x = 0; x < w; x++) {
        printf("+---");
    }

    printf("+\n");

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            maze_cell_t *c = MAZE_CELL(dg->maze, x, y);

            which = ' ';

            if (c->dead) {
                which = 'D';
            } else if (c->start) {
                which = 'S';
            } else if (c->end) {
                which = 'E';
            } else if (c->best) {
                which = '*';
            } else if (c->solved) {
                which = ' ';
            }

            if ((x > 0) && MAZE_HAS_EXIT(dg->maze, x, y, DIR_WEST)) {
                printf("  %c ", which);
            } else {
                printf("| %c ", which);
            }
        }

        printf("|\n");

        for (x = 0; x < w; x++) {
            if ((y < h) && MAZE_HAS_EXIT(dg->maze, x, y, DIR_SOUTH)) {
                printf("+   ");
            } else {
                printf("+---");
            }
        }

        printf("+\n");
    }
}

/*
 * jigpiece_add_frag
 *
 * Replace a fragment of the maze to make it more interesting.
 */
static void jigpiece_add_frag (dungeon_t *dg)
{
    int32_t f;
    int32_t i;
    int32_t c;
    int32_t x;
    int32_t y;
    int32_t ax;
    int32_t ay;
    int32_t cx;
    int32_t cy;
    int32_t tries;
    int32_t dir;
    int32_t frag;
    int32_t alt;
    int32_t map;

    /*
     * For each frag.
     */
    for (f = 0; f < dg->frag_cnt; f++) {

        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
            for (x = 0; x < JIGPIECE_WIDTH; x++) {
                uint32_t fragchar = dg->frag[0][f].c[x][y];

                if (!valid_frag_char[fragchar]) {
                    for (y = 0; y < JIGPIECE_HEIGHT; y++) {
                        for (x = 0; x < JIGPIECE_WIDTH; x++) {
                            uint32_t fragchar = dg->frag[0][f].c[x][y];

                            printf("%c", fragchar);
                        }
                        printf("\n");
                    }
                    printf("\n");

                    ERR("invalid fragment char [0x%x/%c] above", fragchar, fragchar);
                }
            }
        }
    }

    int F;

    for (F = 0; F < dg->frag_cnt; F++) {

        int f = myrand() % dg->frag_cnt;

        /*
         * For each orientation of a frag.
         */
        for (dir = 0; dir < MAZE_FRAG_DIRECTIONS; dir++) {

            if (dg->frag[dir][f].empty) {
                continue;
            }

            /*
             * For each orientation of a frag.
             */
            for (c = 0; c < dg->frag_cnt_alts[f]; c++) {
#ifdef MAZE_DEBUG_SHOW_AS_GENERATING_FRAGMENTS
                maze_print_cells(dg);
                map_jigsaw_buffer_print();
#endif
                /*
                 * Try and place the frag.
                 */
                tries = 0;
                while (tries++ < MAX_MAP_NUMBER_OF_TIMES_TO_TRY_AND_PLACE_FRAG) {

                    ax = myrand() % (MAP_WIDTH + JIGPIECE_WIDTH);
                    ay = myrand() % (MAP_HEIGHT + JIGPIECE_HEIGHT);
                    ax -= JIGPIECE_WIDTH;
                    ay -= JIGPIECE_HEIGHT;

                    for (x = 0; x < JIGPIECE_WIDTH; x++) {
                        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
                            /*
                             * Skip empty spaces.
                             */
                            frag = dg->frag[dir][f].c[x][y];
                            if (frag == MAP_EMPTY) {
                                continue;
                            }

                            cx = ax + x;
                            cy = ay + y;

                            /*
                             * It's ok to be off map but only if a space.
                             */
                            if ((cx < 0) || (cx >= MAP_WIDTH) ||
                                (cy < 0) || (cy >= MAP_HEIGHT)) {
                                goto next;
                            }

                            /*
                             * Check the frag is an exact match.
                             */
                            map = map_jigsaw_buffer_getchar(cx, cy);
                            if (map != frag) {
                                /*
                                 * Allow the frag to force that spaces
                                 * must exist.
                                 */
                                if (frag == MAP_SPACE) {
                                    if (map == MAP_EMPTY) {
                                        continue;
                                    }
                                }
                                goto next;
                            }
                        }
                    }

                    /*
                     * Choose something to replace the frag.
                     */
                    do {
                        i = myrand() % dg->frag_cnt_alts[f];
                        i += dg->frag_to_alt_base[f];

                    } while (dg->frag_alt[dir][i].empty);

                    /*
                     * Place the frag.
                     */
                    for (x = 0; x < JIGPIECE_WIDTH; x++) {
                        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
                            alt = dg->frag_alt[dir][i].c[x][y];
                            frag = dg->frag[dir][f].c[x][y];

                            cx = ax + x;
                            cy = ay + y;

                            /*
                             * Skip off map.
                             */
                            if ((cx < 0) || (cx >= MAP_WIDTH) ||
                                (cy < 0) || (cy >= MAP_HEIGHT)) {
                                continue;
                            }

                            /*
                             * If the alternative has space and the frag
                             * does not, overwrite it.
                             */
                            if (alt == MAP_EMPTY) {
                                if (frag == MAP_EMPTY) {
                                    continue;
                                }
                            }

                            map_jigsaw_buffer_goto(cx, cy);
                            map_jigsaw_buffer_putchar(alt);
                        }
                    }

                    break;
next:
                    continue;
                }
            }
        }
    }
}

/*
 * jigpiece_add_triggers
 *
 * Replace a fragment of the maze to make it more interesting.
 */
static void jigpiece_add_triggers (dungeon_t *dg)
{
    int32_t f;
    int32_t i;
    int32_t c;
    int32_t x;
    int32_t y;
    int32_t ax;
    int32_t ay;
    int32_t cx;
    int32_t cy;
    int32_t tries;
    int32_t dir;
    int32_t frag;
    int32_t action1;
    int32_t action2;
    int32_t action3;
    int32_t alt;
    int32_t map;
    int32_t color_n = 0;
    const char *color;

    memset(map_jigsaw_buffer_action1, 0, sizeof(map_jigsaw_buffer_action1));
    memset(map_jigsaw_buffer_action2, 0, sizeof(map_jigsaw_buffer_action2));
    memset(map_jigsaw_buffer_action3, 0, sizeof(map_jigsaw_buffer_action3));
    memset(map_jigsaw_buffer_color, 0, sizeof(map_jigsaw_buffer_color));
    memset(map_jigsaw_buffer_solved, 0, sizeof(map_jigsaw_buffer_solved));

    /*
     * For each frag.
     */
    for (f = 0; f < dg->trigger_fragment_cnt; f++) {

        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
            for (x = 0; x < JIGPIECE_WIDTH; x++) {
                uint32_t fragchar = dg->frag[0][f].c[x][y];

                if (!valid_frag_char[fragchar]) {
                    for (y = 0; y < JIGPIECE_HEIGHT; y++) {
                        for (x = 0; x < JIGPIECE_WIDTH; x++) {
                            uint32_t fragchar = dg->frag[0][f].c[x][y];

                            printf("%c", fragchar);
                        }
                        printf("\n");
                    }
                    printf("\n");

                    ERR("invalid fragment char [0x%x/%c] above", fragchar, fragchar);
                }
            }
        }
    }

    int F;

    for (F = 0; F < dg->trigger_fragment_cnt; F++) {

        int f = myrand() % dg->trigger_fragment_cnt;

        if ((myrand() % 100) > 5) {
            continue;
        }

        /*
         * For each orientation of a frag.
         */
        for (dir = 0; dir < MAZE_FRAG_DIRECTIONS; dir++) {

            if (dg->frag[dir][f].empty) {
                continue;
            }

            /*
             * For each orientation of a frag.
             */
            for (c = 0; c < dg->trigger_cnt_alts[f]; c++) {
#ifdef MAZE_DEBUG_SHOW_AS_GENERATING_FRAGMENTS
                maze_print_cells(dg);
                map_jigsaw_buffer_print();
#endif
                /*
                 * Try and place the frag.
                 */
                tries = 0;
                while (tries++ < MAX_MAP_NUMBER_OF_TIMES_TO_TRY_AND_PLACE_FRAG) {

                    ax = myrand() % (MAP_WIDTH + JIGPIECE_WIDTH);
                    ay = myrand() % (MAP_HEIGHT + JIGPIECE_HEIGHT);
                    ax -= JIGPIECE_WIDTH;
                    ay -= JIGPIECE_HEIGHT;

                    for (x = 0; x < JIGPIECE_WIDTH; x++) {
                        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
                            /*
                             * Skip empty spaces.
                             */
                            frag = dg->trigger_fragment[dir][f].c[x][y];
                            if (frag == MAP_EMPTY) {
                                continue;
                            }

                            cx = ax + x;
                            cy = ay + y;

                            /*
                             * It's ok to be off map but only if a space.
                             */
                            if ((cx < 0) || (cx >= MAP_WIDTH) ||
                                (cy < 0) || (cy >= MAP_HEIGHT)) {
                                goto next;
                            }

                            /*
                             * Check the frag is an exact match.
                             */
                            map = map_jigsaw_buffer_getchar(cx, cy);
                            if (map != frag) {
                                /*
                                 * Allow the frag to force that spaces
                                 * must exist.
                                 */
                                if (frag == MAP_SPACE) {
                                    if (map == MAP_EMPTY) {
                                        continue;
                                    }
                                }
                                goto next;
                            }
                        }
                    }

                    /*
                     * Choose something to replace the frag.
                     */
                    do {
                        i = myrand() % dg->trigger_cnt_alts[f];
                        i += dg->trigger_fragment_to_alt_base[f];

                    } while (dg->trigger[dir][i].empty);

                    color = color_find_nth(color_n++);

                    dungeon_resize_trigger(dg, i);
                    dungeon_resize_trigger_fragment(dg, f);
                    dungeon_resize_action1(dg, i);
                    dungeon_resize_action2(dg, i);
                    dungeon_resize_action3(dg, i);

                    /*
                     * Place the frag.
                     */
                    for (x = 0; x < JIGPIECE_WIDTH; x++) {
                        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
                            frag = dg->trigger_fragment[dir][f].c[x][y];
                            alt = dg->trigger[dir][i].c[x][y];
                            action1 = dg->action1[dir][i].c[x][y];
                            action2 = dg->action2[dir][i].c[x][y];
                            action3 = dg->action3[dir][i].c[x][y];

                            cx = ax + x;
                            cy = ay + y;

                            /*
                             * Skip off map.
                             */
                            if ((cx < 0) || (cx >= MAP_WIDTH) ||
                                (cy < 0) || (cy >= MAP_HEIGHT)) {
                                continue;
                            }

                            map_jigsaw_buffer_goto(cx, cy);

                            if (alt != frag) {
                                map_jigsaw_buffer_putchar(alt);
                                map_jigsaw_buffer_at_x--;
                            }

                            if (action1 != frag) {
                                map_jigsaw_buffer_action1[map_jigsaw_buffer_at_x][map_jigsaw_buffer_at_y] = 
                                            action1;
                            }

                            if (action2 != frag) {
                                map_jigsaw_buffer_action2[map_jigsaw_buffer_at_x][map_jigsaw_buffer_at_y] = 
                                            action2;
                            }

                            if (action3 != frag) {
                                map_jigsaw_buffer_action3[map_jigsaw_buffer_at_x][map_jigsaw_buffer_at_y] = 
                                            action3;
                            }

                            map_jigsaw_buffer_color[map_jigsaw_buffer_at_x][map_jigsaw_buffer_at_y] = 
                                            color;

                        }
                    }

                    break;
next:
                    continue;
                }
            }
        }
    }
}

/*
 * jigpiece_create_mirrored_pieces
 */
static void jigpiece_create_mirrored_pieces (dungeon_t *dg)
{
    int32_t c;
    int32_t x;
    int32_t y;
    int32_t r;
    int32_t origs_cnt;
    int32_t prev;

    origs_cnt = dg->jigpieces_cnt;

    for (c = 0; c < origs_cnt; c++) {

        /*
         * How many doors etc... are in each jigpiece? We use this to avoid
         * flipping some jigpiece types.
         */
        jigpiece_count_char_types(dg, c);

        /*
         * Generate 3 rotations
         */
        prev = c;
        for (r=0; r<3; r++) {
            /*
             * Rotate 90 degrees
             */
            if (dg->jigpieces_cnt >= JIGPIECE_MAX) {
                ERR("Too many jigpiece to mirror");
            } else dungeon_resize_jigpiece(dg, dg->jigpieces_cnt+1);

            for (x = 0; x < JIGPIECE_WIDTH; x++) {
                for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                    if (dg->jigpiece[c].rotatable) {
                        dg->jigpiece[dg->jigpieces_cnt].c[JIGPIECE_WIDTH - 1 - y][x] =
                                dg->jigpiece[prev].c[x][y];
                    } else {
                        dg->jigpiece[dg->jigpieces_cnt].c[x][y] =
                                dg->jigpiece[prev].c[x][y];
                        dg->jigpiece[dg->jigpieces_cnt].empty = true;
                    }
                }
            }

            jigpiece_count_char_types(dg, dg->jigpieces_cnt);
            prev = dg->jigpieces_cnt++;
        }

        /*
         * Mirror horizontally
         */
        if (dg->jigpieces_cnt >= JIGPIECE_MAX) {
            ERR("Too many jigpiece to mirror");
        } else dungeon_resize_jigpiece(dg, dg->jigpieces_cnt+1);

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->jigpiece[c].horiz_flip) {
                    dg->jigpiece[dg->jigpieces_cnt].c[JIGPIECE_WIDTH - 1 - x][y] =
                            dg->jigpiece[c].c[x][y];
                } else {
                    dg->jigpiece[dg->jigpieces_cnt].c[x][y] =
                            dg->jigpiece[c].c[x][y];
                    dg->jigpiece[dg->jigpieces_cnt].empty = true;
                }
            }
        }

        jigpiece_count_char_types(dg, dg->jigpieces_cnt);
        dg->jigpieces_cnt++;

        /*
         * Mirror vertically
         */
        if (dg->jigpieces_cnt >= JIGPIECE_MAX) {
            ERR("Too many jigpiece to mirror");
        } else dungeon_resize_jigpiece(dg, dg->jigpieces_cnt+1);

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->jigpiece[c].vert_flip) {
                    dg->jigpiece[dg->jigpieces_cnt].c[x][JIGPIECE_HEIGHT - 1 - y] =
                            dg->jigpiece[c].c[x][y];
                } else {
                    dg->jigpiece[dg->jigpieces_cnt].c[x][y] =
                            dg->jigpiece[c].c[x][y];
                    dg->jigpiece[dg->jigpieces_cnt].empty = true;
                }
            }
        }

        jigpiece_count_char_types(dg, dg->jigpieces_cnt);
        dg->jigpieces_cnt++;

        /*
         * Mirror horizontally and vertically
         */
        if (dg->jigpieces_cnt >= JIGPIECE_MAX - 1) {
            ERR("Too many jigpiece to mirror");
        } else dungeon_resize_jigpiece(dg, dg->jigpieces_cnt+2);

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                /*
                 * Bit of a hack, use the next jigpiece as scratch space else
                 * we end up overwriting what we are mirroring vertically
                 * below.
                 */
                if (dg->jigpiece[c].vert_flip && 
                    dg->jigpiece[c].horiz_flip) {

                    dg->jigpiece[dg->jigpieces_cnt+1].c[JIGPIECE_WIDTH - 1 - x][y] =
                            dg->jigpiece[c].c[x][y];
                } else {
                    dg->jigpiece[dg->jigpieces_cnt+1].c[x][y] =
                            dg->jigpiece[c].c[x][y];
                    dg->jigpiece[dg->jigpieces_cnt+1].empty = true;
                }
            }
        }

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->jigpiece[c].vert_flip && 
                    dg->jigpiece[c].horiz_flip) {

                    dg->jigpiece[dg->jigpieces_cnt].c[x][JIGPIECE_HEIGHT - 1 - y] =
                            dg->jigpiece[dg->jigpieces_cnt+1].c[x][y];
                } else {
                    dg->jigpiece[dg->jigpieces_cnt].c[x][y] =
                            dg->jigpiece[dg->jigpieces_cnt+1].c[x][y];
                    dg->jigpiece[dg->jigpieces_cnt].empty = true;
                }
            }
        }

        jigpiece_count_char_types(dg, dg->jigpieces_cnt);
        dg->jigpieces_cnt++;
    }
}

/*
 * jigpiece_create_mirrored_frag
 */
static void jigpiece_create_mirrored_frag (dungeon_t *dg)
{
    int32_t c;
    int32_t x;
    int32_t y;
    int32_t dir;

    dungeon_resize_frag(dg, dg->frag_cnt+2);
    for (c = 0; c < dg->frag_cnt; c++) {
        /*
         * Generate 3 rotations
         */
        for (dir=1; dir<4; dir++) {
            /*
             * Rotate 90 degrees
             */
            for (x = 0; x < JIGPIECE_WIDTH; x++) {
                for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                    if (dg->frag[0][c].rotatable) {
                        dg->frag[dir][c].c[JIGPIECE_WIDTH - 1 - y][x] =
                                        dg->frag[dir-1][c].c[x][y];
                    } else {
                        dg->frag[dir][c].c[x][y] =
                                        dg->frag[dir-1][c].c[x][y];
                        dg->frag[dir][c].empty = true;
                    }
                }
            }
        }

        /*
         * Mirror horizontally
         */
        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->frag[0][c].horiz_flip) {
                    dg->frag[dir][c].c[JIGPIECE_WIDTH - 1 - x][y] =
                                    dg->frag[0][c].c[x][y];
                } else {
                    dg->frag[dir][c].c[x][y] =
                                    dg->frag[0][c].c[x][y];
                    dg->frag[dir][c].empty = true;
                }
            }
        }

        /*
         * Mirror vertically
         */
        dir++;

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->frag[0][c].vert_flip) {

                    dg->frag[dir][c].c[x][JIGPIECE_HEIGHT - 1 - y] =
                                dg->frag[0][c].c[x][y];
                } else {
                    dg->frag[dir][c].c[x][y] =
                                dg->frag[0][c].c[x][y];
                    dg->frag[dir][c].empty = true;
                }
            }
        }

        /*
         * Mirror horizontally and vertically
         */
        dir++;

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                /*
                 * Bit of a hack, use the next frag as scratch space else
                 * we end up overwriting what we are mirroring vertically
                 * below.
                 */
                if (dg->frag[0][c].vert_flip &&
                    dg->frag[0][c].horiz_flip) {

                    dg->frag[dir][c+1].c[JIGPIECE_WIDTH - 1 - x][y] =
                                        dg->frag[0][c].c[x][y];
                } else {
                    dg->frag[dir][c+1].c[x][y] =
                                        dg->frag[0][c].c[x][y];
                    dg->frag[dir][c+1].empty = true;
                }
            }
        }

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->frag[0][c].vert_flip &&
                    dg->frag[0][c].horiz_flip) {

                    dg->frag[dir][c].c[x][JIGPIECE_HEIGHT - 1 - y] =
                                        dg->frag[dir][c+1].c[x][y];
                } else {
                    dg->frag[dir][c].c[x][y] =
                                        dg->frag[dir][c+1].c[x][y];
                    dg->frag[dir][c].empty = true;
                }
            }
        }
    }
}

/*
 * jigpiece_create_mirrored_frag_alt
 */
static void jigpiece_create_mirrored_frag_alt (dungeon_t *dg)
{
    int32_t c;
    int32_t x;
    int32_t y;
    int32_t dir;

    dungeon_resize_frag_alt(dg, dg->frag_alt_cnt+1);
    for (c = 0; c < dg->frag_alt_cnt; c++) {

        for (y = 0; y < JIGPIECE_HEIGHT; y++) {
            for (x = 0; x < JIGPIECE_WIDTH; x++) {
                uint32_t alt = dg->frag_alt[0][c].c[x][y];

                if (!valid_frag_alt_char[alt]) {
                    for (y = 0; y < JIGPIECE_HEIGHT; y++) {
                        for (x = 0; x < JIGPIECE_WIDTH; x++) {
                            uint32_t alt = dg->frag_alt[0][c].c[x][y];

                            printf("%c", alt);
                        }

                        printf("\n");
                    }
                    printf("\n");

                    ERR("invalid fragment alt char [0x%x/%c] above", alt, alt);
                }
            }
        }

        /*
         * Generate 3 rotations
         */
        for (dir=1; dir<4; dir++) {
            /*
             * Rotate 90 degrees
             */
            for (x = 0; x < JIGPIECE_WIDTH; x++) {
                for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                    if (dg->frag_alt[0][c].rotatable) {
                        dg->frag_alt[dir][c].c[JIGPIECE_WIDTH - 1 - y][x] =
                                        dg->frag_alt[dir-1][c].c[x][y];
                    } else {
                        dg->frag_alt[dir][c].c[x][y] =
                                        dg->frag_alt[dir-1][c].c[x][y];
                        dg->frag_alt[dir][c].empty = true;
                    }
                }
            }
        }

        /*
         * Mirror horizontally
         */
        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->frag_alt[0][c].horiz_flip) {

                    dg->frag_alt[dir][c].c[JIGPIECE_WIDTH - 1 - x][y] =
                                    dg->frag_alt[0][c].c[x][y];
                } else {
                    dg->frag_alt[dir][c].c[x][y] =
                                    dg->frag_alt[0][c].c[x][y];
                    dg->frag_alt[dir][c].empty = true;
                }
            }
        }

        /*
         * Mirror vertically
         */
        dir++;

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->frag_alt[0][c].vert_flip) {

                    dg->frag_alt[dir][c].c[x][JIGPIECE_HEIGHT - 1 - y] =
                                dg->frag_alt[0][c].c[x][y];
                } else {
                    dg->frag_alt[dir][c].c[x][y] =
                                dg->frag_alt[0][c].c[x][y];
                    dg->frag_alt[dir][c].empty = true;
                }
            }
        }

        /*
         * Mirror horizontally and vertically
         */
        dir++;

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                /*
                 * Bit of a hack, use the next frag as scratch space else
                 * we end up overwriting what we are mirroring vertically
                 * below.
                 */
                if (dg->frag_alt[0][c].vert_flip &&
                    dg->frag_alt[0][c].horiz_flip) {

                    dg->frag_alt[dir][c+1].c[JIGPIECE_WIDTH - 1 - x][y] =
                                        dg->frag_alt[0][c].c[x][y];
                } else {
                    dg->frag_alt[dir][c+1].c[x][y] =
                                        dg->frag_alt[0][c].c[x][y];
                    dg->frag_alt[dir][c+1].empty = true;
                }
            }
        }

        for (x = 0; x < JIGPIECE_WIDTH; x++) {
            for (y = 0; y < JIGPIECE_HEIGHT; y++) {

                if (dg->frag_alt[0][c].vert_flip &&
                    dg->frag_alt[0][c].horiz_flip) {

                    dg->frag_alt[dir][c].c[x][JIGPIECE_HEIGHT - 1 - y] =
                                        dg->frag_alt[dir][c+1].c[x][y];
                } else {
                    dg->frag_alt[dir][c].c[x][y] =
                                        dg->frag_alt[dir][c+1].c[x][y];
                    dg->frag_alt[dir][c].empty = true;
                }
            }
        }
    }
}

/*
 * jigpiece_intersect_score
 */
static int32_t jigpiece_intersect_score (dungeon_t *dg, int32_t a, int32_t dir, int32_t b)
{
    return (
        (dg->jigpiece[a].exits[dir] == dg->jigpiece[b].exits[3 - dir]) ?
            1 : 0);
}

/*
 * maze_generate_all_random_directions
 */
static void maze_generate_all_random_directions (dungeon_t *dg, maze_cell_t * c, int depth)
{
    int32_t dir;
    int32_t any_dir;
    int32_t new_dir;

    c->walked = 1;

    while (1) {
        /*
         * Check we have somewhere we can go to.
         */
        for (dir = 0, any_dir = 0; dir < 4; dir++) {
            any_dir += MAZE_CAN_GO(c, dir);
        }

        /*
         * At the end of the maze?
         */
        if (!any_dir) {
            return;
        }

        /*
         * Choose a random dir to walk.
         */
        do {
            new_dir = myrand() % 4;
        } while (!MAZE_CAN_GO(c, new_dir));


        if ((depth > 2) &&
            ((myrand() % 100) < MAZE_HOW_LIKELY_PERCENT_ARE_END_CORRIDORS * depth)) {
            if (!c->exit[new_dir]->walked &&
                !c->exit[new_dir]->exits) {
                c->exit[new_dir]->walked = 1;
                c->exit[new_dir]->dead = 1;
                return;
            }
        }

        c->exits |= (1 << new_dir);
        c->exit[new_dir]->exits |= (1 << (3 - new_dir));

        if ((myrand() % 100) < MAZE_HOW_LIKELY_PERCENT_ARE_FORKS) {
            do {
                new_dir = myrand() % 4;
            } while (!MAZE_CAN_GO(c, new_dir));

            c->exits |= (1 << new_dir);
            c->exit[new_dir]->exits |= (1 << (3 - new_dir));
        }

        maze_generate_all_random_directions(dg, c->exit[new_dir], depth + 1);
    }
}

/*
 * Wrap all corridors in walls
 */
static void maze_add_decorations (void)
{
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;

    for (x = 0; x < MAP_WIDTH; x++) {
        map_jigsaw_buffer_goto(x, 0);
        map_jigsaw_buffer_putchar(MAP_ROCK);
        map_jigsaw_buffer_goto(x, MAP_HEIGHT - 1);
        map_jigsaw_buffer_putchar(MAP_ROCK);
    }

    for (y = 0; y < MAP_HEIGHT; y++) {
        map_jigsaw_buffer_goto(0, y);
        map_jigsaw_buffer_putchar(MAP_ROCK);
        map_jigsaw_buffer_goto(MAP_WIDTH - 1, y);
        map_jigsaw_buffer_putchar(MAP_ROCK);
    }

    LOG("Maze: Added borders:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);

    for (x = 1; x < MAP_WIDTH - 1; x++) {
        for (y = 1; y < MAP_HEIGHT - 1; y++) {

            if ((map_jigsaw_buffer_getchar(x, y) != MAP_CORRIDOR)) {
                continue;
            }

            for (dx = -1; dx <=1; dx++) {
                for (dy = -1; dy <=1; dy++) {
                    if (map_jigsaw_buffer_getchar(x+dx, y+dy) == MAP_EMPTY) {
                        map_jigsaw_buffer_goto(x+dx, y+dy);
                        map_jigsaw_buffer_putchar(MAP_CORRIDOR_WALL);
                    }
                }
            }
        }
    }

    LOG("Maze: Added corridor walls:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);

    /*
     * Make sure all floor tiles have a wall around them.
     */
    for (x = 1; x < MAP_WIDTH - 1; x++) {
        for (y = 1; y < MAP_HEIGHT - 1; y++) {

            if ((map_jigsaw_buffer_getchar(x, y) != MAP_FLOOR)) {
                continue;
            }

            for (dx = -1; dx <=1; dx++) {
                for (dy = -1; dy <=1; dy++) {
                    if (map_jigsaw_buffer_getchar(x+dx, y+dy) == MAP_EMPTY) {
                        map_jigsaw_buffer_goto(x+dx, y+dy);
                        map_jigsaw_buffer_putchar(MAP_WALL);
                    }
                }
            }
        }
    }

    LOG("Maze: Added walls around floor tiles:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);

    /*
     * Extra thick walls.
     */
    {
        int thick;
        int depth = myrand() % 2;

        for (thick = 0; thick < depth; thick++) {
            memcpy(map_jigsaw_buffer_old,
                map_jigsaw_buffer, sizeof(map_jigsaw_buffer));

            for (x = 1; x < MAP_WIDTH - 1; x++) {
                for (y = 1; y < MAP_HEIGHT - 1; y++) {

                    if (map_jigsaw_buffer_old_getchar(x, y) != MAP_WALL) {
                        continue;
                    }

                    for (dx = -1; dx <=1; dx++) {
                        for (dy = -1; dy <=1; dy++) {
                            if (map_jigsaw_buffer_old_getchar(x+dx, y+dy) == MAP_EMPTY) {
                                map_jigsaw_buffer_goto(x+dx, y+dy);
                                map_jigsaw_buffer_putchar(MAP_WALL);
                            }
                        }
                    }
                }
            }

            if (depth) {
                LOG("Maze: Added thick walls:");
                //map_jigsaw_buffer_print_file(MY_STDOUT);
            }
        }
    }

#if 0
    /*
     * Not so great as it can encase monsters.
     */

    /*
     * Random thick walls.
     */
    {
        int count;
        int depth = myrand() % 2;

        for (count = 0; count < MAP_WIDTH * MAP_HEIGHT * depth; count++) {
            x = myrand() % MAP_WIDTH;
            y = myrand() % MAP_HEIGHT;

            if (map_jigsaw_buffer_getchar(x, y) != MAP_WALL) {
                continue;
            }

            for (dx = -1; dx <=1; dx++) {
                for (dy = -1; dy <=1; dy++) {
                    if (map_jigsaw_buffer_getchar(x+dx, y+dy) == MAP_EMPTY) {
                        map_jigsaw_buffer_goto(x+dx, y+dy);
                        map_jigsaw_buffer_putchar(MAP_WALL);
                    }
                }
            }
        }

        if (depth) {
            LOG("Maze: Added thick random walls:");
            map_jigsaw_buffer_print_file(MY_STDOUT);
        }
    }
#endif
}

static int maze_flood_find (int32_t x, int32_t y, char find, int depth)
{
    static char walked[MAP_WIDTH][MAP_HEIGHT];

    if (!depth) {
        memset(walked, ' ', sizeof(walked));
        memset(map_jigsaw_buffer_solved, 0, sizeof(map_jigsaw_buffer_solved));
    }

    if (walked[x][y] != ' ') {
        return (false);
    }

    if (map_jigsaw_buffer_getchar(x, y) == find) {
        map_jigsaw_buffer_solved[x][y] = 1;
        return (depth);
    }

    walked[x][y] = '.';

    if (jigpiece_char_is_passable(map_jigsaw_buffer_getchar(x-1, y))) {
        int solved = maze_flood_find(x-1, y, find, depth + 1);
        if (solved) {
            map_jigsaw_buffer_solved[x][y] = 1;
            return (solved);
        }
    }

    if (jigpiece_char_is_passable(map_jigsaw_buffer_getchar(x+1, y))) {
        int solved = maze_flood_find(x+1, y, find, depth + 1);
        if (solved) {
            map_jigsaw_buffer_solved[x][y] = 1;
            return (solved);
        }
    }

    if (jigpiece_char_is_passable(map_jigsaw_buffer_getchar(x, y-1))) {
        int solved = maze_flood_find(x, y-1, find, depth + 1);
        if (solved) {
            map_jigsaw_buffer_solved[x][y] = 1;
            return (solved);
        }
    }

    if (jigpiece_char_is_passable(map_jigsaw_buffer_getchar(x, y+1))) {
        int solved = maze_flood_find(x, y+1, find, depth + 1);
        if (solved) {
            map_jigsaw_buffer_solved[x][y] = 1;
            return (solved);
        }
    }

    return (false);
}

static int maze_check_exit_can_be_reached (void)
{
    int32_t x;
    int32_t y;
    int32_t sx;
    int32_t sy;
    int found_start = 0;

    y = 1;
    for (x = 1; x < MAP_WIDTH - 1; x++) {
        for (y = 1; y < MAP_HEIGHT - 1; y++) {

            if ((map_jigsaw_buffer_getchar(x, y) == MAP_START)) {
                found_start = 1;
                break;
            }
        }

        if (found_start) {
            break;
        }
    }

    if (!found_start) {
        return (false);
    }

    sx = x;
    sy = y;

    int found_end = 0;

    for (x = 1; x < MAP_WIDTH - 1; x++) {
        for (y = 1; y < MAP_HEIGHT - 1; y++) {

            if ((map_jigsaw_buffer_getchar(x, y) == MAP_END)) {
                found_end = 1;
                break;
            }
        }

        if (found_end) {
            break;
        }
    }

    if (!found_end) {
        return (false);
    }

    int depth = maze_flood_find(sx, sy, MAP_END, 0);
    if (depth <= MAZE_MIN_DISTANCE_START_AND_END) {
        return (false);
    }

    return (true);
}

/*
 * Find "old" in a room and replace with "new"
 */
static uint8_t 
maze_replace_room_char (char new_char)
{
    uint32_t tries = MAP_WIDTH * MAP_HEIGHT * 10;

    while (tries--) {
        uint32_t cx = myrand() % MAP_WIDTH;
        uint32_t cy = myrand() % MAP_HEIGHT;
        
        if (jigpiece_char_is_dungeon_floor_or_corridor(map_jigsaw_buffer_getchar(cx, cy)) &&
            jigpiece_char_is_dungeon_floor_or_corridor(map_jigsaw_buffer_getchar(cx - 1, cy)) &&
            jigpiece_char_is_dungeon_floor_or_corridor(map_jigsaw_buffer_getchar(cx, cy - 1)) &&
            jigpiece_char_is_dungeon_floor_or_corridor(map_jigsaw_buffer_getchar(cx + 1, cy)) &&
            jigpiece_char_is_dungeon_floor_or_corridor(map_jigsaw_buffer_getchar(cx, cy + 1)) &&
            !jigpiece_char_is_monst((map_jigsaw_buffer_getchar(cx-1, cy))) &&
            !jigpiece_char_is_monst((map_jigsaw_buffer_getchar(cx-2, cy))) &&
            !jigpiece_char_is_monst((map_jigsaw_buffer_getchar(cx+1, cy))) &&
            !jigpiece_char_is_monst((map_jigsaw_buffer_getchar(cx+2, cy)))) {

            map_jigsaw_buffer_goto(cx, cy);
            map_jigsaw_buffer_putchar(new_char);

            return (true);
        }
    }

    return (false);
}

static void maze_check_teleports (void)
{
    int32_t x;
    int32_t y;
    int teleport = 0;

    for (x = 1; x < MAP_WIDTH - 1; x++) {
        for (y = 1; y < MAP_HEIGHT - 1; y++) {

            if ((map_jigsaw_buffer_getchar(x, y) == MAP_TELEPORT)) {
                teleport++;
            }
        }
    }

    if (teleport > 1) {
        return;
    }

    for (x = 1; x < MAP_WIDTH - 1; x++) {
        for (y = 1; y < MAP_HEIGHT - 1; y++) {
            if ((map_jigsaw_buffer_getchar(x, y) == MAP_TELEPORT)) {
                map_jigsaw_buffer_goto(x, y);
                map_jigsaw_buffer_putchar(MAP_ROCK);
            }
        }
    }
}

/*
 * maze_dump_jigpieces_to_map
 */
static void dump_jigpieces_to_map (dungeon_t *dg)
{
    int32_t w = MAP_JIGSAW_PIECES_ACROSS;
    int32_t h = MAP_JIGSAW_PIECES_DOWN;
    int32_t y;
    int32_t x;

    /*
     * Offset the map by an amount so we can have a border.
     */
    int dx = ((MAP_WIDTH - (JIGPIECE_WIDTH * MAP_JIGSAW_PIECES_ACROSS)) / 2);
    int dy = ((MAP_HEIGHT - (JIGPIECE_HEIGHT * MAP_JIGSAW_PIECES_DOWN)) / 2);

    for (y = 0; y < MAP_HEIGHT; y++) {
        for (x = 0; x < MAP_WIDTH; x++) {
            map_jigsaw_buffer_goto(x, y);
            map_jigsaw_buffer_putchar(MAP_EMPTY);
        }
    }

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {

            maze_cell_t *c = MAZE_CELL(dg->maze, x, y);

            if (c->jigpiece_index) {
                jigpiece_printat(dg,
                                (JIGPIECE_WIDTH * x) + dx,
                                (JIGPIECE_HEIGHT * y) + dy, c->jigpiece_index);
            }
        }
    }
}

#ifdef MAZE_DEBUG_SHOW_AS_GENERATING
/*
 * maze_debug
 */
static void maze_debug (dungeon_t *dg)
{
    dump_jigpieces_to_map(dg);

    map_jigsaw_buffer_print();

    memset(map_jigsaw_buffer, 0, sizeof(map_jigsaw_buffer));
    putchar('\n');
    map_jigsaw_buffer_set_fgbg(TERM_COLOR_WHITE, TERM_COLOR_BLACK);

    putchar('\n');
}
#endif

/*
 * maze_convert_to_map
 */
static void maze_convert_to_map (dungeon_t *dg)
{
    dump_jigpieces_to_map(dg);
    LOG("Maze: Added maze pieces:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);

    jigpiece_create_mirrored_frag(dg);

    jigpiece_create_mirrored_frag_alt(dg);

    /*
     * Add this before fragments
     */
    LOG("Maze: Added corridor walls:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);

    int x, y, dx, dy;

    for (x = 1; x < MAP_WIDTH - 1; x++) {
        for (y = 1; y < MAP_HEIGHT - 1; y++) {

            if ((map_jigsaw_buffer_getchar(x, y) != MAP_CORRIDOR)) {
                continue;
            }

            for (dx = -1; dx <=1; dx++) {
                for (dy = -1; dy <=1; dy++) {
                    if (map_jigsaw_buffer_getchar(x+dx, y+dy) == MAP_EMPTY) {
                        map_jigsaw_buffer_goto(x+dx, y+dy);
                        map_jigsaw_buffer_putchar(MAP_CORRIDOR_WALL);
                    }
                }
            }
        }
    }

    jigpiece_add_triggers(dg);

    LOG("Maze: Added triggers:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);

    jigpiece_add_frag(dg);
    jigpiece_add_frag(dg);
    jigpiece_add_frag(dg);
    jigpiece_add_frag(dg);
    jigpiece_add_frag(dg);

    LOG("Maze: Added fragments:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);

    maze_add_decorations();

    /*
     * Add random junk outside the level
     */
    for (x = 1; x < MAP_WIDTH - 1; x++) {
        for (y = 1; y < MAP_HEIGHT - 1; y++) {

            if (map_jigsaw_buffer_getchar(x, y) != MAP_EMPTY) {
                continue;
            }

            map_jigsaw_buffer_goto(x, y);

            int r = myrand() % 1000;
            if (r < 250) {
                map_jigsaw_buffer_putchar(MAP_ROCK);
            } else if (r < 252) {
                map_jigsaw_buffer_putchar(MAP_GENERATOR);
            } else if (r < 255) {
                map_jigsaw_buffer_putchar(MAP_LAVA);
            } else if (r < 257) {
                map_jigsaw_buffer_putchar(MAP_TRAP);
            } else if (r < 258) {
                map_jigsaw_buffer_putchar(MAP_TREASURE);
            } else {
                /*
                 * space
                 */
            }
        }
    }

    LOG("Maze: Added junk in outside of level:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);
}

/*
 * maze_jigsaw_generate_all_possible_pieces
 *
 * Make a list of all the jigpiece that satisfy the connection needs of this
 * maze cell.
 */
static uint8_t maze_jigsaw_generate_all_possible_pieces (dungeon_t *dg)
{
    int32_t w = MAP_JIGSAW_PIECES_ACROSS;
    int32_t h = MAP_JIGSAW_PIECES_DOWN;
    maze_cell_t *mcell;
    int32_t dir;
    int32_t x;
    int32_t y;
    int32_t c;
    int32_t a;
    int32_t b;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {

            mcell = MAZE_CELL(dg->maze, x, y);
            mcell->possible_jigpieces_size = 0;
            mcell->x = x;
            mcell->y = y;

            for (c = 1; c < dg->jigpieces_cnt; c++) {

                /*
                 * DOn't have dead end corridors, so filter out all jigpieces 
                 * that are just corridors where the maze cell has only one 
                 * exit, i.e. it's a dead end.
                 */
                if (!MULTIPLE_BITS((int)mcell->exits)) {
                    if (dg->jigpiece[c].has[MAP_CORRIDOR]) {
                        continue;
                    }
                }

                for (dir = 0; dir < DIR_MAX; dir++) {
                    /*
                     * Filter to pieces that have at least one exit in each of
                     * all the directions needed.
                     */
                    a = dg->jigpiece[c].exits[dir] ? 1 : 0;
                    b = MAZE_HAS_EXIT(dg->maze, x, y, dir) ? 1 : 0;

                    if (a != b) {
                        break;
                    }
                }

                if (dir == DIR_MAX) {
                    mcell->possible_jigpieces[mcell->possible_jigpieces_size++] = c;
                }
            }

            if (!mcell->possible_jigpieces_size) {
                printf("cell %d %d\n",x,y);
                maze_print_cells(dg);

                return (false);
            }
        }
    }

    return (true);
}

/*
 * maze_generate_jigpiece_find
 *
 * Returns 0 on failure. 1 on being able to fill the maze with jigsaw pieces.
 */
static int32_t
maze_generate_jigpiece_find (dungeon_t *dg, maze_cell_t *mcell,
                             uint32_t *count)
{
    int32_t intersect_list[mcell->possible_jigpieces_size];
    int32_t intersect_list_size;
    maze_cell_t *ocell;
    int32_t dir;
    int32_t exits;
    int32_t p;
    int32_t c;
    int32_t i;
    int32_t ok = 0;

    (*count)++;

    /*
     * Tried to long to solve this maze? Try another.
     */
    if (*count > MAZE_HOW_LONG_TO_SPEND_TRYING_TO_SOLVE_MAZE) {
        return (0);
    }

#ifdef MAZE_DEBUG_SHOW_AS_GENERATING
    maze_debug(dg);
#endif

    /*
     * Already solved this cell?
     */
    if (mcell->jigpiece_index) {
        return (1);
    }

    c = 0;
    intersect_list_size = 0;

    /*
     * +---+---+
     * | M |   |
     * +   +   +
     * | O |   |
     * +   +   +
     * |       |
     * +---+---+
     *
     * mcell == M
     * ocell == O
     */
    for (p = 0; p < mcell->possible_jigpieces_size; p++) {

        c = mcell->possible_jigpieces[p];

        /*
         * Filter to only cells that satisfy all direction exits.
         */
        for (dir = 0; dir < DIR_MAX; dir++) {

            /*
             * Try to satisfy M and create a cell that joins onto all its
             * neighbors.
             */
            if (!(mcell->exits & (1 << dir))) {
                continue;
            }

            ocell = mcell->exit[dir];
            if (!ocell) {
                continue;
            }

            /*
             * If no neighbor in a given direction, we can't determine what to
             * put.
             */
            if (!ocell->jigpiece_index) {
                continue;
            }

            /*
             * No rooms next to other rooms just to cut the number of rooms in
             * half.
             */
            if (dg->jigpiece[c].has[MAP_FLOOR]) {
                if (dg->jigpiece[ocell->jigpiece_index].has[MAP_FLOOR]) {
                    if ((myrand() % 100) > MAZE_ROOM_NEXT_TO_OTHER_ROOMS_CHANCE) {
                        break;
                    }
                }
            }

            /*
             * If the cells join exactly then this is a possibility.
             */
            if (!jigpiece_intersect_score(dg, c, dir, ocell->jigpiece_index)) {
                break;
            }
        }

        if (dir == DIR_MAX) {
            intersect_list[intersect_list_size++] = c;
        }
    }

    if (!intersect_list_size) {
        mcell->jigpiece_index = 0;
        return (0);
    }

    /*
     * Choose a random jigsaw piece. 
     */
    for (i=0; i < 1 + (myrand() % 3); i++) {

        uint32_t j = intersect_list[myrand() % intersect_list_size];
        jigpiece_t *jp = &dg->jigpiece[j];

        /*
         * Only allow one shop per level.
         */
        int skip = false;
        int m;

        if (jp->has[MAP_SHOP_FLOOR]) {
            for (m = 0; m < MAP_JIGSAW_PIECES_ACROSS * MAP_JIGSAW_PIECES_DOWN; m++) {
                uint32_t k = dg->maze[m].jigpiece_index;
                if (k) {
                    jigpiece_t *kp = &dg->jigpiece[k];
                    if (kp->has[MAP_SHOP_FLOOR]) {
                        skip = true;
                    }
                }
            }
        }

        if (skip) {
            continue;
        }

        mcell->jigpiece_index = j;
        mcell->jp = jp;

        exits = 0;
        ok = 1;

        /*
         * And now make sure it fits all adjoining exits.
         */
        for (dir = 0; dir < DIR_MAX; dir++) {
            if (!(mcell->exits & (1 << dir))) {
                continue;
            }

            ocell = mcell->exit[dir];
            if (!ocell) {
                continue;
            }

            if (ocell->jigpiece_index) {
                continue;
            }

            exits = 1;

            /*
             * If it does not fit any one direction, abort.
             */
            if (!maze_generate_jigpiece_find(dg, ocell, count)) {
                ok = 0;
                break;
            }
        }

        /*
         * If there were no adjoining pieces, then we pass automatically.
         */
        if (!exits) {
            return (1);
        }

        if (!ok) {
            /*
             * This piece does not fit. Try another.
             */
            continue;
        }

        /*
         * This piece fits
         */
        return (1);
    }

    mcell->jigpiece_index = 0;

    return (ok);
}

/*
 * maze_jigsaw_solve
 */
static int32_t maze_jigsaw_solve (dungeon_t *dg)
{
    int32_t w = MAP_JIGSAW_PIECES_ACROSS;
    int32_t h = MAP_JIGSAW_PIECES_DOWN;
    maze_cell_t *mcell;
    maze_cell_t *ocell;
    int32_t x, y;
    int32_t dir;
    int32_t c;

    for (;;) {
        x = myrand() % MAP_JIGSAW_PIECES_ACROSS;
        y = myrand() % MAP_JIGSAW_PIECES_DOWN;

        mcell = MAZE_CELL(dg->maze, x, y);
        if (!mcell->dead) {
            if (mcell->exits) {
                break;
            }
        }
    }

    for (c = 1; c < mcell->possible_jigpieces_size; c++) {

        /*
         * Reset the maze.
         */
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                ocell = MAZE_CELL(dg->maze, x, y);
                ocell->jigpiece_index = 0;
            }
        }

        x = 0;
        y = 0;
        mcell->jigpiece_index =
            mcell->possible_jigpieces[myrand() % mcell->possible_jigpieces_size];

        for (dir = 0; dir < DIR_MAX; dir++) {
            if (!(mcell->exits & (1 << dir))) {
                continue;
            }

            ocell = mcell->exit[dir];
            if (!ocell) {
                continue;
            }

            /*
             * Recursively try to solve the maze jigsaw.
             */
            uint32_t count = 0;

            if (!maze_generate_jigpiece_find(dg, ocell, &count)) {
                break;
            }
        }

        /*
         * If we satisfy all directions, we have a complete solution.
         */
        if (dir == DIR_MAX) {
            return (1);
        }
    }

    return (0);
}

/*
 * maze_solve_search
 */
static int32_t maze_solve_search (dungeon_t *dg, maze_cell_t *c)
{
    int32_t dir;

    if (c->solved) {
        return (0);
    }

    c->solved = 1;
    c->best = 1;

    if (c->end) {
        return (1);
    }

    for (dir = 0; dir < 4; dir++) {
        if (c->exits & (1 << dir)) {
            if (maze_solve_search(dg, c->exit[dir])) {
                return (1);
            }
        }
    }

    c->best = 0;
    return (0);
}


/*
 * maze_solve
 */
static int32_t maze_solve (dungeon_t *dg, int32_t w, int32_t h)
{
    int32_t x, y;
    maze_cell_t *s = 0;
    maze_cell_t *e = 0;
    maze_cell_t *c;
    int count = 0;

    /*
     * Assign a random start and exit.
     */
    while (1) {
        x = myrand() % w;
        y = myrand() % h;

        if (count++ > 10000) {
            return (0);
        }

        c = MAZE_CELL(dg->maze, x, y);

        if (!c->exits) {
            continue;
        }

        if (c == s) {
            continue;
        }

        if (c == e) {
            continue;
        }

        if (!s) {
            s = c;
            dg->sx = x;
            dg->sy = y;
            continue;
        }

        if (!e) {
            e = c;
            dg->ex = x;
            dg->ey = y;

            s->start = 1;
            e->end = 1;

            if (maze_solve_search(dg, s)) {
                return (1);
            }

            s->start = 0;
            e->end = 0;

            return (0);
        }
    }
}

/*
 * maze_generate_and_solve
 */
static uint8_t maze_generate_and_solve (dungeon_t *dg)
{
    int32_t y;
    int32_t x;
    int32_t w = MAP_JIGSAW_PIECES_ACROSS;
    int32_t h = MAP_JIGSAW_PIECES_DOWN;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            maze_cell_t * c = MAZE_CELL(dg->maze, x, y);

            c->exits = 0;
            c->exit[DIR_NORTH] = y > 0 ?
                            MAZE_CELL(dg->maze,   x  , y - 1) : 0;
            c->exit[DIR_SOUTH] = y < h-1 ?
                            MAZE_CELL(dg->maze,   x  , y + 1) : 0;
            c->exit[DIR_WEST]  = x > 0 ?
                            MAZE_CELL(dg->maze, x - 1,   y  ) : 0;
            c->exit[DIR_EAST]  = x < w-1 ?
                            MAZE_CELL(dg->maze, x + 1,   y  ) : 0;
        }
    }

    x = MAP_JIGSAW_PIECES_ACROSS / 2;
    y = MAP_JIGSAW_PIECES_DOWN / 2;

    maze_generate_all_random_directions(dg, MAZE_CELL(dg->maze, x, y), 0);

    if (!maze_solve(dg, w, h)) {
        return (false);
    }

    return (true);
}

static void dungeon_reseed(dungeon_t *dg)
{
    fflush(MY_STDOUT);
    maze_seed = myrand();
    mysrand(maze_seed);

    LOG("Maze: Try new seed");

    memset(dg->maze, 0, sizeof(dg->maze));
}

/*
 * generate_level
 */
static int32_t generate_level (levelp level,
                               const char *jigsaw_map,
                               int32_t opt_seed)
{
    dungeon_t *dg;
    int32_t c;

    char *buf;

    if (opt_seed) {
        maze_seed = opt_seed;
    } else {
        maze_seed = myrand() % 1000;
    }

    mysrand(maze_seed);

    if (level) {
        level->seed = maze_seed;
    }

    LOG("Maze: Seed %u", maze_seed);

    /*
     * Read in the jigsaw pieces.
     */
    buf = filetobuf(jigsaw_map);
    if (!buf) {
        ERR("no buf");
    }

    dg = new_dungeon();

    jigpieces_read(dg, buf);
    myfree(buf);

    for (c = 0; c < dg->jigpieces_cnt; c++) {
        jigpiece_count_char_types(dg, c);
    }

    jigpiece_create_mirrored_pieces(dg);
    jigpiece_create_exits(dg);

    int generating = 1;
    do {
        if (!maze_generate_and_solve(dg)) {
            LOG("Maze: Generate failed, cannot generate:");
            dungeon_reseed(dg);
        } else if (!maze_jigsaw_generate_all_possible_pieces(dg)) {
            LOG("Maze: Generate failed, cannot generate maze pieces:");
            //map_jigsaw_buffer_print_file(MY_STDOUT);
            dungeon_reseed(dg);
        } else if (!maze_jigsaw_solve(dg)) {
            LOG("Maze: Generate failed, cannot solve maze:");
            //map_jigsaw_buffer_print_file(MY_STDOUT);
            dungeon_reseed(dg);
        } else {

            maze_convert_to_map(dg);

            if (!maze_replace_room_char(MAP_START)) {
                LOG("Maze: Generate failed, cannot place start:");
                //map_jigsaw_buffer_print_file(MY_STDOUT);
                dungeon_reseed(dg);
            } else if (!maze_replace_room_char(MAP_END)) {
                LOG("Maze: Generate failed, cannot place exit:");
                //map_jigsaw_buffer_print_file(MY_STDOUT);
                dungeon_reseed(dg);
            } else {
                LOG("Maze: Added start and exit:");
                //map_jigsaw_buffer_print_file(MY_STDOUT);


                if (!maze_check_exit_can_be_reached()) {
                    LOG("Maze: Generate failed, exit cannot be reached:");
                    //map_jigsaw_buffer_print_file(MY_STDOUT);
                    dungeon_reseed(dg);
                } else generating = 0;
            }
        }
    } while (generating);

    /*
     * Check at least 2 teleports.
     */
    maze_check_teleports();

    LOG("Maze: Final maze:");
    //map_jigsaw_buffer_print_file(MY_STDOUT);

#ifdef MAZE_DEBUG_SHOW_CONSOLE
    map_jigsaw_buffer_print();
#endif

    LOG("Maze: Created");

    delete_dungeon(dg);

    return (1);
}

static void init (void)
{
    map_fg[MAP_EMPTY]          = TERM_COLOR_BLACK;
    map_fg[MAP_SPACE]          = TERM_COLOR_WHITE;
    map_fg[MAP_WATER]          = TERM_COLOR_BLACK;
    map_fg[MAP_FLOOR]          = TERM_COLOR_WHITE;
    map_fg[MAP_SHOPKEEPER]     = TERM_COLOR_YELLOW;
    map_fg[MAP_SHOP_FLOOR]     = TERM_COLOR_GREEN;
    map_fg[MAP_ROCK]           = TERM_COLOR_BLUE;
    map_fg[MAP_WALL]           = TERM_COLOR_WHITE;
    map_fg[MAP_CORRIDOR]       = TERM_COLOR_YELLOW;
    map_fg[MAP_CORRIDOR_WALL]  = TERM_COLOR_BLUE;
    map_fg[MAP_MONST]          = TERM_COLOR_BLUE;
    map_fg[MAP_MONST_OR_MOB]   = TERM_COLOR_BLACK;
    map_fg[MAP_TRAP]           = TERM_COLOR_RED;
    map_fg[MAP_TELEPORT]       = TERM_COLOR_BLUE;
    map_fg[MAP_TREASURE]       = TERM_COLOR_WHITE;
    map_fg[MAP_FOOD]           = TERM_COLOR_GREEN;
    map_fg[MAP_LAVA]           = TERM_COLOR_YELLOW;
    map_fg[MAP_ACID]           = TERM_COLOR_GREEN;
    map_fg[MAP_EXIT_WEST]      = TERM_COLOR_BLUE;
    map_fg[MAP_EXIT_EAST]      = TERM_COLOR_BLUE;
    map_fg[MAP_EXIT_SOUTH]     = TERM_COLOR_BLUE;
    map_fg[MAP_EXIT_NORTH]     = TERM_COLOR_BLUE;
    map_fg[MAP_END]            = TERM_COLOR_BLUE;
    map_fg[MAP_START]          = TERM_COLOR_BLUE;
    map_fg[MAP_PADDING]        = TERM_COLOR_WHITE;
    map_fg[MAP_DOOR]           = TERM_COLOR_CYAN;
    map_fg[MAP_WEAPON]         = TERM_COLOR_GREEN;
    map_fg[MAP_GENERATOR]      = TERM_COLOR_CYAN;
    map_fg[MAP_BRAZIER]        = TERM_COLOR_YELLOW;
    map_fg[MAP_POTION]         = TERM_COLOR_CYAN;
    map_fg[MAP_TRIGGER_HERO]   = TERM_COLOR_RED;
    map_fg[MAP_TRIGGER_MONST]  = TERM_COLOR_RED;
    map_fg[MAP_ACTION_UP]      = TERM_COLOR_WHITE;
    map_fg[MAP_ACTION_DOWN]    = TERM_COLOR_WHITE;
    map_fg[MAP_ACTION_LEFT]    = TERM_COLOR_WHITE;
    map_fg[MAP_ACTION_RIGHT]   = TERM_COLOR_WHITE;
    map_fg[MAP_ACTION_SLEEP]   = TERM_COLOR_WHITE;
    map_fg[MAP_ACTION_ZAP]     = TERM_COLOR_WHITE;

    map_bg[MAP_EMPTY]          = TERM_COLOR_BLACK;
    map_bg[MAP_SPACE]          = TERM_COLOR_BLACK;
    map_bg[MAP_WATER]          = TERM_COLOR_CYAN;
    map_bg[MAP_FLOOR]          = TERM_COLOR_BLACK;
    map_bg[MAP_SHOPKEEPER]     = TERM_COLOR_BLACK;
    map_bg[MAP_SHOP_FLOOR]     = TERM_COLOR_BLACK;
    map_bg[MAP_ROCK]           = TERM_COLOR_BLACK;
    map_bg[MAP_WALL]           = TERM_COLOR_BLUE;
    map_bg[MAP_CORRIDOR]       = TERM_COLOR_BLACK;
    map_bg[MAP_CORRIDOR_WALL]  = TERM_COLOR_BLACK;
    map_bg[MAP_MONST]          = TERM_COLOR_BLACK;
    map_bg[MAP_MONST_OR_MOB]   = TERM_COLOR_RED;
    map_bg[MAP_TRAP]           = TERM_COLOR_BLACK;
    map_bg[MAP_TELEPORT]       = TERM_COLOR_BLACK;
    map_bg[MAP_TREASURE]       = TERM_COLOR_YELLOW;
    map_bg[MAP_FOOD]           = TERM_COLOR_BLACK;
    map_bg[MAP_LAVA]           = TERM_COLOR_BLACK;
    map_bg[MAP_ACID]           = TERM_COLOR_BLACK;
    map_bg[MAP_EXIT_WEST]      = TERM_COLOR_BLACK;
    map_bg[MAP_EXIT_EAST]      = TERM_COLOR_BLACK;
    map_bg[MAP_EXIT_SOUTH]     = TERM_COLOR_BLACK;
    map_bg[MAP_EXIT_NORTH]     = TERM_COLOR_BLACK;
    map_bg[MAP_END]            = TERM_COLOR_BLACK;
    map_bg[MAP_START]          = TERM_COLOR_BLACK;
    map_bg[MAP_PADDING]        = TERM_COLOR_BLACK;
    map_bg[MAP_DOOR]           = TERM_COLOR_CYAN;
    map_bg[MAP_BRAZIER]        = TERM_COLOR_YELLOW;
    map_bg[MAP_POTION]         = TERM_COLOR_CYAN;
    map_bg[MAP_TRIGGER_HERO]   = TERM_COLOR_BLACK;
    map_bg[MAP_TRIGGER_MONST]  = TERM_COLOR_BLACK;
    map_bg[MAP_ACTION_UP]      = TERM_COLOR_BLACK;
    map_bg[MAP_ACTION_DOWN]    = TERM_COLOR_BLACK;
    map_bg[MAP_ACTION_LEFT]    = TERM_COLOR_BLACK;
    map_bg[MAP_ACTION_RIGHT]   = TERM_COLOR_BLACK;
    map_bg[MAP_ACTION_SLEEP]   = TERM_COLOR_BLACK;
    map_bg[MAP_ACTION_ZAP]     = TERM_COLOR_BLACK;

    valid_frag_char[MAP_EMPTY]          = true;
    valid_frag_char[MAP_SPACE]          = true;
    valid_frag_char[MAP_WATER]          = true;
    valid_frag_char[MAP_FLOOR]          = true;
    valid_frag_char[MAP_SHOPKEEPER]     = true;
    valid_frag_char[MAP_SHOP_FLOOR]     = true;
    valid_frag_char[MAP_ROCK]           = true;
    valid_frag_char[MAP_WALL]           = true;
    valid_frag_char[MAP_CORRIDOR]       = true;
    valid_frag_char[MAP_CORRIDOR_WALL]  = false;
    valid_frag_char[MAP_MONST]          = true;
    valid_frag_char[MAP_MONST_OR_MOB]      = true;
    valid_frag_char[MAP_TRAP]           = true;
    valid_frag_char[MAP_TELEPORT]       = true;
    valid_frag_char[MAP_TREASURE]       = true;
    valid_frag_char[MAP_FOOD]           = true;
    valid_frag_char[MAP_ACID]           = true;
    valid_frag_char[MAP_EXIT_WEST]      = false;
    valid_frag_char[MAP_EXIT_EAST]      = false;
    valid_frag_char[MAP_EXIT_SOUTH]     = false;
    valid_frag_char[MAP_EXIT_NORTH]     = false;
    valid_frag_char[MAP_END]            = false;
    valid_frag_char[MAP_START]          = false;
    valid_frag_char[MAP_PADDING]        = false;
    valid_frag_char[MAP_DOOR]           = true;
    valid_frag_char[MAP_POTION]         = true;
    valid_frag_char[MAP_WEAPON]         = true;
    valid_frag_char[MAP_GENERATOR]      = true;
    valid_frag_char[MAP_BRAZIER]        = true;

    valid_frag_alt_char[MAP_EMPTY]          = true;
    valid_frag_alt_char[MAP_SPACE]          = false;
    valid_frag_alt_char[MAP_WATER]          = true;
    valid_frag_alt_char[MAP_FLOOR]          = true;
    valid_frag_alt_char[MAP_SHOPKEEPER]     = true;
    valid_frag_alt_char[MAP_SHOP_FLOOR]     = true;
    valid_frag_alt_char[MAP_ROCK]           = true;
    valid_frag_alt_char[MAP_WALL]           = true;
    valid_frag_alt_char[MAP_CORRIDOR]       = true;
    valid_frag_alt_char[MAP_CORRIDOR_WALL]  = true;
    valid_frag_alt_char[MAP_MONST]          = true;
    valid_frag_alt_char[MAP_MONST_OR_MOB]   = true;
    valid_frag_alt_char[MAP_TRAP]           = true;
    valid_frag_alt_char[MAP_TELEPORT]       = true;
    valid_frag_alt_char[MAP_TREASURE]       = true;
    valid_frag_alt_char[MAP_FOOD]           = true;
    valid_frag_alt_char[MAP_LAVA]           = true;
    valid_frag_alt_char[MAP_ACID]           = true;
    valid_frag_alt_char[MAP_EXIT_WEST]      = false;
    valid_frag_alt_char[MAP_EXIT_EAST]      = false;
    valid_frag_alt_char[MAP_EXIT_SOUTH]     = false;
    valid_frag_alt_char[MAP_EXIT_NORTH]     = false;
    valid_frag_alt_char[MAP_END]            = false;
    valid_frag_alt_char[MAP_START]          = false;
    valid_frag_alt_char[MAP_PADDING]        = false;
    valid_frag_alt_char[MAP_DOOR]           = true;
    valid_frag_alt_char[MAP_POTION]         = true;
    valid_frag_alt_char[MAP_WEAPON]         = true;
    valid_frag_alt_char[MAP_GENERATOR]      = true;
    valid_frag_alt_char[MAP_BRAZIER]        = true;

    memset(map_jigsaw_buffer, 0, sizeof(map_jigsaw_buffer));
    memset(map_jigsaw_buffer_old, 0, sizeof(map_jigsaw_buffer_old));
    memset(map_jigsaw_buffer_fg, 0, sizeof(map_jigsaw_buffer_fg));
    memset(map_jigsaw_buffer_bg, 0, sizeof(map_jigsaw_buffer_bg));
    memset(map_jigsaw_buffer_action1, 0, sizeof(map_jigsaw_buffer_action1));
    memset(map_jigsaw_buffer_action2, 0, sizeof(map_jigsaw_buffer_action2));
    memset(map_jigsaw_buffer_action3, 0, sizeof(map_jigsaw_buffer_action3));
    memset(map_jigsaw_buffer_color, 0, sizeof(map_jigsaw_buffer_color));
    memset(map_jigsaw_buffer_solved, 0, sizeof(map_jigsaw_buffer_solved));
}

/*
 * main
 */
int32_t map_jigsaw_test (int32_t argc, char **argv)
{
    const char *jigsaw_map;
    int32_t rc;
    char c;

    init();

    jigsaw_map = "data/map/jigsaw.map";
    opt_seed = 0;

    while ((c = getopt(argc, argv, "f:s:")) != -1) {
        switch (c) {
        case 's':
            opt_seed = atoi(optarg);;
            break;

        default:
            ERR("-f <input file file>, -s <seed>");
            break;
        }
    }

    rc = generate_level(0, jigsaw_map, opt_seed);
    if (!rc) {
        ERR("failed to generate a maze!");
    }

    return (rc);
}

#include "tree.h"
#include "thing_template.h"
#include "wid_game_map_server.h"

static tpp map_char_to_tp (char c,
                           levelp level,
                           int depth,
                           int shop_floor,
                           tpp *wall,
                           tpp *wall2,
                           tpp *door,
                           tpp *floor,
                           tpp *floor2,
                           tpp *floor3,
                           tpp *rock,
                           tpp *gen)
{
    tpp tp = 0;

    switch (c) {
    case MAP_WALL: 
        if (!*wall) {
            *wall = random_wall();
        }
        tp = *wall;
        break;

    case MAP_CORRIDOR_WALL: 
        if (!*wall2) {
            *wall2 = random_corridor_wall();
        }
        tp = *wall2;
        break;

    case MAP_DOOR: 
        if (!*door) {
            *door = random_door();
        }
        tp = *door;
        break;

    case MAP_ROCK: 
        if (!*rock) {
            *rock = random_rock();
        }

        if ((myrand() % 1000) < 5) {
            switch (tp_to_id(*rock)) {
            case THING_ROCK1: tp = id_to_tp(THING_ROCK1a); return (tp);
            case THING_ROCK2: tp = id_to_tp(THING_ROCK2a); return (tp);
            case THING_ROCK3: tp = id_to_tp(THING_ROCK3a); return (tp);
            case THING_ROCK4: tp = id_to_tp(THING_ROCK4a); return (tp);
            case THING_ROCK5: tp = id_to_tp(THING_ROCK5a); return (tp);
            case THING_ROCK6: tp = id_to_tp(THING_ROCK6a); return (tp);
            case THING_ROCK7: tp = id_to_tp(THING_ROCK7a); return (tp);
            }
        }

        if ((myrand() % 1000) < 20) {
            switch (tp_to_id(*rock)) {
            case THING_ROCK1: tp = id_to_tp(THING_CRYSTAL1); return (tp);
            case THING_ROCK2: tp = id_to_tp(THING_CRYSTAL2); return (tp);
            case THING_ROCK3: tp = id_to_tp(THING_CRYSTAL3); return (tp);
            case THING_ROCK4: tp = id_to_tp(THING_CRYSTAL4); return (tp);
            case THING_ROCK5: tp = id_to_tp(THING_CRYSTAL5); return (tp);
            case THING_ROCK6: tp = id_to_tp(THING_CRYSTAL6); return (tp);
            case THING_ROCK7: tp = id_to_tp(THING_CRYSTAL7); return (tp);
            }
        }

        tp = *rock;
        break;

    case MAP_LAVA:
        tp = random_lava();
        break;

    case MAP_TELEPORT:
        tp = tp_find("teleport1");
        break;

    case MAP_SHOP_FLOOR: 
        tp = tp_find("shop_floor1"); 
        if (level) {
            level_set_has_shop(level, true);
        }
        break;

    case MAP_SHOPKEEPER: 
        tp = tp_find("shopkeeper"); 
        break;

    case MAP_START: 
        tp = random_player(); 
        break;

    case MAP_END: 
        tp = random_exit(); 
        break;

    case MAP_FOOD: 
        tp = random_food(); 
        break;

    case MAP_GENERATOR: 
        if (!*gen) {
            *gen = random_mob(depth); 
        }
        tp = *gen;
        break;

    case MAP_MONST_OR_MOB:
        if ((myrand() % 100) < 85) {
            tp = random_monst(depth); 
        } else {
            tp = random_mob(depth); 
        }
        break;

    case MAP_TRAP:
        {
            int r = myrand() % 100;

            if (r < 95) {
                tp = random_trap(depth); 
            } else {
                tp = random_lava();
            }
        }
        break;

    case MAP_WEAPON: 
        tp = random_weapon(shop_floor); 
        break;

    case MAP_POTION: 
        tp = random_potion(shop_floor); 
        break;

    case MAP_MONST: 
        tp = random_monst(depth); 
        break;

    case MAP_BRAZIER: 
        {
            int r = myrand() % 100;

            if (r < 80) {
                tp = tp_find("brazier");
            } else {
                tp = random_treasure(shop_floor);
            }
        }
        break;

    case MAP_TREASURE: {
        int r = myrand() % 100;

        if (shop_floor) {
            tp = random_treasure(shop_floor);
        } else {
            if (r < 20) {
                tp = tp_find("brazier");
            } else if (r < 60) {
                tp = random_food();
            } else {
                tp = random_treasure(shop_floor);
            }
        }
        break;

    case MAP_TRIGGER_HERO:
        tp = tp_find("action_trigger_on_hero");
        break;

    case MAP_TRIGGER_MONST:
        tp = tp_find("action_trigger_on_monst");
        break;

    case MAP_ACTION_LEFT:
        tp = tp_find("action_left");
        break;

    case MAP_ACTION_RIGHT:
        tp = tp_find("action_right");
        break;

    case MAP_ACTION_UP:
        tp = tp_find("action_up");
        break;

    case MAP_ACTION_DOWN:
        tp = tp_find("action_down");
        break;

    case MAP_ACTION_SLEEP:
        tp = tp_find("action_sleep");
        break;

    case MAP_ACTION_ZAP:
        tp = tp_find("action_zap");
        break;
    }

    default:
        break;
    }

    return (tp);
}

/*
 * map_jigsaw_generate
 */
void map_jigsaw_generate (levelp level, widp wid, int depth, grid_wid_replace_t callback)
{
    const char *jigsaw_map;

    static tpp map_tp[MAP_WIDTH][MAP_HEIGHT][MAP_DEPTH_MAX];
    static thing_template_data map_tp_data[MAP_WIDTH][MAP_HEIGHT][MAP_DEPTH_MAX];
    static tpp map_tp_action1[MAP_WIDTH][MAP_HEIGHT];
    static tpp map_tp_action2[MAP_WIDTH][MAP_HEIGHT];
    static tpp map_tp_action3[MAP_WIDTH][MAP_HEIGHT];
    int got_start = 0;
    int got_end = 0;

    memset(map_tp, 0, sizeof(map_tp));
    memset(map_tp_data, 0, sizeof(map_tp_data));
    memset(map_tp_action1, 0, sizeof(map_tp_action1));
    memset(map_tp_action2, 0, sizeof(map_tp_action2));
    memset(map_tp_action3, 0, sizeof(map_tp_action3));

    init();

    jigsaw_map = "data/map/jigsaw.map";
    int rc;
    rc = generate_level(level, jigsaw_map, opt_seed);

    if (!rc) {
        ERR("failed to generate a maze!");
    }

    int32_t x;
    int32_t y;
    int32_t z;

    tpp wall = 0;
    tpp wall2 = 0;
    tpp door = 0;
    tpp floor = 0;
    tpp floor2 = 0;
    tpp floor3 = 0;
    tpp rock = 0;
    tpp gen = 0;

    for (y = 0; y < MAP_HEIGHT; y++) {
        for (x = 0; x < MAP_WIDTH; x++) {
            tpp tp;

            char c = map_jigsaw_buffer[x][y];

            /*
             * Items in shops go on shop floors
             */
            int shop_floor = false;
            if ((x > 0) && (x < MAP_WIDTH - 1) && (y > 0) && (y < MAP_HEIGHT - 1)) {
                shop_floor  = (map_jigsaw_buffer[x][y] == '_') ? 1 : 0;
                shop_floor |= (map_jigsaw_buffer[x-1][y] == '_') ? 1 : 0;
                shop_floor |= (map_jigsaw_buffer[x+1][y] == '_') ? 1 : 0;
                shop_floor |= (map_jigsaw_buffer[x][y-1] == '_') ? 1 : 0;
                shop_floor |= (map_jigsaw_buffer[x][y+1] == '_') ? 1 : 0;
            }

            int corridor_floor = false;
            if ((x > 0) && (x < MAP_WIDTH - 1) && (y > 0) && (y < MAP_HEIGHT - 1)) {
                corridor_floor  = (map_jigsaw_buffer[x][y] == ',') ? 1 : 0;
                corridor_floor |= (map_jigsaw_buffer[x-1][y] == ',') ? 1 : 0;
                corridor_floor |= (map_jigsaw_buffer[x+1][y] == ',') ? 1 : 0;
                corridor_floor |= (map_jigsaw_buffer[x][y-1] == ',') ? 1 : 0;
                corridor_floor |= (map_jigsaw_buffer[x][y+1] == ',') ? 1 : 0;
            }

            int dirt_floor = false;
            if ((x > 0) && (x < MAP_WIDTH - 1) && (y > 0) && (y < MAP_HEIGHT - 1)) {
                dirt_floor  = (map_jigsaw_buffer[x][y] == ' ') ? 1 : 0;
                dirt_floor |= (map_jigsaw_buffer[x-1][y] == ' ') ? 1 : 0;
                dirt_floor |= (map_jigsaw_buffer[x+1][y] == ' ') ? 1 : 0;
                dirt_floor |= (map_jigsaw_buffer[x][y-1] == ' ') ? 1 : 0;
                dirt_floor |= (map_jigsaw_buffer[x][y+1] == ' ') ? 1 : 0;
            }

            int room_floor = false;
            if ((x > 0) && (x < MAP_WIDTH - 1) && (y > 0) && (y < MAP_HEIGHT - 1)) {
                room_floor  = (map_jigsaw_buffer[x][y] == '.') ? 1 : 0;
                room_floor |= (map_jigsaw_buffer[x-1][y] == '.') ? 1 : 0;
                room_floor |= (map_jigsaw_buffer[x+1][y] == '.') ? 1 : 0;
                room_floor |= (map_jigsaw_buffer[x][y-1] == '.') ? 1 : 0;
                room_floor |= (map_jigsaw_buffer[x][y+1] == '.') ? 1 : 0;
            }

            if (!floor) {
                floor = random_floor();
            }

            tp = floor;

            if (shop_floor) {
                tp = tp_find("shop_floor1");
            }

            if ((c == ',') || corridor_floor) {
                if (!floor2) {
                    floor2 = random_corridor();
                }

                tp = floor2;
            }

            if (dirt_floor) {
                if (!floor3) {
                    floor3 = random_dirt();
                }

                tp = floor3;
            }

            map_tp[x][y][tp_get_z_depth(tp)] = tp;

            tp = map_char_to_tp(c, 
                                level,
                                depth,
                                shop_floor,
                                &wall, &wall2, &door, &floor, &floor2, &floor3, &rock, &gen);
            if (!rock) {
                rock = random_rock();
            }

            /*
             * Ensure rock border
             */
            if (x < 4) {
                tp = rock;
            }
            if (x >= MAP_WIDTH - 4) {
                tp = rock;
            }
            if (y < 4) {
                tp = rock;
            }
            if (y >= MAP_HEIGHT - 4) {
                tp = rock;
            }

            if (tp) {
                map_tp[x][y][tp_get_z_depth(tp)] = tp;
            }

            /*
             * Make sure nothing overlaps players
             */
            if (c == MAP_START) {
                got_start = 1;
                continue;
            }

            /*
             * Or exits
             */
            if (c == MAP_END) {
                got_end = 1;
                continue;
            }

            if (tp) {
                switch (c) {
                case MAP_TRIGGER_HERO:
                case MAP_TRIGGER_MONST:
                    {
                        const char *col_name = map_jigsaw_buffer_color[x][y];
                        map_tp_data[x][y][tp_get_z_depth(tp)].col_name = col_name;
                        map_tp_data[x][y][tp_get_z_depth(tp)].col = color_find(col_name);
                        break;
                    } 
                }
            }

            c = map_jigsaw_buffer_action1[x][y];
            if (c) {
                tp = map_char_to_tp(c, 
                                    level,
                                    depth,
                                    shop_floor,
                                    &wall, &wall2, &door, &floor, &floor2, &floor3, &rock, &gen);

                map_tp_action1[x][y] = tp;
            }

            c = map_jigsaw_buffer_action2[x][y];
            if (c) {
                tp = map_char_to_tp(c, 
                                    level,
                                    depth,
                                    shop_floor,
                                    &wall, &wall2, &door, &floor, &floor2, &floor3, &rock, &gen);

                map_tp_action2[x][y] = tp;
            }

            c = map_jigsaw_buffer_action3[x][y];
            if (c) {
                tp = map_char_to_tp(c, 
                                    level,
                                    depth,
                                    shop_floor,
                                    &wall, &wall2, &door, &floor, &floor2, &floor3, &rock, &gen);

                map_tp_action3[x][y] = tp;
            }
        }
    }

    int pass;
    int max_pass = 2;

    for (pass = 0; pass < max_pass; pass++) {
        server_level_is_being_loaded = pass + 1;

        for (y = 0; y < MAP_HEIGHT; y++) {
            for (x = 0; x < MAP_WIDTH; x++) {
                for (z = 0; z < MAP_DEPTH_MAX; z++) {
                    tpp tp = map_tp[x][y][z];
                    if (tp) {
                        tpp_data tp_data = &map_tp_data[x][y][tp_get_z_depth(tp)];

                        (*callback)(wid, x, y, 
                                    0, /* thing */
                                    tp,
                                    tp_data /* tpp_data */,
                                    0 /* item */,
                                    0 /* stats */);
                    }
                }

                {
                    tpp tp;

                    thing_template_data tp_data;

                    memset(&tp_data, 0, sizeof(tp_data));

                    const char *col_name = map_jigsaw_buffer_color[x][y];
                    if (!col_name) {
                        continue;
                    }

                    tp_data.col_name = col_name;
                    tp_data.col = color_find(col_name);

                    tp = map_tp_action1[x][y];
                    if (tp) {
                        (*callback)(wid, x, y, 
                                    0, /* thing */
                                    tp,
                                    &tp_data /* tpp_data */,
                                    0 /* item */,
                                    0 /* stats */);
                    }

                    tp = map_tp_action2[x][y];
                    if (tp) {
//CON("%d %d %s %s",x, y, col_name,tp_name(tp));
                        (*callback)(wid, x, y, 
                                    0, /* thing */
                                    tp,
                                    &tp_data /* tpp_data */,
                                    0 /* item */,
                                    0 /* stats */);
                    }

                    tp = map_tp_action3[x][y];
                    if (tp) {
                        (*callback)(wid, x, y, 
                                    0, /* thing */
                                    tp,
                                    &tp_data /* tpp_data */,
                                    0 /* item */,
                                    0 /* stats */);
                    }
                }
            }
        }
    }

    server_level_is_being_loaded = 0;
    if (!got_start) {
        ERR("I made a level with no start!");
    }

    if (!got_end) {
        ERR("I made a level with no exit!");
    }
}
