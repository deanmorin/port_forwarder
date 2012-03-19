os := $(shell uname)
server = server
client = client
forwarder = forwarder
compiler = g++
flags = -W -Wall -pedantic
dflags = -g -DDEBUG -DUSE_DEBUG
lib = -lboost_program_options-mt -lpthread
cmp = $(compiler) $(flags) $(inc) -c
lnk = $(compiler) $(flags) $(lib) -o $(bin)
cobj = client.o network.o
sobj = server.o eventbase.o network.o tpool.o
fobj = forwarder.o eventbase.o network.o tpool.o forwardinginfo.o

ifeq ($(os), Darwin)
    flags += -j8
endif

all : $(server) $(client) $(forwarder)

debug : flags += $(dflags)
debug : $(server) $(client)

$(client) : bin = $(client)
$(client) : $(cobj)
	$(lnk) $(cobj) 

client.o : client.cpp network.hpp
	$(cmp) client.cpp
	
network.o : network.cpp network.hpp
	$(cmp) network.cpp

tpool.o : tpool.c tpool.h
	$(cmp) tpool.c

$(server) : bin = $(server)
$(server) : lib += -levent -levent_pthreads
$(server) : $(sobj)
	$(lnk) $(sobj)

server.o : server.cpp
	$(cmp) server.cpp

eventbase.o : eventbase.cpp eventbase.hpp network.hpp
	$(cmp) eventbase.cpp

$(forwarder) : bin = $(forwarder)
$(forwarder) : lib += -levent -levent_pthreads
$(forwarder) : $(fobj)
	$(lnk) $(fobj)

forwardinginfo.o : forwardinginfo.cpp forwardinginfo.hpp
	$(cmp) forwardinginfo.cpp

clean :
	rm $(server) $(client) $(forwarder) *.o
