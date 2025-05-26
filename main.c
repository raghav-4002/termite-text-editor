/* include feature-test macros */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _GNU_SOURCE


#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>


#define WELCOME_MSG "Termite"


#define CTRL_KEY(k) ((k) & 0x1f)    /* this is equivalent to CTRL + k */

enum editor_key {
    ARROW_UP = 1000,
    ARROW_DOWN = 1001,
    ARROW_LEFT = 1002,
    ARROW_RIGHT = 1003,
};


/* struct to store size and characters of a row of a file while printing content */
typedef struct {
    int size;
    char *chars;
} row;


/* struct to store various terminal attributes */
struct editor_attributes{
    int cx, cy;                  /* cursor position */
    int screenrows, screencols;  /* length and width of the screen */
    int rowoff;                  /* variable to manage scrolling */
    int coloff;
    int numrows;                 /* total number of rows of a file to print */
    row *erow;                   /* pointer to array of 'row' structures */
    struct termios orig_termios; /* stores the original terminal settings */
};

struct editor_attributes attributes;


/*
 * Prints error, restores terminal settings and exits program
 */
void
die(const char *s)
{
    /* clear screen */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    /* reposition cursor to top */
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);  /* prints the error msg */
    exit(1);
}


void
disable_raw_mode(void)
{
    /* restore original terminal attributes */
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &attributes.orig_termios) == -1)
        die("tcsetattr");
}


/*
 * This function disables cannonical terminal mode of input. By default, terminal emulators are designed to only accept
   data if 'enter' key is pressed.
 * To make a text editor, we need to disable this feature so that we can have more finer control over the terminal
 */
void
enable_raw_mode(void)
{
    /* get current terminal attributes */
    if(tcgetattr(STDIN_FILENO, &attributes.orig_termios) == -1) die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = attributes.orig_termios;

    /* enable raw mode */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | CS8);
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);


    /* set the updated terminal attributes */
    if(tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1)
        die("tcsetattr");
}


/*
 * datatype 'abuf' and related functions 'append_buffer' and 'free_buffer'
   are used to call the 'write' syscall inside the function 'refresh_screen'
 
 * requesting system-calls repeatedly is expensive
   thus the function 'append_buffer' will first create a 'list' of
   requests to write and then call 'write' only once, writing all the
   content at once.
*/
struct abuf {
    char *s;
    int len;
};

#define ABUF_INIT {NULL, 0}

void
append_buffer(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->s, ab->len + len);
    if(new == NULL) die("realloc");

    memcpy(&new[ab->len], s, len);

    ab->s = new;
    ab->len = ab->len + len;
}


void
free_buffer(struct abuf *ab)
{
    free(ab->s);
}


void
get_window_size(void)
{
    struct winsize ws;

    /* 'ioctl' (input-output control) syscall; used here to get window size */
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) die("ioctl");

    attributes.screenrows = ws.ws_row;
    attributes.screencols = ws.ws_col;    
}


void
init_editor(void)
{
    attributes.cx = 0, attributes.cy = 0; /* set cursor position to top-left */
    attributes.rowoff = 0;                /* row-offset variable to handle vertical scrolling */
    attributes.numrows = 0;               /* number of rows of a file to print */
    attributes.coloff = 0 ;
    attributes.erow = NULL;               /* pointer to 'row' type structure */

    get_window_size();
}


/*
 * Stores size and characters of a single line of file in array of 'row' type struct.
 * Does so by allocating memory.
*/
void
editor_read_lines(char *line, ssize_t line_len)
{
    attributes.erow = realloc(attributes.erow, sizeof(row) * (attributes.numrows + 1));

    while(line[line_len - 1] == '\n' || line[line_len - 1] == '\r')
        line_len--;

    attributes.erow[attributes.numrows].size = line_len;
    attributes.erow[attributes.numrows].chars = malloc(line_len);

    memcpy(attributes.erow[attributes.numrows].chars, line, line_len);

    attributes.numrows++;
}


/*
 * Reads and stores the content of a file.
*/
void
editor_open(const char *filename)
{
    FILE *fptr = fopen(filename, "r");

    if(!fptr) die(NULL);

    char *line = NULL;  /* stores characters of a line */
    size_t n = 0;       /* stores size of the array 'line' */
    ssize_t line_len;   /* stores length of the line */

    /* getline will return -1 if it encounters error or EOF */
    while((line_len = getline(&line, &n, fptr)) != -1) {
        editor_read_lines(line, line_len);
    }
    /* don't throw error if EOF was reached */
    if(line_len == -1 && (errno == EINVAL || errno == ENOMEM)) die("getline");
}


/*
 * Redraws the whole frame, printing all the lines of a file,
 * on the basis of updated parameters. Also prints the message.
*/
void
draw_rows(struct abuf *ab)
{
    int y;

    for(y = 0; y < attributes.screenrows; y++) {
        /* filerow is the starting row to display */
        int filerow = y + attributes.rowoff;

        if(filerow >= attributes.numrows) {
            if(y == attributes.screenrows / 3 && attributes.numrows == 0) {
                /* if numrows = 0 => no content to print => print message and tildes only */
                char welcome[60];

                int welcome_len = snprintf(welcome, sizeof(welcome),
                                    "%s -- A text editor written in C", WELCOME_MSG);
                
                if(welcome_len > attributes.screencols) {
                    welcome_len = attributes.screencols;
                }

                int padding = (attributes.screencols - welcome_len) / 2;

                if(padding != 0) {
                    append_buffer(ab, "~", 1);
                    padding--;
                }

                while(padding != 0) {
                    append_buffer(ab, " ", 1);
                    padding--;
                }

                append_buffer(ab, welcome, welcome_len);
            } else {
                /* draw tildes */
                append_buffer(ab, "~", 1);
            }
        } else {
            /* write the contents of the file */
            int len = attributes.erow[filerow].size - attributes.coloff;
            if(len > attributes.screencols) len = attributes.screencols;
            if(len < 0) len = 0;
            append_buffer(ab, &attributes.erow[filerow].chars[attributes.coloff], len);
        }

        /* clear screen to right of the cursor */
        append_buffer(ab, "\x1b[K", 3);

        /* add newline */
        if(y < attributes.screenrows - 1) {
            append_buffer(ab, "\r\n", 2);
        }
    }
}


