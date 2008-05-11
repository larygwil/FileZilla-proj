CC=gcc
CXX=g++

CXXFLAGS="-pthread"

%.o: %.c
	$(CC) -O0 -g -c -o $@ $*

%.o: %.cpp
	$(CXX) -O0 -g $(CXXFLAGS) -c -o $@ $+

fzprobe: probe.o instance.o log.o permissions.o
	$(CXX) $(CXXFLAGS) -o $@ $+

all: fzprobe

clean:
	rm -f *.o
	rm -f fzprobe
