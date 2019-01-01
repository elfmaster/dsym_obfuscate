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

bool
backup_dynstr_and_zero(elfobj_t *obj)
{
	struct elf_section dynstr, symtab;
	unsigned char *ptr;
	int i, len;
	int libc_index = 0, start_main_index = 0, glibc_version_index = 0,
	    gmon_start_index = 0;
 
	if (elf_section_by_name(obj, ".dynstr", &dynstr) == false) {
		fprintf(stderr, "couldn't find .dynstr section\n");
		return false;
	}

	ptr = elf_offset_pointer(obj, dynstr.offset);
	if (ptr == NULL) {
		fprintf(stderr, "Unable to locate offset: %#lx\n", dynstr.offset);
		return false;
	}
	if (dynstr.size >= DYNSTR_MAX_LEN) {
		fprintf(stderr, ".dynstr too large\n");
		return false;
	}
	memcpy(dynstr_backup, ptr, dynstr.size);
	dynstr_len = dynstr.size;

	for (i = 0; i < dynstr.size; i++) {
		if (strcmp(&ptr[i], "libc.so.6") == 0) {
			libc_index = i;
		}
		else if (strcmp(&ptr[i], "__libc_start_main") == 0) {
			start_main_index = i;
		}
		else if (strcmp(&ptr[i], "GLIBC_2.2.5") == 0) {
			glibc_version_index = i;
		}
		else if (strcmp(&ptr[i], "__gmon_start__") == 0) {
			gmon_start_index = i;
		}
	}
	memset(ptr, 0, dynstr.size);

	for (i = 0; i < dynstr.size; i++) {
		if (i == libc_index) {
			strcat(&ptr[i], "libc.so.6");
		}
		else if (i == start_main_index) {
			strcat(&ptr[i], "__libc_start_main");
		}
		else if (i == glibc_version_index) {
			strcat(&ptr[i], "GLIBC_2.2.5");
		}
		else if (i == gmon_start_index) {
			strcat(&ptr[i], "__gmon_start__\0");
		}
	}
	if (elf_section_by_name(obj, ".symtab", &symtab) == false) {
		fprintf(stderr, "couldn't find .symtab section (already stripped)\n");
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
	int i, j, fd;
	size_t old_size = obj->size;
	size_t stub_size;
	elfobj_t ctor_obj;
	elf_error_t error;
	unsigned long stub_vaddr;
	struct elf_symbol symbol;
	struct elf_section ctors;
	struct elf_section dynstr;
	uint8_t *ptr;

	if (elf_open_object("egg", &ctor_obj,
	    ELF_LOAD_F_STRICT|ELF_LOAD_F_MODIFY, &error) == false) {
	    fprintf(stderr, "%s\n", elf_error_msg(&error));
		exit(EXIT_FAILURE);
	}
	stub_size = ctor_obj.size;
	/*
	 * NOTE: We are directly modifying libelfmaster object's to update
	 * the program header table. This is not technically correct since
	 * its not using the libelfmaster API. Eventually libelfmaster will
	 * support this through accessor functions that are intended to modify
	 * meanwhile we are still using libelfmaster to speed up the process
	 * of creating this PoC for symbol and section lookups.
	 */
	for (i = 0; i < obj->ehdr64->e_phnum; i++) {
		if (obj->phdr64[i].p_type == PT_LOAD &&
		    obj->phdr64[i].p_offset == 0) {
			obj->phdr64[i].p_flags |= PF_W;
		}
		if (obj->phdr64[i].p_type == PT_DYNAMIC) {
			Elf64_Dyn *dyn = (Elf64_Dyn *)&obj->mem[obj->phdr64[i].p_offset];

			for (j = 0; dyn[j].d_tag != DT_NULL; j++) {
				if (dyn[j].d_tag == DT_VERNEEDNUM) {
					dyn[j].d_tag = 0;
				} else if (dyn[j].d_tag == DT_VERNEED) {
					dyn[j].d_tag = DT_DEBUG;
				}
			}
		}
		if (obj->phdr64[i].p_type == PT_NOTE) {
			obj->phdr64[i].p_type = PT_LOAD;
			obj->phdr64[i].p_vaddr = 0xc000000 + old_size;
			obj->phdr64[i].p_filesz = stub_size;
			obj->phdr64[i].p_memsz = obj->phdr64[i].p_filesz;
			obj->phdr64[i].p_flags = PF_R | PF_X;
			obj->phdr64[i].p_paddr = obj->phdr64[i].p_vaddr;
			obj->phdr64[i].p_offset = old_size;
		}
	}
	/*
	 * For debugging purposes we can view our injected code
	 * with objdump by modifying an existing section header
	 * such as .eh_frame.
	 */
#if 0
	obj->shdr64[17].sh_size = stub_size;
	obj->shdr64[17].sh_addr = 0xc000000 + old_size;
	obj->shdr64[17].sh_offset = old_size;
#endif
	/*
	 * Locate .init_array so that we can modify the pointer to
	 * our injected constructor code 'egg (built from constructor.c)'
	 */
	if (elf_section_by_name(obj, ".init_array", &ctors) == false) {
		printf("Cannot find .init_array\n");
		return false;
	}

	/*
	 * Locate the symbol for the function restore_dynstr in our
	 * constructor so that we can find out where to hook the .init_array
	 * function pointer to.
	 */
	if (elf_symbol_by_name(&ctor_obj, "restore_dynstr",
	    &symbol) == false) {
		printf("cannot find symbol \"restore_dynstr\"\n");
		return false;
	}

	/*
	 * Get a pointer to .init_array function pointer
	 * so that we can hook it with our constructor
	 * entry point 'restore_dynstr'
	 */
	ptr = elf_offset_pointer(obj, ctors.offset);

	/*
	 * Because of the way that we build the constructor using 'gcc -N'
	 * it creates a single load segment that is not PAGE aligned, we must
	 * therefore PAGE align it to get the correct symbol_offset from the beginning
	 * of the ELF file.
	 */
	uint64_t symbol_offset = symbol.value - (elf_text_base(&ctor_obj) & ~4095);
	uint64_t entry_point = 0xc000000 + old_size + symbol_offset;

	/*
	 * Set the actual constructor hook with this memcpy.
	 * i.e. *(uint64_t *)&ptr[0] = entry_point;
	 */
	memcpy(ptr, &entry_point, sizeof(uint64_t));

	/*
	 * Now lets get the address of the dynstr_vaddr
	 * within the constructor object. This will eventually
	 * hold the address of the target executables .dynstr
	 * section.
	 */
	if (elf_symbol_by_name(&ctor_obj, "dynstr_vaddr",
	    &symbol) == false) {
		printf("cannot find symbol \"dynstr_vaddr\"\n");
		return false;
	}

	symbol_offset = symbol.value - (elf_text_base(&ctor_obj) & ~4095);

	if (elf_section_by_name(obj, ".dynstr", &dynstr) == false) {
		printf("Cannot find .dynstr\n");
		return false;
	}

	/*
	 * Set dynstr_vaddr to point to .dynstr in the target
	 * executable.
	 */
	ptr = elf_offset_pointer(&ctor_obj, symbol_offset);
	memcpy(ptr, &dynstr.address, sizeof(uint64_t));

	/*
	 * Get ready to write out our new final executable
	 * which includes the constructor code as a 3rd PT_LOAD
	 * segment
	 */
	fd = open(TMP_FILE, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
	if (fd < 0) {
		perror("open");
		return false;
	}

	/*
	 * Write out the original binary
	 */
	if (write(fd, obj->mem, old_size) != old_size) {
		perror("write");
		return false;
	}
	/*
	 *  open 'egg' and find the buffer to store the contents
	 * of dynstr (It is called dynstr_buf)
	 */
	if (elf_symbol_by_name(&ctor_obj, "dynstr_buf",
	    &symbol) == false) {
		fprintf(stderr, "Unable to find symbol dynstr_buf in constructor.o\n");
		return false;
	}

	/*
	 * Patch egg so it has the .dynstr data in its
	 * char dynstr_buf[], so that at runtime it can
	 * restore it into the .dynstr section of the
	 * target executable that egg is injected into.
	 */
	ptr = elf_address_pointer(&ctor_obj, symbol.value);
	memcpy(ptr, dynstr_backup, dynstr_len);

	/*
	 * Append 'egg' constructor code to the end of the target binary
	 * the target binary has a PT_LOAD segment with corresponding offset
	 * and other values pointing to this injected code.
	 */
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
	elf_close_object(&obj);
}
	

