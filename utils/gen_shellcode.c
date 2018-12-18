#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(int argc, char **argv)
{
	int fd;
	struct stat st;
	FILE *of;
	uint8_t *mem;
	size_t i;

	fd = open(argv[1], O_RDONLY);
	fstat(fd, &st);
	of = fopen("shellcode", "w+");

	mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	fprintf(of, "unsigned char stub_shellcode[] =\n");
	fprintf(of, "\"");
	for (i = 0; i < st.st_size; i++) {
		if (i > 1)
			if ((i % 32) == 0) {
				fprintf(of, "\"");
				fprintf(of, "\n");
				fprintf(of, "\"");
			}
		fprintf(of, "\\x%02x", mem[i]);
	}
	fprintf(of, "\";");
	fclose(of);
	close(fd);
}

