flags = -Wpedantic -Wall -Wextra -Werror


main: main.c
	$(CC) main.c $(flags)


test: test.c
	$(CC) test.c $(flags)
