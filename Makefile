flags = -Wpedantic -Wall -Wextra -std=c99 -Werror


main: main.c
	$(CC) main.c $(flags)


test: test.c
	$(CC) test.c $(flags)
