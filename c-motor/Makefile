CC = gcc
CFLAGS = -g -Wall -Wextra
TARGET = exe

LIBS = $(shell mysql_config --libs)
CFLAGS += $(shell mysql_config --cflags)

-include .env

export DB_HOST
export DB_USER
export DB_NAME
export DB_PASS

all: $(TARGET)

$(TARGET): main.c database/db.c database/config.c
	@$(CC) $(CFLAGS) $^ -o $(TARGET) $(LIBS)

run: all
ifndef DB_PASS
	$(error O arquivo .env nao foi configurado ou DB_PASS esta vazio.)
endif
	@./$(TARGET)

clean:
	@rm -f *.o $(TARGET)

.PHONY: all run clean

.PRECIOUS: exe