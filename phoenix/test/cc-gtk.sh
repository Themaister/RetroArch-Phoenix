g++-4.5 -std=gnu++0x -I.. -g -c ../phoenix.cpp -DPHOENIX_GTK `pkg-config --cflags gtk+-2.0`
g++-4.5 -std=gnu++0x -I.. -g -c test.cpp
g++-4.5 -o test-gtk test.o phoenix.o `pkg-config --libs gtk+-2.0` -lX11
rm *.o
