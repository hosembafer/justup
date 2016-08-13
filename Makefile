INSTALL = /usr/bin/install -c

CC = gcc 
CPARAMS = -g -o
NAME = justup

CFLAGS = `pkg-config --cflags --libs sqlite3` -lftp -lssl -lcrypto -lssh
SOURCES = src/justup.c
EXECUTABLE = $(NAME)

all: $(NAME)

clean:
	rm -rf $(NAME)

justup:
	rm -rf $(NAME)
	rm -rf /usr/bin/$(NAME)
	
	$(CC) $(CPARAMS) $(EXECUTABLE) $(SOURCES) $(CFLAGS)

install:all
	$(INSTALL) $(NAME) /usr/bin/$(NAME)

uninstall:
	rm -rf /usr/bin/$(NAME)