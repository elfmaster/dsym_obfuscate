#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(void)
{
	char *string = "hi:you ";
	char *p;
	FILE *fd = fopen("/etc/passwd", "r");
	printf("Hi\n");
	p = strtok(string, ": ");
	pause();
}

