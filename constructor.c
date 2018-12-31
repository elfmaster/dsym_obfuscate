#include <stdio.h>
#include <elf.h>

#if 0
char * itox(long x, char *t);
char *itoa(long x, char *t);
long _write(long, char *, unsigned long);
#endif

unsigned char dynstr_buf[8192] __attribute__((section(".data"), aligned(8))) =
    { [0 ... 8191] = 0};

unsigned long dynstr_vaddr __attribute__((section(".data"))) = {0};

extern unsigned long get_rip_label;
unsigned long get_rip(void);

#define PIC_RESOLVE_ADDR(target) (get_rip() - ((char *)&get_rip_label - (char *)target))

void
_memcpy(void *dst, void *src, unsigned int len)
{
	int i;
	unsigned char *s = (unsigned char *)src;
	unsigned char *d = (unsigned char *)dst;

	for (i = 0; i < len; i++) {
		*d = *s;
		s++, d++;
	}
	return;
}

int
_strcmp(const char *s1, const char *s2)
{
	int r = 0;

	while (!(r = (*s1 - *s2) && *s2))
		s1++, s2++;
	if (!r)
		return r;
	return r = (r < 0) ? -1 : 1;
}

unsigned long
get_rip(void)
{
	unsigned long ret;

	__asm__ __volatile__
	(
	"call get_rip_label     \n"
	".globl get_rip_label   \n"
	"get_rip_label:         \n"
	"pop %%rax              \n"
	"mov %%rax, %0" : "=r"(ret)
	);

        return ret;
}

_start()
{
	restore_dynstr();
	        __asm__ volatile("mov $0, %rdi\n"
                         "mov $60, %rax\n"
                         "syscall"); 
}

long _write(long fd, char *buf, unsigned long len)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov %2, %%rdx\n"
                        "mov $1, %%rax\n"
                        "syscall" : : "g"(fd), "g"(buf), "g"(len));
        asm volatile("mov %%rax, %0" : "=r"(ret));
        return ret;
}

void
restore_dynstr(void)
{
	char *strtab = NULL;
	Elf64_Ehdr *ehdr;
	Elf64_Shdr *shdr;
	int i;
	unsigned char *mem = (unsigned char *)0x400000;
	char string[] = {'.','d','y','n','s','t','r', 0};
	char dot[] = {'.', 0};

	ehdr = (Elf64_Ehdr *)mem;
	shdr = (Elf64_Shdr *)&mem[ehdr->e_shoff];
	/*
	 * XXX cannot find the section header string table
	 * because it is not mapped.
	 */
	strtab = dynstr_vaddr; //(char *)&mem[shdr[ehdr->e_shstrndx].sh_offset];
	_memcpy((void *)dynstr_vaddr, PIC_RESOLVE_ADDR(dynstr_buf), 61);
#if 0
	for (i = 0; i < ehdr->e_shnum; i++) {
		_write(1, dot, 1);
		if (_strcmp(&strtab[shdr[i].sh_name],
		    (char *)string) != 0)
			continue;
		_memcpy((char *)shdr[i].sh_addr,
		    (char *)PIC_RESOLVE_ADDR(dynstr_buf),
		    shdr[i].sh_size);
		break;
	}
#endif
	return;
}
