#include <asm-generic/ioctls.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>


#define CTRL_KEY(k) ((k) & 0x1f)


struct termios orig_termios;
int rows, cols;


void die(const char *s);
void enable_raw_mode(void);
void disable_raw_mode(void);
void get_window_size(void);
void refresh_screen(void);
void draw_tildes(void);
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
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;

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
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}


void
get_window_size(void)
{
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) die("ioctl");

    rows = ws.ws_row;
    cols = ws.ws_col;    
}


void
refresh_screen(void)
{
    /* clear screen */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    /* repostion cursor to the top */
    write(STDOUT_FILENO, "\x1b[H", 3);

    draw_tildes();

    write(STDOUT_FILENO, "\x1b[H", 3);
}


void
draw_tildes(void)
{
    int y;

    for(y = 0; y < rows; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
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