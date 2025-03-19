flags = -Wpedantic -Wall -Wextra -std=c99


main: main.c
	$(CC) main.c $(flags)


test: test.c
	$(CC) test.c $(flags)