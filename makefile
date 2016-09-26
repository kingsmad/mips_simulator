OBJS = disassembler.o bin2buf.o main.o
CC = g++
DEBUG = -g
CFLAGS = -Wall -c $(DEBUG) -std=c++11
LFLAGS = -Wall $(DEBUG)

sim : $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o sim 

disassembler.o : disassembler.h disassembler.cpp bin2buf.h bin2buf.cpp
	$(CC) $(CFLAGS) disassembler.cpp

bin2buf.o : disassembler.h disassembler.cpp bin2buf.h bin2buf.cpp
	$(CC) $(CFLAGS) bin2buf.cpp

main.o: disassembler.o bin2buf.o main.cpp
	$(CC) $(CFLAGS) main.cpp

clean:
	    \rm *.o sim

tar:
	    tar cfv sim.tar *.h *.cpp *.o sim
