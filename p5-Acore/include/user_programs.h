
extern unsigned char _elf___test_test_shell_elf[];
int _length___test_test_shell_elf;
extern unsigned char _elf___test_echo_elf[];
int _length___test_echo_elf;
extern unsigned char _elf___test_recv_elf[];
int _length___test_recv_elf;
extern unsigned char _elf___test_send_elf[];
int _length___test_send_elf;
typedef struct ElfFile {
  char *file_name;
  unsigned char* file_content;
  int* file_length;
} ElfFile;

#define ELF_FILE_NUM 4
extern ElfFile elf_files[4];
extern int get_elf_file(const char *file_name, unsigned char **binary, int *length);
