#include <stdio.h>
#include <elf.h>

int main(void)
{
	printf("Hello there\n");
	printf("%d\n", sizeof(Elf64_Ehdr));
	exit(0);
}
