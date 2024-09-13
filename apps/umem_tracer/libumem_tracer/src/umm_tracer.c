/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include "libunwind.h"
#include "umem_tracer.h"

/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define SKIP_STACK						(3)
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
UMEM_TRACER *g_tracer;
DBGLVL g_dbglvl;
static void* (*real_malloc)(size_t) = NULL;
static void (*real_free)(void*) = NULL;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
extern void unix_sock_server_init(void);
extern void unix_sock_server_exit(void);
extern void save_data_to_file(void);

static void get_cmdline(int pid, int len, char *cmdline)
{
	int fd;
	ssize_t ret;
	char path[MAX_PATH_LEN] = {0};
	snprintf(path, MAX_PATH_LEN, "/proc/%d/cmdline", pid);
	fd = open(path, O_RDONLY);
	if (-1 == fd)
	{
		UMT_LOG(ERROR, "open %s failed\n", path);
		strncpy(cmdline, "?????", len);
	}
	ret = read(fd, cmdline, len);
	if (-1 == ret)
	{
		UMT_LOG(ERROR, "read %s failed\n", path);
		strncpy(cmdline, "?????", len);
	}
	if (fd > 0)
	{
		close(fd);
	}
	return;
}

static int save_stacktrace(int skip, unsigned long *trace) 
{
    unw_cursor_t cursor;
    unw_context_t context;
    unw_word_t ip;
    int ret;
	int trace_len;

    // Initialize the cursor to current frame for local unwinding
    if (0 != unw_getcontext(&context))
    {
        UMT_LOG(ERROR, "unw_getcontext failed\n");
        return 0;
    }
    if (0 != unw_init_local(&cursor, &context))
    {
        UMT_LOG(ERROR, "unw_init_local failed\n");
        return 0;
    }

    UMT_LOG(DEBUG, "Call stack:\n");
	trace_len = 0;
    do
    {
        if (0 != unw_get_reg(&cursor, UNW_REG_IP, &ip))
        {
            UMT_LOG(ERROR, "unw_get_reg failed\n");
			goto out;
        }
		if (0 < skip)
		{
			skip--;
		}
		else
		{
			trace[trace_len++] = (unsigned long)ip;
		}

        ret = unw_step(&cursor);
        if (ret < 0)
        {
            UMT_LOG(ERROR, "unw_step failed\n");
			goto out;
        }
    } while (ret > 0 && trace_len < MAX_TRACE);

out:
	return trace_len;
}

static unsigned long cal_hkey(unsigned long *trace, unsigned int trace_len)
{
	unsigned long hkey = 0;
	int i = 0;
	for (i = 0; i < trace_len; i++)
	{
		hkey += trace[i];
	}
	return hkey;
}

#define find_stacktrace(type, htable, trace, hkey, stacktrace) \
do \
{ \
	type *tmpstack = NULL; \
    hash_for_each_possible(htable, tmpstack, hnode, hkey) \
	{ \
        if (0 == memcmp(tmpstack->trace, trace, MAX_TRACE)) \
        { \
			stacktrace = tmpstack; \
			break; \
        } \
	} \
} while (0);

#define destroy_stacktrace_table(type, htable) \
do \
{ \
	type *stacktrace = NULL; \
	struct hlist_node *tmp_hnode = NULL; \
	int bkt; \
	hash_for_each_safe(htable, bkt, tmp_hnode, stacktrace, hnode) \
	{ \
		if (NULL != stacktrace) \
		{ \
			hash_del(&stacktrace->hnode); \
			real_free(stacktrace); \
		} \
	} \
} while (0);

#define find_traceobj(htable, obj, traceobj) \
do \
{ \
	TRACEOBJ *tmpobj = NULL; \
	unsigned long hkey = (unsigned long)obj; \
	hash_for_each_possible(g_tracer->traceobj_table, tmpobj, hnode, hkey) \
	{ \
		if (tmpobj->addr == (unsigned long)obj) \
		{ \
			traceobj = tmpobj; \
			break; \
		} \
	} \
} while (0);

#define destroy_traceobj_table(htable) \
do \
{ \
    TRACEOBJ *traceobj = NULL; \
    struct hlist_node *tmp_hnode = NULL; \
    int bkt; \
    hash_for_each_safe(htable, bkt, tmp_hnode, traceobj, hnode) \
    { \
        if (NULL != traceobj) \
        { \
            hash_del(&traceobj->hnode); \
            real_free(traceobj); \
        } \
    } \
} while (0);

static void tracer_init(void)
{
	real_malloc = dlsym(RTLD_NEXT, "malloc");
	if (NULL == real_malloc)
	{
		UMT_LOG(FATAL, "dlsym malloc failed\n");
		exit(EXIT_FAILURE);
	}
	real_free = dlsym(RTLD_NEXT, "free");
	if (NULL == real_free)
	{
		UMT_LOG(FATAL, "dlsym free failed\n");
		exit(EXIT_FAILURE);
	}
	g_tracer = (UMEM_TRACER *)real_malloc(sizeof(UMEM_TRACER)); 
	if (NULL == g_tracer)
	{
		UMT_LOG(FATAL, "malloc g_tracer failed\n");
		exit(EXIT_FAILURE);
	}
	memset(g_tracer, 0, sizeof(UMEM_TRACER));
	pthread_mutex_init(&g_tracer->mutex, NULL);
	hash_init(g_tracer->traceobj_table);
	hash_init(g_tracer->allocstack_table);
	g_tracer->pid = getpid();
	get_cmdline(g_tracer->pid, MAX_CMDLINE_LEN, g_tracer->cmdline);
	return;
}

