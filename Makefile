CC = gcc
CFLAGS = -Wall -g
OBJ = main.o parser.o executor.o jobs.o

tinyshell: $(OBJ)
	$(CC) $(CFLAGS) -o tinyshell $(OBJ)

main.o: main.c tinyshell.h parser.h executor.h jobs.h
	$(CC) $(CFLAGS) -c main.c

parser.o: parser.c parser.h tinyshell.h
	$(CC) $(CFLAGS) -c parser.c

executor.o: executor.c executor.h tinyshell.h jobs.h
	$(CC) $(CFLAGS) -c executor.c

jobs.o: jobs.c jobs.h tinyshell.h
	$(CC) $(CFLAGS) -c jobs.c

clean:
	rm -f *.o tinyshell