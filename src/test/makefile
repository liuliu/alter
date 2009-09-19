COMPFLAG=-O2
LINKFLAG=-ldl -lpthread -lrt
LINKLIB=-lapr-1 -laprutil-1
CXXFLAGS=-I"/opt/apr/include" -I"/opt/apr-util/include"

all: slab_test list_test radix_tree_test queue_test thread_test socket_pipe_test http_test

slab_test: slab_test.o ../lib/frl_slab_pool.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG)

list_test: list_test.o ../lib/frl_list.o ../lib/frl_slab_pool.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG)

radix_tree_test: radix_tree_test.o ../lib/frl_radix_tree.o ../lib/frl_slab_pool.o ../lib/frl_util_md5.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG)

queue_test: queue_test.o ../lib/frl_queue.o ../lib/frl_slab_pool.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG)

thread_test: thread_test.o ../lib/frl_util_threads.o ../lib/frl_queue.o ../lib/frl_slab_pool.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG)

socket_pipe_test: socket_pipe_test.o ../lib/frl_util_socket_pipe.o ../lib/frl_queue.o ../lib/frl_slab_pool.o ../lib/frl_hash_func.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG)

http_test: http_test.o ../lib/frl_util_server_event.o ../lib/frl_slab_pool.o
	g++ $(CXXFLAGS) $(LINKLIB) -o $@ $^ $(LINKFLAG)

clean:
	rm *.o ../lib/*.o

.c.o:
	g++ $(CXXFLAGS) $< -o $@ -g -c $(COMPFLAG)
