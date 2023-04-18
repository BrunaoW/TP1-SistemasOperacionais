#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>

#include "dccthread.h"
#include "dlist.h"

#define TRUE 1
#define FALSE 0

// TAD info da thread
typedef struct dccthread
{
    ucontext_t state;
    char name[DCCTHREAD_MAX_NAME_SIZE];
} dccthread_t;

// Threads gerente e principal
dccthread_t* manager_thread;
dccthread_t* main_thread;

struct dlist* ready_threads_list;

void dccthread_init(void (*func)(int), int param) {
    // inicializa lista de threads prontas a serem executadas
    ready_threads_list = dlist_create();

    // Inicializa thread gerente
    manager_thread = (dccthread_t*) malloc(sizeof(dccthread_t));
    getcontext(&manager_thread->state);
    strcpy(manager_thread->name, "thread_gerente");
    
    // Inicializa thread principal
    main_thread = dccthread_create("thread_principal", func, param);
}

dccthread_t* dccthread_create(const char *name, void (*func)(int), int param) {
    // Inicializa uma nova thread
    dccthread_t* new_thread = (dccthread_t*) malloc(sizeof(dccthread_t));
    getcontext(&new_thread->state);
    strcpy(new_thread->name, name);

    dlist_push_right(ready_threads_list, new_thread);

	makecontext(&new_thread->state, (void*) func, 1, param);
    return new_thread;
}