CXXFLAGS = -O3 -g -pipe -march=corei7 -std=gnu++0x -Wall -flto
#CXXFLAGS = -O0 -g -pipe -std=gnu++0x

all: chess bookgen

mobility_data.hpp : mobility_gen.cpp
	g++ mobility_gen.cpp -o mobility_gen
	./mobility_gen > mobility_data.hpp
	rm mobility_gen

OBJECT_FILES = \
	book.o \
	calc.o \
	config.o \
	detect_check.o \
	eval.o \
	hash.o \
	mobility.o \
	moves.o \
	moves_captures_and_checks.o \
	pawn_structure_hash_table.o \
	pvlist.o \
	statistics.o \
	unix.o \
	util.o \
	zobrist.o 

%.o: %.cpp *.hpp
	g++ $(CXXFLAGS) -pthread -c -o $@ $<

mobility.o: mobility_data.hpp

chess: $(OBJECT_FILES) chess.o
	g++ $(CXXFLAGS) -pthread -o $@ $^


bookgen: $(OBJECT_FILES) bookgen.o
	g++ $(CXXFLAGS) -pthread -o $@ $^

clean:
	rm -f chess bookgen mobility_gen
	rm -f *.gcda
	rm -f *.o
	rm -f mobility_data.hpp

.PHONY: all clean
