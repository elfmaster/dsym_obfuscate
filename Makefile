all:
	gcc -N -static -nostdlib constructor.c -o egg
	gcc -g dsymobf.c libelfmaster.a -o dsymobf
	gcc -no-pie test.c -o test
	gcc -no-pie test2.c -o test2 -lpthread
clean:
	rm -f egg
	rm -f test
	rm -f test2
	rm -f dsymobf
