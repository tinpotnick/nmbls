
INCLUDE = /usr/include

CXX = g++
CXXFLAGS += -Wall -g -O0 -fPIC -I$(INCLUDE)
LDFLAGS = -lboost_system -lnmbls

RM = rm -f

TARGET = hellowworld
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) ${LDFLAGS} $^ -o $@

clean:
	$(RM) *.o hellowworld
