
CCOPTS:=-Wall -g -O0 -std=c++11
LL:= -lboost_system -lboost_thread -lboost_filesystem -ldl -lcrypto -lssl
CC:=g++

OBJDIR := output
OBJS := $(addprefix $(OBJDIR)/,base64.o http_connection.o utils.o weblibs.o nmbls.o http_server.o https_server.o)

# we require boost installing to build against (i.e. arch linux pacman -S boost)
default: output/nmbls

$(OBJDIR)/nmbls: $(OBJS)
	$(MAKE) -C mods
	g++ $(OBJS) -o $(OBJDIR)/nmbls $(LL)

clean:
	- rm $(OBJS) $(OBJDIR)/nmbls
	$(MAKE) -C mods clean


$(OBJDIR)/%.o : %.cpp
	$(CC) -c $(CCOPTS) -o $@ -c $<

all: $(OBJS)

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir $(OBJDIR)
