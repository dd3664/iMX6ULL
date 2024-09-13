/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <time.h>
#include "list.h"
#include "list_sort.h"
#include "hashtable.h"
#include "umem_tracer.h"

/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define MAX_FILE_NAME_LEN					(64)
#define MAX_SHOW_ALLOCSTACK_CNT				(1024)
#define MAX_TIME_LEN						(24)
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
extern UMEM_TRACER *g_tracer;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
static int descending_by_hold(void *priv, struct list_head *a, struct list_head *b)
{
	ALLOCSTACK *allocstack_a = list_entry(a, ALLOCSTACK, list);
	ALLOCSTACK *allocstack_b = list_entry(b, ALLOCSTACK, list);
	return (allocstack_b->hold - allocstack_a->hold);
}

static void replace_char(char *s_str, int len, char src, char dst)
{
	int i = 0;
	for (i = 0; i < len; i++)
	{
		if (s_str[i] == src)
		{
			s_str[i] = dst;
		}
	}
	return;
}

static void __show_freestack_info(int fd, FREESTACK *freestack)
{
	Dl_info dlinfo;
	int i;

	if (-1 == fd)
	{
		UMT_LOG(ERROR, "valid fd\n");
		return;
	}
	dprintf(fd, "free counts=%u\n", freestack->total_free);
	for(i = 0; i < freestack->trace_len; i++)
	{
		if (dladdr((void *)freestack->trace[i], &dlinfo) && dlinfo.dli_fname)
		{
			dprintf(fd, "ip=0x%-16lx dli_fbase=0x%-16lx dli_offset=0x%-16lx dli_fname=%-32s\n", (long)freestack->trace[i], (long)dlinfo.dli_fbase, (long)freestack->trace[i] - (long)dlinfo.dli_fbase, dlinfo.dli_fname);
		}
		else
		{
			dprintf(fd, "ip=0x%-16lx\n", (long)freestack->trace[i]);
		}
	}
	dprintf(fd, "\n");
	return;
}

static void __show_allocstack_info(int fd, ALLOCSTACK *allocstack)
{
	Dl_info dlinfo;
	FREESTACK *freestack = NULL;
	struct hlist_node *tmp_hnode = NULL;
	int bkt;
	int i;
	
	if (-1 == fd)
	{
		UMT_LOG(ERROR, "invalid fd\n");
		return;
	}

	dprintf(fd, "------------------------------------------------------------------------------------------------------------------\n");
	dprintf(fd, "hold counts=%u, hold size=%uB, total alloc counts=%u\n", allocstack->hold, allocstack->hold_size,  allocstack->total_alloc);
	for (i = 0; i < allocstack->trace_len; i++)
	{
		if (dladdr((void *)allocstack->trace[i], &dlinfo) && dlinfo.dli_fname)
		{
			dprintf(fd, "ip=0x%-16lx dli_fbase=0x%-16lx dli_offset=0x%-16lx dli_fname=%-32s\n", (long)allocstack->trace[i], (long)dlinfo.dli_fbase, (long)allocstack->trace[i] - (long)dlinfo.dli_fbase, dlinfo.dli_fname);
		}
		else
		{
			dprintf(fd, "ip=0x%-16lx\n", (long)allocstack->trace[i]);
		}
	}
	dprintf(fd, "\n");

	hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, freestack, hnode)
	{
		if (NULL != freestack)
		{
			__show_freestack_info(fd, freestack);
		}
	}
	return;
}

void save_data_to_file(void)
{
	char fname[MAX_FILE_NAME_LEN] = {0};
	char time_s[MAX_TIME_LEN] = {0};
	char cmdline[MAX_CMDLINE_LEN] = {0};
	time_t tt;
	struct tm *ptm;
	int fd;
	ALLOCSTACK *allocstack = NULL;
	ALLOCSTACK *tmpstack = NULL;
	struct hlist_node *tmp_hnode = NULL;
	int bkt;

	tt = time(NULL);
	ptm = localtime(&tt);
	strftime(time_s, MAX_TIME_LEN, "%Y-%m-%d %H:%M:%S", ptm);

	strncpy(cmdline, g_tracer->cmdline, MAX_CMDLINE_LEN);
	replace_char(cmdline, strlen(cmdline), '/', '-');
	snprintf(fname, MAX_FILE_NAME_LEN, "%s%d-%s", DATA_FILE_PATH, g_tracer->pid, cmdline);
	if (0 > access(DATA_FILE_PATH, F_OK))
	{
		if (0 > mkdir(DATA_FILE_PATH, 0755))
		{
			UMT_LOG(ERROR, "create data file path failed\n");
			return;
		}
	}
	fd = open(fname, O_WRONLY | O_CREAT | O_APPEND);
	if (-1 == fd)
	{
		UMT_LOG(ERROR, "open %s failed\n", fname);
		return;
	}

	pthread_mutex_lock(&g_tracer->mutex);
	INIT_LIST_HEAD(&g_tracer->allocstack_queue.list);
	hash_for_each_safe(g_tracer->allocstack_table, bkt, tmp_hnode, allocstack, hnode)
	{
		if (NULL != allocstack && g_tracer->allocstack_queue.qlen < MAX_SHOW_ALLOCSTACK_CNT)
		{
			list_add_tail(&allocstack->list, &g_tracer->allocstack_queue.list);
			g_tracer->allocstack_queue.qlen++;
		}
	}
	list_sort(NULL, &g_tracer->allocstack_queue.list, descending_by_hold);
	dprintf(fd, "=====================================================ALLOC STACK %s=====================================================\n", time_s);
	dprintf(fd, "hold object=%u, hold object size=%luB, hold allocstack=%u\n", g_tracer->hold_object, g_tracer->hold_object_size, g_tracer->hold_allocstack);
	list_for_each_entry_safe(allocstack, tmpstack, &g_tracer->allocstack_queue.list, list)
	{
		if (NULL != allocstack)
		{
			__show_allocstack_info(fd, allocstack);
			list_del(&allocstack->list);
			g_tracer->allocstack_queue.qlen--;
		}
	}
	pthread_mutex_unlock(&g_tracer->mutex);
	close(fd);
	return;
}

