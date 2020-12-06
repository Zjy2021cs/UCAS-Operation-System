/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <test.h>
#include <string.h>
#include <os.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <alloca.h>

#define SCREEN_WIDTH    80
#define SCREEN_HEIGHT   50

// test exit
void test_shell_task1()
{
    int i;

    for (i = 0; i < 500; i++)
    {
        sys_move_cursor(0, 0);
        printf("I am Task A.(%d)           \n", i);
    }

    sys_exit();
}

// test kill & waitpid
void test_shell_task2()
{
    int i;

    sys_move_cursor(0, 1);
    printf("I am waiting Task A to exit.\n");
    sys_waitpid(2);

    for (i = 0;; i++)
    {
        sys_move_cursor(0, 1);
        printf("I am Task B.(%d)           \n", i);
    }
}

// test waitpid
void test_shell_task3()
{
    int i;

    sys_move_cursor(0, 2);
    printf("I am waiting Task B to exit.\n");
    sys_waitpid(3);

    for (i = 0;; i++)
    {
        sys_move_cursor(0, 2);
        printf("I am Task C.(%d)           \n", i);
    }
}

#define SHELL_BUFF_SIZE 64
#define MAX_ARGS 256

static int shell_tail = 0;
static char shell_buff[SHELL_BUFF_SIZE];

/* TODO: ps, exec, kill, exit, clear, wait , taskset
void parse_command(char buffer[]){
    char argv[4][8];
    int i=0,j=0;
    int num;
    while(buffer[j]!='\0' && buffer[j]!=' '){
        argv[0][i++]=buffer[j++];
    }
    argv[0][i]='\0';

    int k=j;
    if(!strcmp(argv[0],"taskset")){
        int count_arg=0;
        while(buffer[k++]!='\0'){
            i=0;count_arg++;
            while(buffer[k]!='\0' && buffer[k]!=' '){
                argv[count_arg][i++] = buffer[k++];
            }
            argv[count_arg][i]='\0';
        }
        if(!strcmp(argv[1],"-p")){       //taskset -p mask pid
            int mask, pid;
            if(!strcmp(argv[2],"0x1")){
                mask = 1;
            }else if(!strcmp(argv[2],"0x2")){
                mask = 2;
            }else{
                mask = 3;
            }
            pid = atoi(argv[3]);
            sys_taskset_p(mask,pid);
            printf("%s\n",buffer);
        }else{                           //taskset mask num
            int mask;
            if(!strcmp(argv[1],"0x1")){
                mask = 1;
            }else if(!strcmp(argv[1],"0x2")){
                mask = 2;
            }else{
                mask = 3;
            }
            num = atoi(argv[2]);
            sys_taskset_exec(mask,test_tasks[num],AUTO_CLEANUP_ON_EXIT);
            printf("exec process[%d]\n",num);
        }
        return;
    }

    if(buffer[j++]==' '){
        i=0;
        while(buffer[j]!='\0'){
            argv[1][i++]=buffer[j++];
        }
        argv[1][i]='\0';
        num = atoi(argv[1]);
    }

    if(!strcmp(argv[0],"ps")){
        sys_process_show();
    }else if(!strcmp(argv[0],"exec")){
        pid_t pid;
        pid = sys_spawn(test_tasks[num], NULL, AUTO_CLEANUP_ON_EXIT);//?????????????????????????????????????
        printf("exec process[%d]\n",pid);
    }else if(!strcmp(argv[0],"kill")){
        int killed;
        killed = sys_kill(num);
        if(killed){
            printf("process[%d] has been killed.\n",num);
        }else{
            printf("error: process[%d] is not running.\n",num);
        }
    }else if(!strcmp(argv[0],"exit")){
        sys_exit();
    }else if(!strcmp(argv[0],"clear")){
        sys_screen_clear();
        sys_move_cursor(1, SHELL_BEGIN);
        printf("------------------- COMMAND -------------------\n");
    }else if(!strcmp(argv[0],"wait")){
        int waiting;
        waiting = sys_waitpid(num);
        if(waiting){
            printf("waiting for process[%d]...\n",num);
        }else{
            printf("error: process[%d] is not found.\n",num);
        }
    }else{
        printf("Unknown command!\n");
    }
}*/

// static int start_test_task = 3;
int main()
{
    char ch;
    static char _arg_buf[SHELL_BUFF_SIZE] = {0};
    char *argv[MAX_ARGS] = {0};
    int i, l, pid, command;

    sys_move_cursor(0, SCREEN_HEIGHT / 2);
    printf("------------------- COMMAND -------------------\n");
    printf("> root@Luoshan_OS: ");

    while (1)
    {
        command = 0;
        // read UART port
        ch = sys_get_char();
        if (ch == 8 || ch == 127) // Backspace
        {
            if (shell_tail > 0)
                shell_tail--;
            printf("%c", ch);
        }
        else if (ch == 13) // Enter
        {
            command = 1;
            printf("\n");
        }
        else if (ch != 0)
        {
            shell_buff[shell_tail++] = ch;
            printf("%c", ch);
        }

        if (command == 1)
        {
            shell_buff[shell_tail] = '\0';
            l = strlen(shell_buff);

            // process show
            if (memcmp(shell_buff, "ps", 2) == 0)
            {
                sys_process_show();
            }
            // process show
            else if (memcmp(shell_buff, "ls", 2) == 0)
            {
                sys_show_exec();
            }
            // spawn a new task
            else if (memcmp(shell_buff, "exec", 4) == 0)
            {
                pid = 0;
                l = strlen(shell_buff);

                int argc = 0;
                for (i = 4; i < l; i++)
                {
                    if (shell_buff[i] != ' ' && shell_buff[i-1] == ' ')
                        argc++;
                }
                strcpy(_arg_buf, &shell_buff[4]);
                int tmp_argc = 0;
                l -= 4;
                for (i = 0; i < l; ++i) {
                    if (_arg_buf[i] == ' ') {
                        _arg_buf[i] = 0;
                    } else if (_arg_buf[i - 1] == 0) {
                        argv[tmp_argc++] = &_arg_buf[i];
                    }
                }

                printf("exec process %s.\n", argv[0]);
                pid_t cmd_pid = sys_exec(argv[0], argc, argv,
                          AUTO_CLEANUP_ON_EXIT);
                printf("success! pid = %d\n",cmd_pid);
            }
            // kill process
            else if (memcmp(shell_buff, "kill", 4) == 0)
            {
                pid = 0;
                l = strlen(shell_buff);

                for (i = 4; i < l; i++)
                {
                    if (shell_buff[i] != ' ')
                        break;
                }

                while ((i < l) && (shell_buff[i] != ' '))
                {
                    pid = 10 * pid + (shell_buff[i] - '0');
                    i++;
                }

                if (pid != 0)
                {
                    printf("kill process pid=%d\n", pid);
                    sys_kill(pid);
                }
            }
            // clear the screen
            else if (memcmp(shell_buff, "clear", 5) == 0)
            {
                sys_screen_clear(0, SCREEN_HEIGHT - 1);
                sys_move_cursor(0, SCREEN_HEIGHT / 2);
                printf("------------------- COMMAND -------------------\n");
            }

            printf("> root@Luoshan_OS: ");
            shell_tail = 0;
        }
    }    
    return 0;
}