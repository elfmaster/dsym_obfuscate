#include <stdio.h>
#include <elf.h>

unsigned char dynstr_buf[8192] __attribute__((section(".data"), aligned(8))) =
    { [0 ... 8191] = 0};

unsigned long dynstr_vaddr __attribute__((section(".data"))) = {0};
unsigned long dynstr_size __attribute__((section(".data"))) = {0};
extern unsigned long get_rip_label;

unsigned long get_rip(void);
void restore_dynstr(void);

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

/*
 * NOTE: This was only for testing purposes, _start() is never
 * actually called. restore_dynstr() is called by the target
 * executables constructor function table that we patch.
 */
int _start()
{
	restore_dynstr();
	        __asm__ volatile("mov $0, %rdi\n"
                         "mov $60, %rax\n"
                         "syscall"); 
}

void
restore_dynstr(void)
{
	/*
	 * We copy over the original contents of .dynstr and put them
	 * back into place right before the dynamic linker needs them
	 * for lazy linking.
	 */
	_memcpy((char *)dynstr_vaddr, (char *)PIC_RESOLVE_ADDR(dynstr_buf), dynstr_size);
	return;
}
