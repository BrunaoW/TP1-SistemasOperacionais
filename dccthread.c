#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dccthread.h"
#include "dlist.h"

#define TRUE 1
#define FALSE 0

typedef unsigned char uint8_t;

typedef struct dccthread
{
    ucontext_t state;
    char name[DCCTHREAD_MAX_NAME_SIZE];
    uint8_t is_yielded;
    
    dccthread_t* queued_thread;
    uint8_t is_ready;
    uint8_t is_waiting;
} dccthread_t;

// Threads gerente e principal
dccthread_t* manager_thread;
dccthread_t* main_thread;

// Lista de threads prontas a serem executadas
struct dlist* ready_threads_list;
struct dlist* waiting_threads_list;

void dccthread_init(void (*func)(int), int param) {
    // inicializa listas de threads 
    ready_threads_list = dlist_create();
    waiting_threads_list = dlist_create();

    // Inicializa thread gerente
    manager_thread = (dccthread_t*) malloc(sizeof(dccthread_t));
    getcontext(&manager_thread->state);
    strcpy(manager_thread->name, "manager");
    manager_thread->is_yielded = FALSE;
    manager_thread->queued_thread = NULL;
    manager_thread->state.uc_link = NULL;
    manager_thread->state.uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);
	manager_thread->state.uc_stack.ss_size = THREAD_STACK_SIZE;
	manager_thread->state.uc_stack.ss_flags = 0;
    
    // Inicializa thread principal
    main_thread = dccthread_create("main", func, param);

    // Executa threads prontas para serem executadas
    while (!dlist_empty(ready_threads_list))
    {
        dccthread_t* current_ready_thread = (dccthread_t*) ready_threads_list->head->data;
		swapcontext(&manager_thread->state, &current_ready_thread->state);
        dlist_pop_left(ready_threads_list);
        if (current_ready_thread->is_yielded) {
            current_ready_thread->is_ready = FALSE;
        }
    }

    exit(EXIT_SUCCESS);
}

dccthread_t* dccthread_create(const char *name, void (*func)(int), int param) {
    // Inicializa uma nova thread
    dccthread_t* new_thread; 
    new_thread = (dccthread_t*) malloc(sizeof(dccthread_t));
    getcontext(&new_thread->state);
    strcpy(new_thread->name, name);
    new_thread->queued_thread = NULL;

    new_thread->state.uc_link = &manager_thread->state;
    new_thread->state.uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);
	new_thread->state.uc_stack.ss_size = THREAD_STACK_SIZE;
	new_thread->state.uc_stack.ss_flags = 0;
    new_thread->is_ready = TRUE;
    new_thread->is_waiting = FALSE;
    new_thread->is_yielded = FALSE;

    dlist_push_right(ready_threads_list, new_thread);
	makecontext(&new_thread->state, (void*) func, 1, param);

    return new_thread;
}

void dccthread_yield(void) {
    dccthread_t* current_thread = dccthread_self();
    current_thread->is_yielded = TRUE;
    dlist_push_right(ready_threads_list, current_thread);
    swapcontext(&current_thread->state, &manager_thread->state);
}

dccthread_t* dccthread_self(void) {
    return ready_threads_list->head->data;
}

const char* dccthread_name(dccthread_t* tid) {
    return tid->name;
}

int cmp_waiting_threads(const void *e1, const void *e2, void *userdata) {
    dccthread_t* thread_item = (dccthread_t*) e1;
    dccthread_t* thread_about_to_leave = (dccthread_t*) e2;
    if (thread_about_to_leave == thread_item->queued_thread) {
        return 0;
    } else {
        return 1;
    }
}

void dccthread_exit(void) {
    // remover thread atual da lista de threads prontas
    // Verificar se há uma thread aguardando sua finalização
    // Se sim, Setar o contexto da thread a ela
    // Se não, Setar o contexto da thread manager

    dccthread_t* current_thread = dccthread_self();
    dccthread_t* queued_thread = (dccthread_t*) dlist_find_remove(waiting_threads_list, current_thread, cmp_waiting_threads, NULL);
    if (queued_thread != NULL) {
        queued_thread->is_ready = 1;
        queued_thread->is_waiting = 0;
        dlist_push_right(ready_threads_list, queued_thread);
    }

    setcontext(&manager_thread->state);
}

void dccthread_wait(dccthread_t *tid) {
    // Pega a thread atual
    // Seta a thread a esperar execução na variavel queued_thread
    // Troca o contexto da thread sendo executada pela

    dccthread_t* current_thread = dccthread_self();
    if (tid->is_ready || tid->is_waiting) {
        current_thread->is_ready = 0;
        current_thread->is_waiting = 1;
        current_thread->queued_thread = tid;
        dlist_push_right(waiting_threads_list, current_thread);
        swapcontext(&current_thread->state, &manager_thread->state);
    }
}

void dccthread_sleep(struct timespec ts) {

}