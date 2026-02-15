#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>
#include "include/options.h"

#define BASE_ADDR 0x400000
#define PAGE_SIZE 0x1000

uint64_t find_entry_symbol(
    FILE *f,
    Elf64_Ehdr *eh,
    Elf64_Shdr *shdrs,
    const char *symbol
) {
    for (int i = 0; i < eh->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            Elf64_Sym *syms = malloc(shdrs[i].sh_size);
            fseek(f, shdrs[i].sh_offset, SEEK_SET);
            fread(syms, 1, shdrs[i].sh_size, f);

            Elf64_Shdr *strtab = &shdrs[shdrs[i].sh_link];
            char *names = malloc(strtab->sh_size);
            fseek(f, strtab->sh_offset, SEEK_SET);
            fread(names, 1, strtab->sh_size, f);

            int n = shdrs[i].sh_size / sizeof(Elf64_Sym);
            for (int j = 0; j < n; j++) {
                if (!strcmp(&names[syms[j].st_name], symbol)) {
                    uint64_t addr = syms[j].st_value;
                    free(syms);
                    free(names);
                    return addr;
                }
            }

            free(syms);
            free(names);
        }
    }

    fprintf(stderr, "xld: entry symbol '%s' not found\n", symbol);
    exit(1);
}

int main(int argc, char **argv) {
    xld_options_t opt;
    options_init(&opt);
    options_parse(argc, argv, &opt);

    FILE *in = fopen(opt.input_file, "rb");
    if (!in) {
        perror("input");
        return 1;
    }

    Elf64_Ehdr eh;
    fread(&eh, sizeof(eh), 1, in);

    Elf64_Shdr *shdrs = malloc(eh.e_shnum * sizeof(Elf64_Shdr));
    fseek(in, eh.e_shoff, SEEK_SET);
    fread(shdrs, sizeof(Elf64_Shdr), eh.e_shnum, in);

    Elf64_Shdr *text = NULL;
    for (int i = 0; i < eh.e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_PROGBITS &&
            (shdrs[i].sh_flags & SHF_EXECINSTR)) {
            text = &shdrs[i];
            break;
        }
    }

    if (!text) {
        fprintf(stderr, "xld: .text section not found\n");
        return 1;
    }

    uint8_t *code = malloc(text->sh_size);
    fseek(in, text->sh_offset, SEEK_SET);
    fread(code, 1, text->sh_size, in);

    uint64_t start_offset =
        find_entry_symbol(in, &eh, shdrs, opt.entry_symbol);

    uint64_t entry = BASE_ADDR + PAGE_SIZE + start_offset;

    fclose(in);

    FILE *out = fopen(opt.output_file, "wb");
    if (!out) {
        perror("output");
        return 1;
    }

    Elf64_Ehdr out_eh = {0};
    memcpy(out_eh.e_ident, ELFMAG, 4);
    out_eh.e_ident[EI_CLASS] = ELFCLASS64;
    out_eh.e_ident[EI_DATA]  = ELFDATA2LSB;
    out_eh.e_ident[EI_VERSION] = EV_CURRENT;

    out_eh.e_type = ET_EXEC;
    out_eh.e_machine = EM_X86_64;
    out_eh.e_version = EV_CURRENT;
    out_eh.e_entry = entry;
    out_eh.e_phoff = sizeof(Elf64_Ehdr);
    out_eh.e_ehsize = sizeof(Elf64_Ehdr);
    out_eh.e_phentsize = sizeof(Elf64_Phdr);
    out_eh.e_phnum = 1;

    fwrite(&out_eh, sizeof(out_eh), 1, out);

    Elf64_Phdr ph = {0};
    ph.p_type = PT_LOAD;
    ph.p_flags = PF_R | PF_X;
    ph.p_offset = PAGE_SIZE;
    ph.p_vaddr = BASE_ADDR + PAGE_SIZE;
    ph.p_paddr = ph.p_vaddr;
    ph.p_filesz = text->sh_size;
    ph.p_memsz  = text->sh_size;
    ph.p_align  = PAGE_SIZE;

    fwrite(&ph, sizeof(ph), 1, out);

    fseek(out, PAGE_SIZE, SEEK_SET);
    fwrite(code, 1, text->sh_size, out);

    fclose(out);

    if (!opt.silent) {
        printf("xld: input  = %s\n", opt.input_file);
        printf("xld: output = %s\n", opt.output_file);
        printf("xld: entry  = %s (0x%lx)\n",
               opt.entry_symbol, entry);
        printf("xld: text size = %lu bytes\n", text->sh_size);
    }

    return 0;
}

