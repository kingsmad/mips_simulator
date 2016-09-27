OBJS = disassembler.o bin2buf.o main.o
CC = g++
DEBUG = -g
CFLAGS = -Wall -c $(DEBUG) -std=c++11
LFLAGS = -Wall $(DEBUG)

MIPSsim : $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o MIPSsim 

disassembler.o : disassembler.h disassembler.cpp bin2buf.h bin2buf.cpp
	$(CC) $(CFLAGS) disassembler.cpp

bin2buf.o : disassembler.h disassembler.cpp bin2buf.h bin2buf.cpp
	$(CC) $(CFLAGS) bin2buf.cpp

main.o: disassembler.o bin2buf.o main.cpp
	$(CC) $(CFLAGS) main.cpp

clean:
	    \rm *.o MIPSsim

tar:
	    tar cfv MIPSsim.tar *.h *.cpp *.o MIPSsim
