#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#include "dccthread.h"
#include "dlist.h"

#define TRUE 1
#define FALSE 0

typedef unsigned char bool;

typedef struct dccthread
{
    ucontext_t context;
    char name[DCCTHREAD_MAX_NAME_SIZE];
    dccthread_t* waiting_thread;
} dccthread_t;

// Threads gerente e principal
ucontext_t manager_thread_context;
dccthread_t* main_thread;

// Lista de threads prontas a serem executadas
struct dlist* ready_threads_list;
struct dlist* waiting_threads_list;

// variaveis para controle de preempção
timer_t timer;
struct sigevent signal_event;
struct sigaction signal_action;
struct itimerspec time_spent;

static void stop_thread(int sig){
	dccthread_yield();
}

void init_timer() {
    signal_action.sa_handler = stop_thread;
    sigemptyset(&signal_action.sa_mask);
    signal_action.sa_flags = 0;
    sigaction(SIGUSR1, &signal_action, NULL);

    signal_event.sigev_notify = SIGEV_SIGNAL;
    signal_event.sigev_signo = SIGUSR1;
    signal_event.sigev_value.sival_ptr = &timer;

    time_spent.it_value.tv_sec = 0;
    time_spent.it_value.tv_nsec = 10000000;
    time_spent.it_interval.tv_sec = 0;
    time_spent.it_interval.tv_nsec = 0;

    timer_create(CLOCK_PROCESS_CPUTIME_ID, &signal_event, &timer);
    timer_settime(timer, 0, &time_spent, NULL);
}

bool find_thread_in_list(struct dlist* thread_list, dccthread_t* thread) {
    if (thread_list->count == 0) return FALSE;

    struct dnode* current_thread_item = thread_list->head;
    while (current_thread_item != NULL && (dccthread_t*)current_thread_item->data != thread)
    {
        current_thread_item = current_thread_item->next;
    }
    
    if (current_thread_item != NULL) {
        return TRUE;
    } else {
        return FALSE;
    }
}

void dccthread_init(void (*func)(int), int param) {
    // inicializa listas de threads 
    ready_threads_list = dlist_create();
    waiting_threads_list = dlist_create();

    // Inicializa thread gerente
    getcontext(&manager_thread_context);
    
    // Inicializa thread principal
    main_thread = dccthread_create("main", func, param);

    // Executa threads prontas para serem executadas
    while (!dlist_empty(ready_threads_list) || !dlist_empty(waiting_threads_list))
    {
        init_timer();
        
        dccthread_t* current_ready_thread = (dccthread_t*) ready_threads_list->head->data;
        dccthread_t* waiting_thread = current_ready_thread->waiting_thread;
        
        // verifica se está esperando por uma thread
        if (waiting_thread != NULL) {
            if (find_thread_in_list(ready_threads_list, waiting_thread)) {
                dlist_push_right(ready_threads_list, current_ready_thread);
                continue;
            } else {
                current_ready_thread->waiting_thread = NULL;
            }
        }

    	timer_settime(timer, 0, &time_spent, NULL);
		swapcontext(&manager_thread_context, &current_ready_thread->context);
        timer_delete(&timer);
        dlist_pop_left(ready_threads_list);
    }

    exit(EXIT_SUCCESS);
}

dccthread_t* dccthread_create(const char *name, void (*func)(int), int param) {
    // Inicializa uma nova thread
    dccthread_t* new_thread; 
    new_thread = (dccthread_t*) malloc(sizeof(dccthread_t));
    getcontext(&new_thread->context);
    strcpy(new_thread->name, name);
    new_thread->waiting_thread = NULL;

    // aloca espaço para stack
    new_thread->context.uc_link = &manager_thread_context;
    new_thread->context.uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);
	new_thread->context.uc_stack.ss_size = THREAD_STACK_SIZE;
	new_thread->context.uc_stack.ss_flags = 0;

    // adiciona na lista de threads prontas e chama makecontext()
    dlist_push_right(ready_threads_list, new_thread);
	makecontext(&new_thread->context, (void*) func, 1, param);

    return new_thread;
}

void dccthread_yield(void) {
    dccthread_t* current_thread = dccthread_self();
    dlist_push_right(ready_threads_list, current_thread);
    swapcontext(&current_thread->context, &manager_thread_context);
}

dccthread_t* dccthread_self(void) {
    return ready_threads_list->head->data;
}

const char* dccthread_name(dccthread_t* tid) {
    return tid->name;
}

void dccthread_exit(void) {
    // remover thread atual da lista de threads prontas
    // Setar o contexto da thread manager

    dccthread_t* current_thread = dccthread_self();
    free(current_thread);
    setcontext(&manager_thread_context);
}

void dccthread_wait(dccthread_t *tid) {
    // Pega a thread atual
    // Seta a thread a esperar execução na variavel waiting_thread
    // Troca o contexto da thread sendo executada pelo contexto da manager

    dccthread_t* current_thread = dccthread_self();
    current_thread->waiting_thread = tid;
    dlist_push_right(ready_threads_list, current_thread);
    swapcontext(&current_thread->context, &manager_thread_context);
}

void dccthread_sleep(struct timespec ts) {

}