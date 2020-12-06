#include <os/sched.h>
#include <screen.h>
#include <stdio.h>

static char blank[]  = {"                   "};
static char plane1[] = {"    ___         _  "};
static char plane2[] = {"| __\\_\\______/_| "};
static char plane3[] = {"<[___\\_\\_______| "};
static char plane4[] = {"|  o'o             "};

int test_score_fly()
{
    int i = 22, j = 10;

    while (1) {
        for (i = 60; i > 0; i--) {
            /* move */
            vt100_move_cursor(i, j + 0);
            printk("%s", plane1);

            vt100_move_cursor(i, j + 1);
            printk("%s", plane2);

            vt100_move_cursor(i, j + 2);
            printk("%s", plane3);

            vt100_move_cursor(i, j + 3);
            printk("%s", plane4);
        }
        do_scheduler();

        vt100_move_cursor(1, j + 0);
        printk("%s", blank);

        vt100_move_cursor(1, j + 1);
        printk("%s", blank);

        vt100_move_cursor(1, j + 2);
        printk("%s", blank);

        vt100_move_cursor(1, j + 3);
        printk("%s", blank);
        do_scheduler();
    }

    // exit the program
    do_exit();
}

static long int score_atol(const char* str)
{
    int base = 10;
    if ((str[0] == '0' && str[1] == 'x') ||
        (str[0] == '0' && str[1] == 'X')) {
        base = 16;
        str += 2;
    }
    long ret = 0;
    while (*str != '\0') {
        if ('0' <= *str && *str <= '9') {
            ret = ret * base + (*str - '0');
        } else if (base == 16) {
            if ('a' <= *str && *str <= 'f'){
                ret = ret * base + (*str - 'a');
            } else if ('A' <= *str && *str <= 'F') {
                ret = ret * base + (*str - 'A');
            } else {
                return 0;
            }
        } else {
            return 0;
        }
        ++str;
    }
    return ret;
}

int test_score_rw(int argc, char* argv[])
{
    int r0 = 0x42;
    long mem2      = 0;
    uintptr_t mem1 = 0;
    int curs       = 0;
    int i;
    vt100_move_cursor(2, 2);
    // printk("argc = %d\n", argc);
    // for (i = 0; i < argc; ++i) {
    //  printk("argv[%d] = %s\n", i, argv[i]);
    // }
    for (i = 1; i < argc; i++) {
        mem1 = score_atol(argv[i]);
        // vt100_move_cursor(2, curs+i);
        // mem2         = rand();
        r0 = (0x5deece66dll * r0 + 0xbll) & 0x7fffffff;
        mem2 = r0;
        *(long*)mem1 = mem2;
        printk("0x%lx, %ld\n", mem1, mem2);
        if (*(long*)mem1 != mem2) {
            printk("Error!\n");
        }
        do_scheduler();
    }
    // Only input address.
    // Achieving input r/w command is recommended but not required.
    printk("Success!\n");
    // while(1);
    return 0;
}
