###########################################################
#
# Makefile: hfsh
#
###########################################################
.SUFFIXES: .h .c .cpp .l .o

CC = gcc
CXX = g++
CFLAGS = -g 
LEX = flex
LIBS = -ll -lpthread
RM = /bin/rm
RMFLAGS = -rf

EXE = hfsh
OBJS = hfsh.o csapp.o lex.yy.o


$(EXE): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $<

.cpp.o:
	$(CXX) $(CFLAGS) -c $<

lex.yy.o: lex.yy.c
	$(CC) $(CFLAGS) -c $<

lex.yy.c: scan.l
	$(LEX) $<

clean:
	$(RM) $(RMFLAGS) *.o *~ hfsh lex.yy.c tests-out

test:
	./test-hfsh.sh

test-all:
	./test-hfsh.sh -c



