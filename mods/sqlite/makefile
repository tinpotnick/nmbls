
CCOPTS:=-Wall -g -O0 -fPIC -std=c++11
LL:= -shared -lsqlite3


default: ../../output/libsqlite.so

../../output/libsqlite.so: ../../output/sqlite.o
	g++ ../../output/sqlite.o -o ../../output/libsqlite.so $(LL)

clean:
	- rm ../../output/sqlite.o ../../output/libsqlite.so

../../output/sqlite.o: sqlite.cpp sqlite.h
	g++ -c $(CCOPTS) sqlite.cpp -o ../../output/sqlite.o $(INCLUDE) $(DEFINE)