static void tracer_exit(void)
{
	if (g_tracer->hold_allocstack > 0)
	{
		save_data_to_file();
	}
	return;	
}

static void trace_alloc(const void *obj, size_t size)
{
	unsigned long trace[MAX_TRACE] = {0};
	unsigned int trace_len = 0;
	unsigned long hkey = 0;
	TRACEOBJ *traceobj = NULL;
	ALLOCSTACK *allocstack = NULL;
	if (false == g_tracer->initilized_flags)
	{
		return;
	}
	traceobj = real_malloc(sizeof(TRACEOBJ));
	memset(traceobj, 0, sizeof(TRACEOBJ));
	traceobj->addr = (unsigned long)obj;
	traceobj->size = size;
	trace_len = save_stacktrace(SKIP_STACK, trace);
	hkey = cal_hkey(trace, trace_len);
	pthread_mutex_lock(&g_tracer->mutex);
	find_stacktrace(ALLOCSTACK, g_tracer->allocstack_table, trace, hkey, allocstack);
	if (NULL == allocstack)
	{
		allocstack = real_malloc(sizeof(ALLOCSTACK));
		memset(allocstack, 0, sizeof(ALLOCSTACK));
		allocstack->trace_len = trace_len;
		memcpy(allocstack->trace, trace, sizeof(trace));
		hash_add(g_tracer->allocstack_table, &allocstack->hnode, hkey);
		hash_init(allocstack->freestack);
		g_tracer->hold_allocstack++;
	}
	traceobj->allocstack = allocstack;
	allocstack->hold++;
	allocstack->total_alloc++;
	allocstack->hold_size += size;
	g_tracer->hold_object++;
	g_tracer->hold_object_size += size;
	hash_add(g_tracer->traceobj_table, &traceobj->hnode, traceobj->addr);
	pthread_mutex_unlock(&g_tracer->mutex);
	return;
}

static void trace_free(const void *obj)
{
	TRACEOBJ *traceobj = NULL;
	ALLOCSTACK *allocstack = NULL;
	FREESTACK *freestack = NULL;
	unsigned long trace[MAX_TRACE] = {0};
	unsigned int trace_len = 0;
	unsigned long hkey = 0;
	if (false == g_tracer->initilized_flags)
	{
		return;
	}
	find_traceobj(g_tracer->traceobj_table, obj, traceobj);
	if (NULL == traceobj)
	{
		return;
	}
	allocstack = traceobj->allocstack;
	pthread_mutex_lock(&g_tracer->mutex);
	allocstack->hold--;
	allocstack->hold_size -= traceobj->size;
	g_tracer->hold_object--;
	g_tracer->hold_object_size -= traceobj->size;
	hash_del(&traceobj->hnode);
	real_free(traceobj);
	if (0 == allocstack->hold)
	{
		hash_del(&allocstack->hnode);
		destroy_stacktrace_table(FREESTACK, allocstack->freestack);
		real_free(allocstack);
		g_tracer->hold_allocstack--;
	}
	else
	{
		trace_len = save_stacktrace(SKIP_STACK, trace);
		hkey = cal_hkey(trace, trace_len);
		find_stacktrace(FREESTACK, allocstack->freestack, trace, hkey, freestack);
		if (NULL == freestack)
		{
			freestack = real_malloc(sizeof(FREESTACK));
			memset(freestack, 0, sizeof(FREESTACK));
			freestack->trace_len = trace_len;
			memcpy(freestack->trace, trace, sizeof(trace));
			hash_add(allocstack->freestack, &freestack->hnode, hkey);
		}
		freestack->total_free++;
	}
	pthread_mutex_unlock(&g_tracer->mutex);
	return;
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
void* malloc(size_t size)
{
	UMT_LOG(DEBUG, "Enter my malloc\n");
    if (!real_malloc) {
        real_malloc = dlsym(RTLD_NEXT, "malloc");
    }
    void* ptr = real_malloc(size);
    UMT_LOG(DEBUG, "malloc(%zu) = %p\n", size, ptr);
	trace_alloc(ptr, size);
    return ptr;
}

void free(void* ptr)
{
	UMT_LOG(DEBUG, "Enter my free\n");
    if (!real_free) {
        real_free = dlsym(RTLD_NEXT, "free");
    }
    UMT_LOG(DEBUG, "free(%p)\n", ptr);
	trace_free(ptr);
    real_free(ptr);
	return;
}

__attribute__((constructor)) void pre_init()
{
	g_dbglvl = ERROR;
	tracer_init();
	unix_sock_server_init();
	g_tracer->initilized_flags = true;
}
__attribute__((destructor)) void post_exit()
{
	unix_sock_server_exit();
	tracer_exit();
}
