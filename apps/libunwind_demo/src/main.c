/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#define _GNU_SOURCE
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
	int ret;

    // Initialize the cursor to current frame for local unwinding
    if (0 != unw_getcontext(&context))
	{
		printf("unw_getcontext failed\n");
		return;
	}
    if (0 != unw_init_local(&cursor, &context))
	{
		printf("unw_init_local failed\n");
		return;
	}

    printf("Call stack:\n");
    do
	{
        if (0 != unw_get_reg(&cursor, UNW_REG_IP, &ip))
		{
			printf("unw_get_reg failed\n");
			return;
		}
        if (0 != unw_get_reg(&cursor, UNW_REG_SP, &sp))
		{
			printf("unw_get_reg failed\n");
			return;
		}
        if (0 != unw_get_proc_name(&cursor, func_name, sizeof(func_name), &offset))
		{
			snprintf(func_name, sizeof(func_name), "%s", "??????");
			offset = 0;
		}
        printf("ip = 0x%lx, sp = 0x%lx, function = %s+0x%lx\n", (long)ip, (long)sp, func_name, (long)offset);

		ret = unw_step(&cursor);
		if (ret < 0)
		{
			printf("unw_step failed\n");
		}
    } while (ret > 0);
}

void foo() {
    print_stacktrace();
}

void bar() {
    foo();
}

int main() {
    bar();
    return 0;
}
