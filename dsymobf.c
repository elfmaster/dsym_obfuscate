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

#define TMP_FILE ".xyz.file"
#define PADDING_SIZE 1024
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define PAGE_ALIGN(x) (x & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x) (PAGE_ALIGN(x) + PAGE_SIZE)
#define PAGE_ROUND(x) (PAGE_ALIGN_UP(x))

#define DYNSTR_MAX_LEN 8192 * 4

unsigned char dynstr_backup[DYNSTR_MAX_LEN];
unsigned long int dynstr_len;

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

void printit(size_t len, uint8_t *ptr)
{
	int i;
	static int count = 0;

	printf("Printing %d item\n", count++);
	for (i = 0; i < len; i++)
		printf("%c", ptr[i]);
	printf("\n");
}

bool
backup_dynstr_and_zero(elfobj_t *obj)
{
	struct elf_section dynstr, symtab;
	unsigned char *ptr;
	int i, len;

	if (elf_section_by_name(obj, ".dynstr", &dynstr) == false) {
		fprintf(stderr, "couldn't find .dynstr section\n");
		return false;
	}

	ptr = elf_offset_pointer(obj, dynstr.offset);
	if (ptr == NULL) {
		fprintf(stderr, "Unable to locate offset: %#lx\n", dynstr.offset);
		return false;
	}
	printf("dynstr offset: %lx, zeroing bytes at %p\n", dynstr.offset, ptr);
	if (dynstr.size >= DYNSTR_MAX_LEN) {
		fprintf(stderr, ".dynstr too large\n");
		return false;
	}
	printf("Before backing up\n");
	printit(dynstr.size + 32, ptr);
	printf("Backing up dynamic string table from ptr: %p, %lu bytes\n", ptr, dynstr.size);

	memcpy(dynstr_backup, ptr, dynstr.size);
	dynstr_len = dynstr.size;
	printf("After we do backup\n");
	printit(dynstr.size + 32, ptr);

	int libc_index, start_main_index, glibc_version_index, gmon_start_index;
	size_t libc_len, start_main_len, glibc_version_len, gmon_start_len;

	printf("Before loop\n");
	printit(dynstr.size + 32, ptr);

	for (i = 0; i < dynstr.size; i++) {
		if (strcmp(&ptr[i], "libc.so.6") == 0) {
			libc_index = i;
			libc_len = strlen("libc.so.6");
		}
		else if (strcmp(&ptr[i], "__libc_start_main") == 0) {
			start_main_index = i;
			start_main_len = strlen("__libc_start_main");
		}
		else if (strcmp(&ptr[i], "GLIBC_2.2.5") == 0) {
			glibc_version_index = i;
			glibc_version_len = strlen("GLIBC_2.2.5");
		}
		else if (strcmp(&ptr[i], "__gmon_start__") == 0) {
			gmon_start_index = i;
			gmon_start_len = strlen("__gmon_start__");
		}
	}
	printit(dynstr.size + 32, ptr);
	for (i = 0; i < dynstr.size + 32; i++)
		printf("%c", ptr[i]);
	printf("\n");
	memset(ptr, 0, dynstr.size);
	printf("After memset\n");

	printit(dynstr.size + 32, ptr);

	for (i = 0; i < dynstr.size; i++) {
		if (i == libc_index) {
			strcat(&ptr[i], "libc.so.6");
			printit(libc_len, &ptr[i]);
		}
		else if (i == start_main_index) {
			strcat(&ptr[i], "__libc_start_main");
			printit(start_main_len, &ptr[i]);
		}
		else if (i == glibc_version_index) {
			strcat(&ptr[i], "GLIBC_2.2.5");
			printit(glibc_version_len, &ptr[i]);
		}
		else if (i == gmon_start_index) {
			strcat(&ptr[i], "__gmon_start__\0");
			printit(gmon_start_len, &ptr[i]);
		}
	}
	for (i = 0; i < dynstr.size + 32; i++) {
		printf("%c", ptr[i]);
	}
	printf("\n");
	if (elf_section_by_name(obj, ".symtab", &symtab) == false) {
		fprintf(stderr, "couldn't find .symtab section\n");
		goto done;
	}
	ptr = elf_offset_pointer(obj, symtab.offset);
	for (i = 0; i < symtab.size; i++)
		ptr[i] = '\0';
done:
	return true;
}

