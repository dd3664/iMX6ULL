#ifndef __UMEM_TRACER_H_
#define __UMEM_TRACER_H_

/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <pthread.h>
#include "list.h"
#include "list_sort.h"
#include "hashtable.h"

/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define UMT_LOG(lvl, fmt, ...) do \
{ \
	if(lvl >= g_dbglvl) \
	{ \
		printf("[UMT][%s:%d]"fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} \
} while(0);

#define MAX_TRACE                     16
#define MAX_BUFER_LEN                 8
#define TRACEOBJ_HTABLE_SIZE          12   /* 1 << TRACEOBJ_HTABLE_SIZE */
#define ALLOCSTACK_HTABLE_SIZE        12   /* 1 << ALLOCSTACK_HTABLE_SIZE */
#define EACH_FREESTACK_HTABLE_SIZE    3    /* 1 << EACH_FREESTACK_HTABLE_SIZE */
#define SERVER_UNIX_PATH              "/tmp/umem_trace_sock/"
#define DATA_FILE_PATH                 "/tmp/umem_trace_data/"
#define MAX_PATH_LEN           		  32
#define MAX_CMDLINE_LEN				  32

typedef struct __alloc_stack {
    unsigned long trace[MAX_TRACE];
    unsigned int trace_len;
    unsigned int hold;
    unsigned int total_alloc;
    unsigned int hold_size;
    struct list_head list;
    struct hlist_node hnode;
    DECLARE_HASHTABLE(freestack, EACH_FREESTACK_HTABLE_SIZE);
} ALLOCSTACK;

typedef struct __free_stack {
    unsigned long trace[MAX_TRACE];
    unsigned int trace_len;
    unsigned int total_free;
    struct hlist_node hnode;
} FREESTACK;

typedef struct __trace_obj {
    unsigned long addr;
    unsigned int size;
    ALLOCSTACK *allocstack;
    struct hlist_node hnode;
} TRACEOBJ;

typedef struct __allocstack_queue {
    struct list_head list;
    unsigned int qlen;
} ALLOCSTACK_QUEUE;

typedef enum
{
	DEBUG,
	INFO,
	WARN,
	ERROR,
	FATAL,
	OFF
} DBGLVL;

typedef struct __umem_tracer {
    pthread_mutex_t mutex;
    DECLARE_HASHTABLE(traceobj_table, TRACEOBJ_HTABLE_SIZE);
    DECLARE_HASHTABLE(allocstack_table, ALLOCSTACK_HTABLE_SIZE);
    ALLOCSTACK_QUEUE allocstack_queue;
    unsigned int hold_allocstack;
    unsigned int hold_object;
    unsigned long hold_object_size;
	int pid;
	char cmdline[MAX_CMDLINE_LEN];
	int server_sock;
	char server_path[MAX_PATH_LEN];
    bool initilized_flags;
} UMEM_TRACER;

/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
extern DBGLVL g_dbglvl;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/

#endif /* __UMEM_TRACER_H_ */
