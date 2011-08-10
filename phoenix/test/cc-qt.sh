moc -i -o ../qt/qt.moc ../qt/qt.moc.hpp
g++-4.5 -std=gnu++0x -I.. -g -c ../phoenix.cpp `pkg-config --cflags QtCore QtGui` -DPHOENIX_QT
g++-4.5 -std=gnu++0x -I.. -g -c test.cpp
g++-4.5 -o test-qt test.o phoenix.o `pkg-config --libs QtCore QtGui`
rm *.o