bool
inject_constructor(elfobj_t *obj)
{
	int i, fd;
	size_t old_size = obj->size;
	size_t stub_size;
	elfobj_t ctor_obj;
	elf_error_t error;
	unsigned long stub_vaddr;
	struct elf_symbol symbol;
	struct elf_section ctors;
	uint8_t *ptr;

	if (elf_open_object("egg", &ctor_obj,
	    ELF_LOAD_F_STRICT|ELF_LOAD_F_MODIFY, &error) == false) {
	    fprintf(stderr, "%s\n", elf_error_msg(&error));
		exit(EXIT_FAILURE);
	}
	stub_size = ctor_obj.size;

	for (i = 0; i < obj->ehdr64->e_phnum; i++) {
		if (obj->phdr64[i].p_type != PT_NOTE)
			continue;
		printf("Changing load segments\n");
		obj->phdr64[i].p_type = PT_LOAD;
		obj->phdr64[i].p_vaddr = 0xc000000 + old_size;
		printf("Setting filesz for new PT_LOAD to %d\n",
		    stub_size + PADDING_SIZE);
		obj->phdr64[i].p_filesz = stub_size + PADDING_SIZE;
		obj->phdr64[i].p_memsz = obj->phdr64[i].p_filesz;
		obj->phdr64[i].p_flags = PF_R | PF_X;
		obj->phdr64[i].p_paddr = obj->phdr64[i].p_vaddr;
		obj->phdr64[i].p_offset = old_size;
	}
#if 0
	obj->shdr64[3].sh_size = stub_size;
	obj->shdr64[3].sh_addr = 0xc000000 + old_size;
	obj->shdr64[3].sh_offset = old_size;
#endif

	if (elf_section_by_name(obj, ".init_array", &ctors) == false) {
		printf("Cannot find .init_array\n");
		return false;
	}

	if (elf_symbol_by_name(&ctor_obj, "restore_dynstr",
	    &symbol) == false) {
		printf("cannot find symbol \"restore_dynstr\"\n");
		return false;
	}
	printf("symbol value: %lx\n", symbol.value);
	ptr = elf_offset_pointer(obj, ctors.offset);
	uint64_t entry_point = 0xc000000 + old_size + symbol.value; // + sizeof(Elf64_Ehdr);
	memcpy(ptr, &entry_point, sizeof(uint64_t));
	printf("Set .init_array to %#lx\n", 0xc000000 + old_size + symbol.value);
	printf("ptr value: %lx\n", *(uint64_t *)ptr);
	fd = open(TMP_FILE, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
	if (fd < 0) {
		perror("open");
		return false;
	}

	printf("obj->shdr64[1].sh_offset = %d same as %d?\n", obj->shdr64[1].sh_offset, old_size);
	printf("Writing %d bytes of obj->mem\n", old_size);
	if (write(fd, obj->mem, old_size) != old_size) {
		perror("write");
		return false;
	}
	/*
	 * open constructor.o and find the buffer to store the contents
	 * of dynstr (It is called dynstr_buf)
	 */
	if (elf_symbol_by_name(&ctor_obj, "dynstr_buf",
	    &symbol) == false) {
		fprintf(stderr, "Unable to find symbol dynstr_buf in constructor.o\n");
		return false;
	}

	/*
	 * Patch egg so it has the dynstr data
	 */
	printf("Symbol location: %lx\n", symbol.value);
	ptr = elf_offset_pointer(&ctor_obj, symbol.value);
	ptr += sizeof(Elf64_Ehdr);
	printf("symbol.value: %lx\n", symbol.value);
	printf("Copying %d bytes\n", dynstr_len);
	_memcpy(ptr, dynstr_backup, dynstr_len);

	/*
	 * Append constructor.o to the end of the target binary
	 * the target binary has a PT_LOAD segment with corresponding offset
	 * and other values pointing to this injected code.
	 */
	printf("Writing out constructor.o into final object. %d bytes written\n",
	    ctor_obj.size);

	if (write(fd, (char *)ctor_obj.mem, ctor_obj.size) != ctor_obj.size) {
		perror("write");
		return false;
	}
	if (rename(TMP_FILE, obj->path) < 0) {
		perror("rename");
		return false;
	}
	close(fd);
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
	    ELF_LOAD_F_STRICT|ELF_LOAD_F_MODIFY, &error) == false) {
		fprintf(stderr, "%s\n", elf_error_msg(&error));
		exit(EXIT_FAILURE);
	}

	printf("backing up dynstr\n");
	res = backup_dynstr_and_zero(&obj);

	printf("Injecting constructor.o into %s\n", argv[1]);
	res = inject_constructor(&obj);

	printf("Commiting changes to %s\n", obj.path);
	elf_close_object(&obj); }
	
	

