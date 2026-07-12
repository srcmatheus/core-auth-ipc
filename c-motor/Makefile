CC = gcc
CFLAGS = -Wall -Wextra
TARGET = exe

-include .env

export DB_HOST
export DB_USER
export DB_NAME
export DB_PASS

all: $(TARGET)

$(TARGET): main.c database/db.c database/config.c
	@$(CC) $(CFLAGS) $^ -o $(TARGET)

run: all
	@ifndef DB_PASS
		$(error The .env file has not been configured. Copy .env.example and fill in your credentials.)
	@endif
	@./$(TARGET)

clean:
	@rm -f *.o $(TARGET)

.PHONY: all run clean

.PRECIOUS: exe