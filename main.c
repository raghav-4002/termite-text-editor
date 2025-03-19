#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#define CTRL_KEY(k) ((k) & 0x1f)


struct termios orig_termios;


void die(const char *s);
void enable_raw_mode(void);
void disable_raw_mode(void);
void process_input(void);
char read_input(void);


int
main(void)
{
    enable_raw_mode();

    while(1) {
        process_input();
    }

    return 0;
}


void
die(const char *s)
{
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
process_input(void)
{
    char ch;

    if((ch = read_input()) == -1) die("read_input");

    if(ch == CTRL_KEY('q')) exit(0);

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