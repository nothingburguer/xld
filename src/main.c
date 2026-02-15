#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>
#include "include/options.h"

#define BASE_ADDR 0x400000
#define PAGE_SIZE 0x1000
#define TEXT_ADDR (BASE_ADDR + PAGE_SIZE)

#define BOLD  "\033[1m"
#define RESET "\033[0m"

typedef struct {
    Elf64_Shdr *text;
    Elf64_Shdr *rodata;
    Elf64_Shdr *data;
    Elf64_Shdr *bss;
} sections_t;

typedef struct {
    uint64_t text;
    uint64_t rodata;
    uint64_t data;
    uint64_t bss;
} runtime_layout_t;


static uint64_t align_up(uint64_t v) {
    return (v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

const char *get_section_name(
    FILE *f,
    Elf64_Ehdr *eh,
    Elf64_Shdr *shdrs,
    int index
) {
    static char namebuf[256];

    Elf64_Shdr shstr = shdrs[eh->e_shstrndx];
    char *names = malloc(shstr.sh_size);

    fseek(f, shstr.sh_offset, SEEK_SET);
    fread(names, 1, shstr.sh_size, f);

    strcpy(namebuf, names + shdrs[index].sh_name);
    free(names);
    return namebuf;
}

uint64_t section_runtime_addr(runtime_layout_t *l, const char *name) {
    if (!strcmp(name, ".text"))   return l->text;
    if (!strcmp(name, ".rodata")) return l->rodata;
    if (!strcmp(name, ".data"))   return l->data;
    if (!strcmp(name, ".bss"))    return l->bss;

    fprintf(stderr, "xld: unsupported relocation target '%s'\n", name);
    exit(1);
}

sections_t find_sections(
    FILE *f,
    Elf64_Ehdr *eh,
    Elf64_Shdr *shdrs
) {
    sections_t s = {0};

    Elf64_Shdr shstr = shdrs[eh->e_shstrndx];
    char *names = malloc(shstr.sh_size);

    fseek(f, shstr.sh_offset, SEEK_SET);
    fread(names, 1, shstr.sh_size, f);

    for (int i = 0; i < eh->e_shnum; i++) {
        char *n = names + shdrs[i].sh_name;

        if (!strcmp(n, ".text"))   s.text   = &shdrs[i];
        if (!strcmp(n, ".rodata")) s.rodata = &shdrs[i];
        if (!strcmp(n, ".data"))   s.data   = &shdrs[i];
        if (!strcmp(n, ".bss"))    s.bss    = &shdrs[i];
    }

    free(names);
    return s;
}

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

void apply_relocations(
    FILE *f,
    Elf64_Ehdr *eh,
    Elf64_Shdr *shdrs,
    uint8_t *text_buf,
    runtime_layout_t *layout
) {
    for (int i = 0; i < eh->e_shnum; i++) {

        if (shdrs[i].sh_type != SHT_RELA)
            continue;

        const char *secname = get_section_name(f, eh, shdrs, i);
        if (strcmp(secname, ".rela.text") != 0)
            continue;

        int count = shdrs[i].sh_size / sizeof(Elf64_Rela);
        Elf64_Rela *rela = malloc(shdrs[i].sh_size);

        fseek(f, shdrs[i].sh_offset, SEEK_SET);
        fread(rela, sizeof(Elf64_Rela), count, f);

        Elf64_Shdr *symtab = &shdrs[shdrs[i].sh_link];
        Elf64_Sym *syms = malloc(symtab->sh_size);

        fseek(f, symtab->sh_offset, SEEK_SET);
        fread(syms, 1, symtab->sh_size, f);

        for (int r = 0; r < count; r++) {

            Elf64_Rela *rel = &rela[r];

            if (ELF64_R_TYPE(rel->r_info) != R_X86_64_PC32) {
                fprintf(stderr, "xld: unsupported relocation type %ld\n",
                        ELF64_R_TYPE(rel->r_info));
                exit(1);
            }

            uint32_t sym_index = ELF64_R_SYM(rel->r_info);
            Elf64_Sym *sym = &syms[sym_index];

            const char *target_sec =
                get_section_name(f, eh, shdrs, sym->st_shndx);

            uint64_t S = section_runtime_addr(layout, target_sec);
            uint64_t P = layout->text + rel->r_offset;
            int64_t  A = rel->r_addend;

            int32_t value = (int32_t)(S + A - P);

            *(int32_t*)(text_buf + rel->r_offset) = value;
        }

        free(rela);
        free(syms);
    }
}

static void print_link_report(
    const xld_options_t *opt,
    sections_t *sec,
    uint64_t entry
) {
    if (opt->silent)
        return;

    const int W = 7;

    printf(BOLD "xld" RESET ": %-*s = %s\n", W, "input",  opt->input_file);
    printf(BOLD "xld" RESET ": %-*s = %s\n", W, "output", opt->output_file);
    printf(BOLD "xld" RESET ": %-*s = %s (0x%lx)\n",
           W, "entry", opt->entry_symbol, entry);

    if (sec->text)
        printf(BOLD "xld" RESET ": %-*s = %lu bytes\n",
               W, "text", sec->text->sh_size);

    if (sec->rodata)
        printf(BOLD "xld" RESET ": %-*s = %lu bytes\n",
               W, "rodata", sec->rodata->sh_size);

    if (sec->data)
        printf(BOLD "xld" RESET ": %-*s = %lu bytes\n",
               W, "data", sec->data->sh_size);

    if (sec->bss)
        printf(BOLD "xld" RESET ": %-*s = %lu bytes\n",
               W, "bss", sec->bss->sh_size);

    printf("\nCopyright (c) 2026 nothingburguer\n");
    printf("Licensed under MIT License. All Rights Reserved.\n");
}


int main(int argc, char **argv) {

    xld_options_t opt;
    options_init(&opt);
    options_parse(argc, argv, &opt);

    FILE *in = fopen(opt.input_file, "rb");
    if (!in) { perror("input"); return 1; }

    Elf64_Ehdr eh;
    fread(&eh, sizeof(eh), 1, in);

    Elf64_Shdr *shdrs = malloc(eh.e_shnum * sizeof(Elf64_Shdr));
    fseek(in, eh.e_shoff, SEEK_SET);
    fread(shdrs, sizeof(Elf64_Shdr), eh.e_shnum, in);

    sections_t sec = find_sections(in, &eh, shdrs);
    if (!sec.text) { fprintf(stderr, "xld: .text missing\n"); return 1; }

    uint8_t *text_buf = malloc(sec.text->sh_size);
    fseek(in, sec.text->sh_offset, SEEK_SET);
    fread(text_buf, 1, sec.text->sh_size, in);

    uint8_t *rodata_buf = NULL;
    if (sec.rodata) {
        rodata_buf = malloc(sec.rodata->sh_size);
        fseek(in, sec.rodata->sh_offset, SEEK_SET);
        fread(rodata_buf, 1, sec.rodata->sh_size, in);
    }

    uint8_t *data_buf = NULL;
    if (sec.data) {
        data_buf = malloc(sec.data->sh_size);
        fseek(in, sec.data->sh_offset, SEEK_SET);
        fread(data_buf, 1, sec.data->sh_size, in);
    }

    runtime_layout_t layout;
    layout.text   = TEXT_ADDR;
    layout.rodata = layout.text + sec.text->sh_size;
    layout.data   = align_up(layout.rodata + (sec.rodata ? sec.rodata->sh_size : 0));
    layout.bss    = layout.data + (sec.data ? sec.data->sh_size : 0);

    uint64_t start_offset = find_entry_symbol(in, &eh, shdrs, opt.entry_symbol);
    uint64_t entry = layout.text + start_offset;

    print_link_report(&opt, &sec, entry);
    apply_relocations(in, &eh, shdrs, text_buf, &layout);

    apply_relocations(in, &eh, shdrs, text_buf, &layout);

    fclose(in);

    FILE *out = fopen(opt.output_file, "wb");
    if (!out) { perror("output"); return 1; }

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
    out_eh.e_phnum = 2;

    fwrite(&out_eh, sizeof(out_eh), 1, out);

    /* LOAD 1 (RX) includes headers */
    Elf64_Phdr ph_text = {0};
    ph_text.p_type   = PT_LOAD;
    ph_text.p_flags  = PF_R | PF_X;
    ph_text.p_offset = 0;
    ph_text.p_vaddr  = BASE_ADDR;
    ph_text.p_paddr  = BASE_ADDR;
    ph_text.p_filesz = PAGE_SIZE + sec.text->sh_size +
                       (sec.rodata ? sec.rodata->sh_size : 0);
    ph_text.p_memsz  = ph_text.p_filesz;
    ph_text.p_align  = PAGE_SIZE;

    /* LOAD 2 (RW) */
    Elf64_Phdr ph_data = {0};
    ph_data.p_type   = PT_LOAD;
    ph_data.p_flags  = PF_R | PF_W;
    ph_data.p_offset = align_up(ph_text.p_filesz);
    ph_data.p_vaddr  = layout.data;
    ph_data.p_paddr  = layout.data;
    ph_data.p_filesz = sec.data ? sec.data->sh_size : 0;
    ph_data.p_memsz  = ph_data.p_filesz + (sec.bss ? sec.bss->sh_size : 0);
    ph_data.p_align  = PAGE_SIZE;

    fwrite(&ph_text, sizeof(ph_text), 1, out);
    fwrite(&ph_data, sizeof(ph_data), 1, out);

    /* write code */
    fseek(out, PAGE_SIZE, SEEK_SET);
    fwrite(text_buf, 1, sec.text->sh_size, out);
    if (sec.rodata) fwrite(rodata_buf, 1, sec.rodata->sh_size, out);

    fseek(out, ph_data.p_offset, SEEK_SET);
    if (sec.data) fwrite(data_buf, 1, sec.data->sh_size, out);

    fclose(out);

    return 0;
}