/*
 * Clears the whole screen, calls `draw_rows` function to draw rows.
*/
void
refresh_screen(void)
{
    struct abuf ab = ABUF_INIT;

    /* hide cursor */
    append_buffer(&ab, "\x1b[?25l", 6);
    /* repostion cursor to the top */
    append_buffer(&ab, "\x1b[H", 3);

    draw_rows(&ab);

    /* reposition cursor based on user input */
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", attributes.cy + 1, attributes.cx + 1);
    append_buffer(&ab, buf, strlen(buf));

    /* unhide the cursor */
    append_buffer(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.s, ab.len);

    free(ab.s);
}


int
read_input(void)
{
    char ch[4] = {0};   /* initialized with 0 to solve a particular bug */

    read(STDIN_FILENO, ch, 4);

    /* handle escape key and arrow keys */
    if(ch[0] == '\x1b' && ch[1] == '[') {
        switch(ch[2]) {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
        }
    }

    return ch[0];
}


void
update_cursor_row(row **cursor_row)
{
    *cursor_row = (attributes.cy + attributes.rowoff >= attributes.numrows) ?
                  NULL : &attributes.erow[attributes.cy + attributes.rowoff];
}


void
snap_cursor_at_end(row *cursor_row)
{
    /* firstly adjust the column offset, depending on the size of row */
        attributes.coloff = cursor_row->size > attributes.screencols ?
                            cursor_row->size - attributes.screencols + 1:
                            0;
        attributes.cx = cursor_row->size - attributes.coloff;
}


/*
 * This function moves cursor across the screen.
 * It also has the logic to scroll.
*/
void
move_cursor(int ch)
{
    row *cursor_row = NULL;
    update_cursor_row(&cursor_row);

    switch(ch) {
        case ARROW_UP:
            if(attributes.cy > 0) {
                /* move cursor up only when not at top of screen */
                attributes.cy--;
            }
            else if(attributes.rowoff) {
                /* scroll up only when not at top of file */
                attributes.rowoff--;
            }
            break;

        case ARROW_DOWN:
            if(cursor_row) {
                /*
                 * trigger Arrow down key only if the cursor is at
                 * the row where text is present
                 */

                if(attributes.cy + 1 < attributes.screenrows) {
                    /* move cursor down, only if not at bottom of screen */
                    attributes.cy++;
                }
                else {
                    /* if the cursor is at the bottom of the screen, scroll down */
                    attributes.rowoff++;
                }
            }
            break;

        case ARROW_LEFT:
            if(attributes.cx > 0) {
                /* move cursor left only when not at the edge of the screen */
                attributes.cx--;
            }
            else if(attributes.coloff) {
                /* scroll left only when coloffset is not zero */
                attributes.coloff--;
            }
            else if(attributes.cy + attributes.coloff > 0) {
                /* move to the end of line of previous row */
                attributes.cy--;
                update_cursor_row(&cursor_row);
                snap_cursor_at_end(cursor_row);
            }
            break;

        case ARROW_RIGHT:
            if(cursor_row && attributes.cx + attributes.coloff <= cursor_row->size) {
                /* 
                 * trigger Arrow right key only if current row of cursor has some text 
                 * and cursor is not at the end of the text
                 */

                if(attributes.cx + 1 < attributes.screencols) {
                    /* move cursor right only if not at the right edge of the screen */
                    attributes.cx++;
                }
                else if(attributes.cx + 1 == attributes.screencols) {
                    /* if at the right edge of the screen, just scroll right */
                    attributes.coloff++;
                }
                if(attributes.cx + attributes.coloff == cursor_row->size + 1) {
                    attributes.coloff = 0;
                    attributes.cy++;
                    attributes.cx = 0;
                }
            }
            break;
    }

    /* update the cursor row */
    update_cursor_row(&cursor_row);

    /* snap cursor at the end of the row; only if cursor lands on row with some text */
    if(cursor_row && attributes.cx + attributes.coloff > cursor_row->size) {
        snap_cursor_at_end(cursor_row);
    }

    /* adjustment for empty row */
    if(!cursor_row) {
        attributes.coloff = 0;
        attributes.cx = 0;
    }
}


/*
 * Proccesses the read input via `read_input` function.
*/
void
process_input(void)
{
    int ch;

    if((ch = read_input()) == -1) die("read_input");

    switch(ch) {
        case CTRL_KEY('q'): case '\x1b':
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
            break;

        case ARROW_UP: case ARROW_DOWN: case ARROW_RIGHT: case ARROW_LEFT:
            move_cursor(ch);
            break;
    }

}


int
main(int argc, char *argv[])
{
    enable_raw_mode();
    init_editor();

    /* if a second argument is given */
    if(argc == 2) {
        editor_open(argv[1]);
    }

    while(1) {
        refresh_screen();
        process_input();
    }

    return 0;
}