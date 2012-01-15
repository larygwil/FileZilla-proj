CFLAGS = -O3 -g -pipe -march=corei7 -std=gnu++0x -Wall -Wextra -flto
#CFLAGS = -O0 -g -pipe -std=gnu++0x -Wall
CXXFLAGS = $(CFLAGS) -std=gnu++0x

all: octochess bookgen

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
	fen.o \
	hash.o \
	logger.o \
	mobility.o \
	moves.o \
	moves_captures.o \
	moves_checks.o \
	pawn_structure_hash_table.o \
	pvlist.o \
	see.o \
	selftest.o \
	statistics.o \
	tweak.o \
	unix.o \
	util.o \
	sqlite/sqlite3.o \
	zobrist.o 

%.o: %.cpp *.hpp
	g++ $(CXXFLAGS) -pthread -c -o $@ $<

sqlite/sqlite3.o: sqlite/sqlite3.c sqlite/sqlite3.h
	gcc $(CFLAGS) -pthread -c -o $@ $<

mobility.o: mobility_data.hpp

octochess: $(OBJECT_FILES) chess.o
	g++ $(CXXFLAGS) -pthread -ldl -o $@ $^


bookgen: $(OBJECT_FILES) bookgen.o
	g++ $(CXXFLAGS) -pthread -ldl -o $@ $^

clean:
	rm -f octochess bookgen mobility_gen
	rm -f *.gcda
	rm -f *.o
	rm -f mobility_data.hpp

.PHONY: all clean
