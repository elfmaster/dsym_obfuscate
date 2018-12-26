#include <stdio.h>
#include <elf.h>

#if 0
char * itox(long x, char *t);
char *itoa(long x, char *t);
long _write(long, char *, unsigned long);
#endif

unsigned char dynstr_buf[8192] __attribute__((section(".data"), aligned(8))) =
    { [0 ... 8191] = 0};

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

#if 0
int _printf(char *fmt, ...)
{
        int in_p;
        unsigned long dword;
        unsigned int word;
        char numbuf[26] = {0};
        __builtin_va_list alist;

        in_p;

        __builtin_va_start((alist), (fmt));

        in_p = 0;
        while(*fmt) {
                if (*fmt!='%' && !in_p) {
                        _write(1, fmt, 1);
                        in_p = 0;
                }
                else if (*fmt!='%') {
                        switch(*fmt) {
                                case 's':
                                        dword = (unsigned long) __builtin_va_arg(alist, long);
                                        _puts((char *)dword);
                                        break;
                                case 'u':
                                        word = (unsigned int) __builtin_va_arg(alist, int);
                                        _puts(itoa(word, numbuf));
                                        break;
                                case 'd':
                                        word = (unsigned int) __builtin_va_arg(alist, int);
                                        _puts(itoa(word, numbuf));
                                        break;
                                case 'x':
                                        dword = (unsigned long) __builtin_va_arg(alist, long);
                                        _puts(itox(dword, numbuf));
                                        break;
                                default:
                                        _write(1, fmt, 1);
                                        break;
                        }
                        in_p = 0;
                }
                else {
                        in_p = 1;
                }
            fmt++;
        }
        return 1;
}

char * itox(long x, char *t)
{
        int i;
        int j;

        i = 0;
        do
        {
                t[i] = (x % 16);

                /* char conversion */
                if (t[i] > 9)
                        t[i] = (t[i] - 10) + 'a';
                else
                        t[i] += '0';

                x /= 16;
                i++;
        } while (x != 0);

        t[i] = 0;

        for (j=0; j < i / 2; j++) {
                t[j] ^= t[i - j - 1];
                t[i - j - 1] ^= t[j];
                t[j] ^= t[i - j - 1];
        }
	return t;

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

#endif

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

void
restore_dynstr(void)
{
	char *strtab = NULL;
	Elf64_Ehdr *ehdr;
	Elf64_Shdr *shdr;
	int i;
	unsigned char *mem;
	const char *string = ".dynstr";

	ehdr = (void *)0x400000;
	shdr = (Elf64_Shdr *)&mem[ehdr->e_shoff];
	strtab = (char *)&mem[shdr[ehdr->e_shstrndx].sh_offset];

	for (i = 0; i < ehdr->e_shnum; i++) {
		if (_strcmp(&strtab[shdr[i].sh_name],
		    (char *)PIC_RESOLVE_ADDR(string)) != 0)
			continue;
		_memcpy((char *)shdr[i].sh_addr,
		    (char *)PIC_RESOLVE_ADDR(dynstr_buf),
		    shdr[i].sh_size);
		break;
	}
	return;
}
