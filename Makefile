CC = gcc
BITS = 64
OPT = 3
SRC = main.c
BIN = toner
CFLAGS = `pkg-config --cflags gtk4 libadwaita-1 gio-2.0`
LIBS = `pkg-config --libs   gtk4 libadwaita-1 gio-2.0` -lpulse-simple -lpulse -pthread -lm

all:
	$(CC) -m$(BITS) -O$(OPT) $(SRC) -o $(BIN) $(CFLAGS) $(LIBS)

clean:
	rm -f $(BIN) $(FPAKBUILD) $(FPAK)
