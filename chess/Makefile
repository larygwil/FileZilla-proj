CFLAGS = -O3 -g -pipe -march=corei7 -Wall -Wextra -flto -static
#CFLAGS = -O0 -g -pipe -Wall -static
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
	endgame.o \
	eval.o \
	eval_values.o \
	fen.o \
	hash.o \
	logger.o \
	magic.o \
	moves.o \
	moves_captures.o \
	moves_noncaptures.o \
	pawn_structure_hash_table.o \
	phased_move_generator.o \
	pgn.o \
	pvlist.o \
	random.o \
	score.o \
	see.o \
	seen_positions.o \
	selftest.o \
	sqlite/sqlite3.o \
	statistics.o \
	string.o \
	tables.o \
	time.o \
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
	uci/time_calculation.o


%.o: %.cpp *.hpp
	g++ $(CXXFLAGS) -c -o $@ $<

sqlite/sqlite3.o: sqlite/sqlite3.c sqlite/sqlite3.h
	gcc $(CFLAGS) -DSQLITE_OMIT_LOAD_EXTENSION -pthread -c -o $@ $<


octochess: $(CHESS_FILES)
	g++ $(CXXFLAGS) -pthread -o $@ $^


bookgen: $(OBJECT_FILES) bookgen.o
	g++ $(CXXFLAGS) -pthread -o $@ $^

clean:
	rm -f octochess bookgen tables_gen
	rm -f *.gcda
	rm -f *.o
	rm -f uci/*.o
	rm -f uci/*.gcda
	rm -f tables.cpp

.PHONY: all clean
