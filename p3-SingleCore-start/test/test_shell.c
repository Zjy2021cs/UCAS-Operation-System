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

struct task_info task_test_waitpid = {
    (uintptr_t)&wait_exit_task, USER_PROCESS};
struct task_info task_test_semaphore = {
    (uintptr_t)&semaphore_add_task1, USER_PROCESS};
struct task_info task_test_condition = {
    (uintptr_t)&test_condition, USER_PROCESS};
struct task_info task_test_barrier = {
    (uintptr_t)&test_barrier, USER_PROCESS};

struct task_info task13 = {(uintptr_t)&SunQuan, USER_PROCESS};
struct task_info task14 = {(uintptr_t)&LiuBei, USER_PROCESS};
struct task_info task15 = {(uintptr_t)&CaoCao, USER_PROCESS};
struct task_info task_test_multicore = {(uintptr_t)&test_multicore, USER_PROCESS};

static struct task_info *test_tasks[16] = {&task_test_waitpid,
                                           &task_test_semaphore,
                                           &task_test_condition,
                                           &task_test_barrier,
                                           &task13, &task14, &task15,
                                           &task_test_multicore};
static int num_test_tasks = 8;

#define SHELL_BEGIN 25

char getchar_uart(){
    char c;
    int n;
    while((n=sys_get_char())==-1);
    c=(char)n;
    return c;
}

// TODO: ps, exec, kill, exit, clear, wait
void parse_command(char buffer[]){
    char argv[2][8];
    int i=0,j=0;
    int num;
    while(buffer[j]!='\0' && buffer[j]!=' '){
        argv[0][i++]=buffer[j++];
    }
    argv[0][i]='\0';
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
}

void test_shell()
{
    // TODO:
    sys_move_cursor(1, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("> root@Luoshan_OS: ");

    char buffer[16];
    int i = 0;
    while (1)
    {
        // TODO: call syscall to read UART port
        char c = getchar_uart();
        if(c=='\r'){                     //'Enter'
        // TODO: parse input   
            printf("\n");
            buffer[i]='\0';
            parse_command(buffer);
            i=0;
            printf("> root@Luoshan_OS: ");
        }else if( c==8 || c==127 ){      // note: backspace maybe 8('\b') or 127(delete)
            i--;
        }else{
            buffer[i++]=c;
            printf("%c",c);
        }
    }
}
