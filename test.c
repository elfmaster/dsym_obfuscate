#include <stdio.h>
#include <elf.h>

int main(void)
{
	FILE *fd = fopen("/etc/passwd", "r");
	printf("Hi\n");
	pause();
}

