CXXFLAGS = -O3 -g -pipe -march=core2 -std=gnu++0x -Wall -flto
#CXXFLAGS = -O0 -g -pipe -std=gnu++0x

%.o: %.cpp *.hpp
	g++ $(CXXFLAGS) -c -o $@ $<

chess: chess.o detect_check.o calc.o util.o statistics.o unix.o eval.o hash.o
	g++ $(CXXFLAGS) -o $@ $^


clean:
	rm -f chess
	rm -f *.o
