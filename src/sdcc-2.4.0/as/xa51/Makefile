CC = gcc
CFLAGS = -ggdb -Wall
YACC = bison -y -d
LEX = flex -i
LEXLIB = 

all: xa_rasm xa_link

OBJECTS = xa_main.o xa_rasm.tab.o xa_rasm.lex.o  

xa_rasm: $(OBJECTS)
	$(CC) -o xa_rasm $(OBJECTS) $(LEXLIB)
	#strip xa_rasm

xa_rasm.lex.o: xa_rasm.lex.c xa_rasm.tab.h xa_main.h
	$(CC) $(CFLAGS) -c xa_rasm.lex.c

xa_rasm.tab.o: xa_rasm.tab.c xa_rasm.tab.h xa_main.h
	$(CC) $(CFLAGS) -c xa_rasm.tab.c

xa_main.o: xa_main.c xa_main.h xa_version.h
	$(CC) $(CFLAGS) -Wall -c xa_main.c

xa_link.o: xa_link.c xa_main.h xa_version.h
	$(CC) $(CFLAGS) -Wall -c xa_link.c

xa_rasm.tab.c xa_rasm.tab.h: xa_rasm.y
	$(YACC) xa_rasm.y
	mv y.tab.c xa_rasm.tab.c
	mv y.tab.h xa_rasm.tab.h

xa_rasm.lex.c: xa_rasm.l
	$(LEX) xa_rasm.l
	mv lex.yy.c xa_rasm.lex.c

all: xa_rasm

clean:
	rm -f *.tab.c *.tab.h *.lex.c *.o *.bak y.output *.hex *.lst *.obj
	rm -f xa_rasm xa_link core *~

