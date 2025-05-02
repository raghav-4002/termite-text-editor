flags = -Wpedantic -Wall -Wextra 


main: main.c
	$(CC) main.c $(flags)


test: test.c
	$(CC) test.c $(flags)
