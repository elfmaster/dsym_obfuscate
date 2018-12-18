#define _GNU_SOURCE
#include <libelfmaster.h>

#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PADDING_SIZE 1024
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define PAGE_ALIGN(x) (x & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x) (PAGE_ALIGN(x) + PAGE_SIZE)
#define PAGE_ROUND(x) (PAGE_ALIGN_UP(x))

#define DYNSTR_MAX_LEN 8192 * 4

unsigned char dynstr_backup[DYNSTR_MAX_LEN];

bool
backup_dynstr_and_zero(elfobj_t *obj)
{
	struct elf_section dynstr;
	unsigned char *ptr;

	if (elf_section_by_name(obj, ".dynstr", &dynstr) == false) {
		fprintf(stderr, "couldn't find .dynstr section\n");
		return false;
	}
	ptr = elf_address_pointer(obj, dynstr.address);
	if (ptr == NULL) {
		fprintf(stderr, "Unable to locate address: %#lx\n", dynstr.address);
		return false;
	}
	if (dynstr.size >= DYNSTR_MAX_LEN) {
		fprintf(stderr, ".dynstr too large\n");
		return false;
	}
	memcpy(dynstr_backup, ptr, dynstr.size);
	for (i = 0; i < dynstr.size; i++)
		ptr[i] = '\0';
	return true;
}

bool
inject_constructor(elfobj_t *obj)
{
	int i;
	size_t old_size = obj->size;
	size_t stub_size = sizeof(stub_shellcode);
	elfobj_t ctor_obj;

	for (i = 0; i < obj->ehdr64->e_phnum; i++) {
		if (obj->phdr64[i].p_type != PT_NOTE)
			continue;
		obj->phdr64[i].p_type = PT_LOAD;
		stub_vaddr = obj->phdr64[i].p_vaddr = 0xc000000 + old_size;
		obj->phdr64[i].p_filesz = stub_size + PADDING_SIZE;
		obj->phdr64[i].p_memsz = obj->phdr64[i].p_filesz;
		obj->phdr64[i].p_flags = PF_R | PF_X;
		obj->phdr64[i].p_paddr = obj->phdr64[i].p_vaddr;
		obj->phdr64[i].p_offset = old_size;
	}
	fd = open(TMP_FILE, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
	if (fd < 0) {
		perror("open");
		return false;
	}

	obj->shdr64[1].sh_offset = old_size;
	obj->shdr64[1].sh_addr = 0xc000000 + old_size;
	obj->shdr64[1].sh_type = SHT_PROGBITS;
	obj->shdr64[1].sh_size = stub_size + 16;

	if (write(fd, obj->mem, old_size) < 0) {
		perror("write");
		return false;
	}

	if (elf_open_object("constructor.o", &ctor_obj,
	    ELF_LOAD_F_STRICT|ELF_LOAD_F_MAP_WRITE, &error) == false) {
		fprintf(stderr, "%s\n", elf_error_msg(&error));
		exit(EXIT_FAILURE);
	}
	/*
	 * open constructor.o and find the buffer to store the contents
	 * of dynstr (It is called dynstr_buf)
	 */
	if (elf_symbol_by_name(&ctor_obj, "dynstr_buf",
	    dynstr_buf) == false) {
		fprintf(stderr, "Unable to find symbol dynstr_buf in constructor.o\n");
		return false;
	}
	/*
	 * Append constructor.o to the end of the target binary
	 * the target binary has a PT_LOAD segment with corresponding offset
	 * and other values pointing to this injected code.
	 */
	if (write(fd, (char *)ctor_obj.mem, ctor_obj.size) < 0) {
		perror("write");
		return false;
	}
	close(fd);
	if (rename(TMP_FILE, obj->path) < 0) {
		perror("rename");
		return false;
	}
	(void) elf_close_object(&ctor_obj);
	return true;
}

int
main(int argc, char **argv)
{
	elfobj_t obj;
	elf_error_t error;
	bool res;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <binary>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	if (elf_open_object(argv[1], &obj,
	    ELF_LOAD_F_STRICT|ELF_LOAD_F_MAP_WRITE, &error) == false) {
		fprintf(stderr, "%s\n", elf_error_msg(&error));
		exit(EXIT_FAILURE);
	}

	res = backup_dynstr_and_zero(&obj);
	res = inject_constructor(&obj);
	elf_close_object(&obj);
}
	
	

