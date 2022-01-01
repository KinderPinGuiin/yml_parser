CC = gcc
CFLAGS = -std=c18 \
  -Wall -Wconversion -Werror -Wextra -Wfatal-errors -Wpedantic -Wwrite-strings \
  -O2 -pthread
LDFLAGS = -pthread -lrt
OBJECTS = main.o yaml_parser.o
executable = main

all: $(executable)

clean:
	$(RM) $(OBJECTS)

$(executable): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(executable)

yaml_parser.o: yaml_parser.c
main.o: main.c