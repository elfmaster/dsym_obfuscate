all:
	gcc -c constructor.c
	gcc dsymobf.c libelfmaster.a -o dsymobf
clean:
	rm -f constructor.o
