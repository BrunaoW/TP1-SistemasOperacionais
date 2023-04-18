#include <ucontext.h>

#include "dccthread.h"
#include "dlist.h"

#define TRUE 1
#define FALSE 0

// TAD info da thread
typedef struct 
{
    ucontext_t state;
    char name[DCCTHREAD_MAX_NAME_SIZE];
} dccthread_t;
