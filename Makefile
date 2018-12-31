all:
	gcc -N -static -nostdlib constructor.c -o egg
	gcc -g dsymobf.c libelfmaster.a -o dsymobf
	gcc -no-pie test.c -o test
clean:
	rm -f constructor.o
	rm -f test
