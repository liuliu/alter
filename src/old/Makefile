LINKLIB=
LINKFLAG=-ldl -lpthread -lrt -lapr-1 -laprutil-1
CXXFLAGS=-I"/opt/apr/include" -I"/opt/apr-util/include"

all: main trigger view server simple-trigger

main: main.o cluscom.o lf_threadpool.o frl_slab_pool.o frl_hash.o frl_list.o server_event.o socket_pipe.o crc32.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG) -O3

trigger: trigger.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG) -O3

view: view.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG) -O3

server: server.o cluscom-daemon.o frl_slab_pool.o server_event.o socket_pipe.o crc32.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG) -O3
	
simple-trigger: simple-trigger.o cluscom-interface.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG) -O3

.c.o:
	g++ $(CXXFLAGS) $< -o $@ -g -c -O3
