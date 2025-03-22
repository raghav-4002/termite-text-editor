#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>


#define CTRL_KEY(k) ((k) & 0x1f)


struct term_attributes {
    struct termios orig_termios;
    int rows, cols;
};

struct term_attributes attributes;


struct abuf {
    char *s;
    int len;
};

#define ABUF_INIT {NULL, 0}


void die(const char *s);
void enable_raw_mode(void);
void disable_raw_mode(void);
void get_window_size(void);
void write_buffer(struct abuf *ab, const char *s, int len);
void free_buffer(struct abuf *ab);
void refresh_screen(void);
void draw_tildes(struct abuf *ab);
void process_input(void);
char read_input(void);


int
main(void)
{
    enable_raw_mode();
    get_window_size();

    while(1) {
        refresh_screen();
        process_input();
    }

    return 0;
}


void
die(const char *s)
{
    /* clear screen */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    /* reposition cursor to top */
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}


void
enable_raw_mode(void)
{
    /* get terminal attributes */
    if(tcgetattr(STDIN_FILENO, &attributes.orig_termios) == -1) die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = attributes.orig_termios;

    /* enable raw mode */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | CS8);
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);

    /* set new terminal attributes */
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
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
get_window_size(void)
{
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) die("ioctl");

    attributes.rows = ws.ws_row;
    attributes.cols = ws.ws_col;    
}


void
write_buffer(struct abuf *ab, const char *s, int len)
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
refresh_screen(void)
{
    struct abuf ab = ABUF_INIT;

    /* hide cursor */
    write_buffer(&ab, "\x1b[?25l", 6);
    /* repostion cursor to the top */
    write_buffer(&ab, "\x1b[H", 3);

    draw_tildes(&ab);

    write_buffer(&ab, "\x1b[H", 3);
    /* unhide the cursor */
    write_buffer(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.s, ab.len);

    free(ab.s);
}


void
draw_tildes(struct abuf *ab)
{
    int y;

    for(y = 0; y < attributes.rows; y++) {
        write_buffer(ab, "~", 1);

        /* clear line */
        write_buffer(ab, "\x1b[K", 3);

        if(y < attributes.rows - 1) {
            write_buffer(ab, "\r\n", 2);
        }
    }
}


void
process_input(void)
{
    char ch;

    if((ch = read_input()) == -1) die("read_input");

    switch(ch) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
            break;
    }

}


char
read_input(void)
{
    char ch;

    /* read 1 byte from standard input file descriptor */
    if(read(STDIN_FILENO, &ch, 1) == -1) {
        return -1;
    }

    return ch;
}