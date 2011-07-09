CXXFLAGS = -O3 -g -pipe -march=core2 -std=gnu++0x
#CXXFLAGS = -O0 -g -pipe -std=gnu++0x

%.o: %.cpp *.hpp
	g++ $(CXXFLAGS) -c -o $@ $<

chess: chess.o detect_check.o
	g++ $(CXXFLAGS) -o $@ $?


clean:
	rm -f chess
	rm -f *.o
