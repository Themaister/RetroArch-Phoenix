g++-4.5 -std=gnu++0x -I.. -O3 -fomit-frame-pointer -c ../phoenix.cpp -DPHOENIX_REFERENCE
g++-4.5 -std=gnu++0x -I.. -O3 -fomit-frame-pointer -c test.cpp
g++-4.5 -s -o test-reference test.o phoenix.o
rm *.o
