TARGET = ssnes-phoenix

OBJ = phoenix/phoenix.o ssnes-phoenix.o

QT_CFLAGS = $(shell pkg-config --cflags QtCore QtGui)
QT_LIBS = $(shell pkg-config --libs QtCore QtGui)

INCLUDES = -Iphoenix

all: $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -std=gnu++0x -c -o $@ $<

phoenix/phoenix.o: phoenix/phoenix.cpp phoenix/qt/qt.moc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -std=gnu++0x -DPHOENIX_QT $(QT_CFLAGS) -c -o $@ $<

%.moc: %.moc.hpp
	moc -i -o $@ $<


$(TARGET): $(OBJ)
	$(CXX) -o $@ $(OBJ) $(QT_LIBS)

clean:
	rm -f *.o
	rm $(TARGET)

.PHONY: clean
