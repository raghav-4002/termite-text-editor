#include <termios.h>
#include <unistd.h>
#include <stdlib.h>


struct termios orig_termios;


void enable_raw_mode(void);
void disable_raw_mode(void);


int
main(void)
{
    enable_raw_mode();

    return 0;
}


void
enable_raw_mode(void)
{
    /* get terminal attributes */
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;

    /* enable raw mode */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | CS8);
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);

    /* set new terminal attributes */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


void
disable_raw_mode(void)
{
    /* restore original terminal attributes */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}