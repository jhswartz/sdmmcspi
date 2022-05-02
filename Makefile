CFLAGS += -std=c99 -pedantic -Wall

all: sdmmcspi.c
	$(CC) -o sdmmcspi sdmmcspi.c $(CFLAGS)
