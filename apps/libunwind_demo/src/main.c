/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libunwind.h"

/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
void print_stacktrace() {
    unw_cursor_t cursor;
    unw_context_t context;
    unw_word_t ip, sp;
    char func_name[256];
    unw_word_t offset;

    // Initialize the cursor to current frame for local unwinding
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    printf("Call stack:\n");
    while (unw_step(&cursor) > 0) {
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        unw_get_proc_name(&cursor, func_name, sizeof(func_name), &offset);

        printf("ip = 0x%lx, sp = 0x%lx, function = %s+0x%lx\n", (long)ip, (long)sp, func_name, (long)offset);
    }
}

void foo() {
    print_stacktrace();
}

void bar() {
    foo();
}

int main() {
    bar();
	while(1)
	{
		sleep(1);
	}
    return 0;
}
