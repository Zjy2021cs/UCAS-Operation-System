#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

#include <os.h>

static char buff[64];

int main(void)
{
    int i, j;
    int fd = sys_fopen("1.txt", O_RDWR); 

    int write_block = 2048;
    int read_time = write_block/256;
    // write 'hello world!' * 10
    for (i = 0; i < write_block; i++)
    {
        int block_num = sys_fwrite(fd, "hello world!\n", 13);
        if(i%256==0){
            printf("[FILE_W]Writing...block_num:%d\n",block_num);
        }
    }

    /*read*/
    for (i = 0; i < read_time; i++)
    {
        sys_fread(fd, buff, 13);
        for (j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }

    sys_close(fd);
}