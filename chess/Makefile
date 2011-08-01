CXXFLAGS = -O3 -g -pipe -march=corei7 -std=gnu++0x -Wall -flto
#CXXFLAGS = -O0 -g -pipe -std=gnu++0x

OBJECT_FILES = \
	book.o \
	calc.o \
	detect_check.o \
	eval.o \
	hash.o \
	moves.o \
	statistics.o \
	unix.o \
	util.o \
	zobrist.o 

%.o: %.cpp *.hpp
	g++ $(CXXFLAGS) -pthread -c -o $@ $<

chess: $(OBJECT_FILES) chess.o
	g++ $(CXXFLAGS) -pthread -o $@ $^


bookgen: $(OBJECT_FILES) bookgen.o
	g++ $(CXXFLAGS) -pthread -o $@ $^

clean:
	rm -f chess
	rm -f *.o

all: chess bookgen

.PHONY: all clean
