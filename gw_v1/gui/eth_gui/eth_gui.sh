gcc `pkg-config --cflags gtk+-3.0` eth_gui.c -o eth_gui `pkg-config --libs gtk+-3.0` -ljson-c
./eth_gui
