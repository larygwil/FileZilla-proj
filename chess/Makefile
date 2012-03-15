CFLAGS = -O3 -g -pipe -march=corei7 -std=gnu++0x -Wall -Wextra -flto
#CFLAGS = -O0 -g -pipe -std=gnu++0x -Wall
CXXFLAGS = $(CFLAGS) -std=gnu++0x

all: octochess bookgen

tables.cpp: tables_gen.cpp
	g++ tables_gen.cpp -o tables_gen
	./tables_gen > tables.cpp
	rm tables_gen

OBJECT_FILES = \
	book.o \
	calc.o \
	config.o \
	detect_check.o \
	eval.o \
	fen.o \
	hash.o \
	logger.o \
	magic.o \
	mobility.o \
	moves.o \
	moves_captures.o \
	moves_noncaptures.o \
	pawn_structure_hash_table.o \
	phased_move_generator.o \
	pgn.o \
	pvlist.o \
	random.o \
	see.o \
	seen_positions.o \
	selftest.o \
	sqlite/sqlite3.o \
	statistics.o \
	string.o \
	tables.o \
	tweak.o \
	unix.o \
	util.o \
	zobrist.o

CHESS_FILES = $(OBJECT_FILES) \
	chess.o \
	xboard.o \
	uci/info.o \
	uci/minimalistic_uci_protocol.o \
	uci/octochess_impl.o \
	uci/runner.o \
	uci/time.o


%.o: %.cpp *.hpp
	g++ $(CXXFLAGS) -c -o $@ $<

sqlite/sqlite3.o: sqlite/sqlite3.c sqlite/sqlite3.h
	gcc $(CFLAGS) -pthread -c -o $@ $<


octochess: $(CHESS_FILES)
	g++ $(CXXFLAGS) -pthread -ldl -o $@ $^


bookgen: $(OBJECT_FILES) bookgen.o
	g++ $(CXXFLAGS) -pthread -ldl -o $@ $^

clean:
	rm -f octochess bookgen tables_gen
	rm -f *.gcda
	rm -f *.o
	rm -f uci/*.o
	rm -f uci/*.gcda
	rm -f tables.cpp

.PHONY: all clean
