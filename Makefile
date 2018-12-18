all:
	gcc -c constructor.c
	gcc -g dsymobf.c libelfmaster.a -o dsymobf
	gcc -no-pie test.c -o test
clean:
	rm -f constructor.o
