CC = gcc

all: process_generator.out clk.out scheduler.out process.out

process_generator.out: process_generator.o clk_functions.o
	$(CC) process_generator.o clk_functions.o -o process_generator.out

clk.out: clk.o
	$(CC) clk.o -o clk.out

scheduler.out: scheduler.o DataStructures.o clk_functions.o circQ.o RR.o MMU.o
	$(CC) scheduler.o DataStructures.o clk_functions.o circQ.o RR.o MMU.o -o scheduler.out -lm

process.out: process.o clk_functions.o
	$(CC) process.o clk_functions.o -o process.out

%.o: %.c
	$(CC) -c $< -o $@

clean:
	rm -f *.out *.o

run:
	./process_generator.out processes.txt