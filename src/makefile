
TARGET = libnmbls.so

CXX = g++
CXXFLAGS += -Wall -g -O0 -fPIC
LDFLAGS = -lboost_system -shared -Wl,-soname=$(TARGET).0

RM = rm -f

SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $^ -o $@ ${LDFLAGS} 

clean:
	$(RM) *.o *.so && ldconfig -n ./

install: all
	/bin/cp -ar $(TARGET) /usr/lib && ldconfig
	/bin/mkdir -p /usr/include/nmbls
	/bin/cp nmbls.h /usr/include/nmbls/
	/bin/cp httpdoc.h /usr/include/nmbls/
	/bin/cp httpconnection.h /usr/include/nmbls/
	/bin/cp websocketframe.h /usr/include/nmbls/
	/bin/cp config.h /usr/include/nmbls/
