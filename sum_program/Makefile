cxx = g++
#flags = -g -static
flags = -g3

sum: main.o args.o summariser.o
	$(cxx) $(flags) main.o args.o summariser.o -o sum

main.o: main.cpp args.h summariser.h
	$(cxx) $(flags) -c main.cpp

args.o: args.cpp args.h
	$(cxx) $(flags) -c args.cpp

summariser.o: summariser.cpp summariser.h
	$(cxx) $(flags) -c summariser.cpp

clean:
	rm main.o args.o summariser.o
