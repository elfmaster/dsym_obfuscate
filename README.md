# dsym_obfuscate

Obfuscates dynamic symbol table by removing .dynstr, and replacing it at runtime.
Currently only works on binaries linked only to libc.so (More work to be accomplished)
Bypasses symbol versioning by marking the DT_VERNEED d_tag as a DT_DEBUG, and sets the
DT_VERNEEDNUM d_val to 0.

Currently only works on non PIE executable's. This can easily be fixed.

# Build

git clone https://github.com/elfmaster/libelfmaster
cd libelfmaster
make
sudo make install
cd ~/dsym_obfuscate
make

# Test

elfmaster@dreamcity:~/git/dsym_obfuscate$ nm -D test
                 U fopen
                 w __gmon_start__
                 U __libc_start_main
                 U pause
                 U puts
                 U strtok
elfmaster@dreamcity:~/git/dsym_obfuscate$ ./dsymobf ./test
backing up dynstr
Loaded basename: libc.so.6
Copying over: libc.so.6 to index 1
Injecting constructor.o into ./test
Commiting changes to ./test
elfmaster@dreamcity:~/git/dsym_obfuscate$ nm -D test
                 U 
                 U 
                 U 
                 U 
                 w __gmon_start__
                 U __libc_start_main
elfmaster@dreamcity:~/git/dsym_obfuscate$ ./test
Hi
^C

# Further work

Get this working on PIE binaries, and binaries that are linked to libraries
other than libc.so.

Currently we just backup the dynamic symbol table, but in practice you would
want to encrypt it with a stream cypher, back it up, and then decrypt it
before restoring at runtime.
