CC = gcc
BITS = 64
OPT = 3
SRC = main.c
BIN = toner
CFLAGS = `pkg-config --cflags gtk4 libadwaita-1 gio-2.0`
LIBS = `pkg-config --libs   gtk4 libadwaita-1 gio-2.0` -lpulse-simple -lpulse -pthread -lm
FPAKNM = com.bluMATRIKZ.Toner
FPAKBLD = build
FPAK = $(FPAKNM).flatpak

# Needs: all flatpak-builder
all:
	flatpak-builder --force-clean --install-deps-from=flathub --repo=repo build $(FPAKNM).yaml
	flatpak build-bundle repo $(FPAK) $(FPAKNM)
	rm -rf $(FPAKBLD)

# Needs: libgtk-4-dev libadwaita-1-dev libpulse-dev gcc make git build-essential flatpak-builder
elf:
	$(CC) -m$(BITS) -O$(OPT) $(SRC) -o $(BIN) $(CFLAGS) $(LIBS)
	strip -s $(BIN)

clean:
	rm -f $(BIN) $(FPAKBUILD) $(FPAK)
