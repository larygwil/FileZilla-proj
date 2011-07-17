CXXFLAGS = -O3 -g -pipe -march=corei7 -std=gnu++0x -Wall -flto
#CXXFLAGS = -O0 -g -pipe -std=gnu++0x

%.o: %.cpp *.hpp
	g++ $(CXXFLAGS) -pthread -c -o $@ $<

chess: chess.o detect_check.o calc.o util.o statistics.o unix.o eval.o hash.o zobrist.o
	g++ $(CXXFLAGS) -pthread -o $@ $^


clean:
	rm -f chess
	rm -f *.o
