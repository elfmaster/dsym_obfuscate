all:
	gcc -N -static -nostdlib constructor.c -o egg
	gcc -g dsymobf.c /opt/elfmaster/lib/libelfmaster.a -o dsymobf
	gcc -no-pie test.c -o test
clean:
	rm -f egg
	rm -f test
	rm -f dsymobf
