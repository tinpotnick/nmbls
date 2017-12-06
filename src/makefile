
TARGET = libnmbls.so

CXX = g++
CXXFLAGS += -Wall -g -O0 -fPIC
LDFLAGS = -lboost_system -shared -Wl,-soname=$(TARGET).0

RM = rm -f

SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) ${LDFLAGS} $^ -o $@

clean:
	$(RM) *.o *.so && ldconfig -n ./

install: all
	/bin/cp -ar $(TARGET) /usr/lib && ldconfig
