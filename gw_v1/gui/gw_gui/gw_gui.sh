gcc `pkg-config --cflags gtk+-3.0` -o gw_gui gw_gui.c `pkg-config --libs gtk+-3.0` -ljson-c
./gw_gui
