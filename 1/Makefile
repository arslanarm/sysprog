GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant -g

all: libcoro.c solution.c
	gcc $(GCC_FLAGS) libcoro.c solution.c ../utils/heap_help/heap_help.c

clean:
	rm a.out


run:
	./a.out 100000 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
	python checker.py -f output