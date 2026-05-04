all: clean build

build:
	gcc process_generator.c clk_functions.c -o process_generator.out
	gcc clk.c -o clk.out
	gcc scheduler.c DataStructures.c clk_functions.c circQ.c RR.c MMU.c -o scheduler.out -lm
	gcc process.c clk_functions.c -o process.out

clean:
	rm -f *.out

run:
	./process_generator.out 