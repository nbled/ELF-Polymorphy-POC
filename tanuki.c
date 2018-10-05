#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <elf.h>

__attribute__((section(".poly"))) void null() {
    asm("nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
    );
}

void read_elf_headers(int fd, Elf64_Ehdr *headers) {
    if (read(fd, (void *)headers, sizeof(Elf64_Ehdr)) < 0) {
        perror("! Failed to read headers");
        exit(-1);
    }
}

void read_elf_sections(int fd, Elf64_Ehdr* headers, Elf64_Shdr** sections) {
    for (unsigned int i = 0; i < headers->e_shnum; i++) {
        lseek(fd, (off_t)headers->e_shoff+(headers->e_shentsize * i), SEEK_SET);
        sections[i] = malloc(sizeof(Elf64_Shdr));
        if (read(fd, sections[i], headers->e_shentsize) < 0) {
            perror("! Failed to read section");
            exit(-1);
        }
    }
}

void* read_elf_section(int fd, Elf64_Shdr* section) {
    lseek(fd, (off_t)section->sh_offset, SEEK_SET);
    void* buffer = malloc(section->sh_size);
    if (read(fd, buffer, section->sh_size) < 0) {
        perror("! Failed to read section");
    }
    return buffer;
}

void randomize(char* buffer, size_t buffer_size) {
    for (unsigned int i = 0; i < buffer_size; i++) {
        buffer[i] = rand() % 256;
    }
}

void copy(int src_fd, int dst_fd) {
    lseek(src_fd, 0, SEEK_SET);
    char* buffer = malloc(1);
    while (read(src_fd, buffer, 1) > 0) {
        write(dst_fd, buffer, 1);
    }
    free(buffer);
}

int main(int argc, char* argv[]) {
    if (argc == 0) {
        return EXIT_FAILURE;
    }

    printf("+ Filename is: %s\n", argv[0]);
    fflush(stdout);

    // Open specified file    
    int fd = open("/proc/self/exe", O_RDONLY);
    if (fd < 0) {
        perror("! Failed to open file");
        exit(-1);
    }

    // Read ELF headers
    Elf64_Ehdr* headers = malloc(sizeof(Elf64_Ehdr));
    read_elf_headers(fd, headers);

    // Check if there is any section table
    if (!headers->e_shoff) {
        perror("! Failed to find sections table");
        exit(-1);
    }

    // Read ELF sections
    Elf64_Shdr** sections = malloc(sizeof(Elf64_Shdr*) * headers->e_shnum);
    read_elf_sections(fd, headers, sections);

    // Get .poly section
    void* string_table = read_elf_section(fd, sections[headers->e_shstrndx]);
    Elf64_Shdr* poly_section;
    for (unsigned int i = 0; i < headers->e_shnum; i++) {
        char* name = string_table + sections[i]->sh_name;
        if (strncmp(name, ".poly", 8) == 0) {
            poly_section = sections[i];
        }
    }
    if (!poly_section) {
        printf("! Failed to get .poly section");
        exit(-1);
    }
    printf("+ .poly offset is: 0x%lx\n", poly_section->sh_offset);
    printf("+ .poly size is: 0x%lx\n", poly_section->sh_size);

    // Randomize .poly section
    srand(time(NULL));
    void* poly_content = read_elf_section(fd, poly_section);
    randomize((char*)poly_content, poly_section->sh_size);

    // Copy actual executable
    char target[256];
    strncpy(target, argv[0], 128);
    strncat(target, "_00_tmp", 8);

    int fd2 = open(target, O_WRONLY | O_CREAT, S_IRWXU);
    copy(fd, fd2);

    // Write out new headers
    lseek(fd2, (off_t)poly_section->sh_offset, SEEK_SET);
    if (write(fd2, poly_content, poly_section->sh_size) < 0) {
        perror("! Failed to write new headers");
        exit(-1);
    }
    printf("+ 0x%lx bytes have been written\n", poly_section->sh_size);

    // Delete the original & rename tmp file
    unlink(argv[0]);
    rename(target, argv[0]);

    // Free heap-allocated memory
    free(poly_content);
    free(string_table);
    for (unsigned int i = 0; i < headers->e_shnum; i++) {
        free(sections[i]);
    }
    free(sections);
    free(headers);

    // Close file descriptor
    close(fd);
    close(fd2);

    return EXIT_SUCCESS;
}
