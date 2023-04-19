#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dccthread.h"
#include "dlist.h"

#define TRUE 1
#define FALSE 0

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
    strcpy(manager_thread->name, "manager");

    manager_thread->state.uc_link = NULL;
    manager_thread->state.uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);
	manager_thread->state.uc_stack.ss_size = THREAD_STACK_SIZE;
	manager_thread->state.uc_stack.ss_flags = 0;
    
    // Inicializa thread principal
    main_thread = dccthread_create("main", func, param);

    // Executa threads prontas para serem executadas
    while (!dlist_empty(ready_threads_list))
    {
		swapcontext(&manager_thread->state, &((dccthread_t*) ready_threads_list->head->data)->state);
		dlist_pop_left(ready_threads_list);
    }

    exit(EXIT_SUCCESS);
}

dccthread_t* dccthread_create(const char *name, void (*func)(int), int param) {
    // Inicializa uma nova thread
    dccthread_t* new_thread; 
    new_thread = (dccthread_t*) malloc(sizeof(dccthread_t));
    getcontext(&new_thread->state);
    strcpy(new_thread->name, name);

    new_thread->state.uc_link = &manager_thread->state;
    new_thread->state.uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);
	new_thread->state.uc_stack.ss_size = THREAD_STACK_SIZE;
	new_thread->state.uc_stack.ss_flags = 0;

    dlist_push_right(ready_threads_list, new_thread);
	makecontext(&new_thread->state, (void*) func, 1, param);

    return new_thread;
}

void dccthread_yield(void) {
    dccthread_t* current_thread = dccthread_self();
    dlist_push_right(ready_threads_list, current_thread);
    swapcontext(&current_thread->state, &manager_thread->state);
}

dccthread_t* dccthread_self(void) {
    return ready_threads_list->head->data;
}

const char* dccthread_name(dccthread_t* tid) {
    return tid->name;
}

void dccthread_exit(void) {

}

void dccthread_wait(dccthread_t *tid) {

}

void dccthread_sleep(struct timespec ts) {

}