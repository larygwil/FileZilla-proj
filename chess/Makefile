CXXFLAGS = -O3 -g -pipe -march=corei7 -std=gnu++0x -Wall -flto
#CXXFLAGS = -O0 -g -pipe -std=gnu++0x

OBJECT_FILES = \
	calc.o \
	chess.o \
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

chess: $(OBJECT_FILES)
	g++ $(CXXFLAGS) -pthread -o $@ $^


clean:
	rm -f chess
	rm -f *.o
