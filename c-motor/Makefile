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

$(TARGET): tests/main_test.c database/db.c database/config.c database/db_utils.c tests/stress_test.c
	@$(CC) $(CFLAGS) $^ -o $(TARGET) $(LIBS)

run: all
ifndef DB_PASS
	$(error The .env file has not been configured. Copy .env.example and fill in your credentials.)
endif
	@./$(TARGET)

clean:
	@rm -f *.o $(TARGET)

.PHONY: all run clean

.PRECIOUS: exe