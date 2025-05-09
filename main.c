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


#define CTRL_KEY(k) ((k) & 0x1f)

enum editor_key {
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT
};

#define WELCOME_MSG "Termite"


typedef struct {
    int size;
    char *chars;
} row;

struct editor_attributes{
    int cx, cy;
    int screenrows, screencols;
    int rowoff;
    int numrows; // stores the total number of rows to print
    row *erow;   // stores the size and characters of a single row to print
    struct termios orig_termios; 
};

struct editor_attributes attributes;


struct abuf {
    char *s;
    int len;
};

#define ABUF_INIT {NULL, 0}


void init_editor(void);
void get_window_size(void);
void editor_open(const char *filename);
void editor_read_lines(char *line, ssize_t line_len);
void refresh_screen(void);
void editor_scroll(void);
void draw_rows(struct abuf *ab);
void process_input(void);
int read_input(void);
void move_cursor(int ch);
void append_buffer(struct abuf *ab, const char *s, int len);
void free_buffer(struct abuf *ab);
void enable_raw_mode(void);
void disable_raw_mode(void);
void die(const char *s);


int
main(int argc, char *argv[])
{
    enable_raw_mode();
    init_editor();
    if(argc == 2) {
        editor_open(argv[1]);
    }

    while(1) {
        refresh_screen();
        process_input();
    }

    return 0;
}


void
init_editor(void)
{
    attributes.cx = 0, attributes.cy = 0; // set cursor position to top left
    attributes.rowoff = 0;
    attributes.numrows = 0;
    attributes.erow = NULL;

    get_window_size();
}


void
get_window_size(void)
{
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) die("ioctl");

    attributes.screenrows = ws.ws_row;
    attributes.screencols = ws.ws_col;    
}


void
editor_open(const char *filename)
{
    FILE *fptr = fopen(filename, "r");    

    char *line = NULL;
    size_t n = 0;
    ssize_t line_len;

    while((line_len = getline(&line, &n, fptr)) != -1) {
        editor_read_lines(line, line_len);
    }
    if(line_len == -1 && (errno == EINVAL || errno == ENOMEM)) die("getline");
}


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


void
refresh_screen(void)
{
    editor_scroll();

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


void
editor_scroll(void)
{
    if(attributes.cy == 0 && attributes.rowoff != 0) {
        attributes.rowoff--;
    }

    if(attributes.cy == attributes.screenrows -1 && attributes.screenrows + attributes.rowoff < attributes.numrows) {
        attributes.rowoff++;
    }
}


void
draw_rows(struct abuf *ab)
{
    int y;

    for(y = 0; y < attributes.screenrows; y++) {
        int filerow = y + attributes.rowoff;
        if(filerow >= attributes.numrows) {
            /* this if-block centres the welcome message to be printed */
            if(y == attributes.screenrows / 3 && attributes.numrows == 0) {
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
                append_buffer(ab, "~", 1);
            }
        } else {
            int len = attributes.erow[filerow].size;
            if(len > attributes.screencols) len = attributes.screencols;
            append_buffer(ab, attributes.erow[filerow].chars, len);
        }

        /* clear screen to right of the cursor */
        append_buffer(ab, "\x1b[K", 3);

        /* add newline */
        if(y < attributes.screenrows - 1) {
            append_buffer(ab, "\r\n", 2);
        }
    }
}


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
move_cursor(int ch)
{
    switch(ch) {
        case ARROW_UP:
            if(attributes.cy != 0) {
                attributes.cy--;
            }
            break;

        case ARROW_DOWN:
            if(attributes.cy != attributes.screenrows - 1) {
                attributes.cy++;
            }
            break;

        case ARROW_LEFT:
            if(attributes.cx != 0) {
                attributes.cx--;
            }
            break;

        case ARROW_RIGHT:
            if(attributes.cx != attributes.screencols - 1) {
                attributes.cx++;
            }
            break;
    }
}


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


void
disable_raw_mode(void)
{
    /* restore original terminal attributes */
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &attributes.orig_termios) == -1)
        die("tcsetattr");
}


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