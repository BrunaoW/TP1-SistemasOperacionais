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
sigset_t preemption_mask;

// variaveis de suporte a espera
timer_t sleep_timer;
struct sigevent sleep_signal_event;
struct sigaction sleep_signal_action;
struct itimerspec sleep_time_spent;
sigset_t sleep_mask;

static void stop_thread(int signal){
	dccthread_yield();
}

void init_timer() {
	signal_action.sa_handler = stop_thread;
    signal_action.sa_flags = 0;
	sigaction(SIGUSR1, &signal_action, NULL);

	signal_event.sigev_signo = SIGUSR1;
	signal_event.sigev_notify = SIGEV_SIGNAL;
	signal_event.sigev_value.sival_ptr = &timer;
	timer_create(CLOCK_PROCESS_CPUTIME_ID, &signal_event, &timer);

	time_spent.it_value.tv_sec = 0;
    time_spent.it_interval.tv_sec = 0;
	time_spent.it_value.tv_nsec = 10000000;
    time_spent.it_interval.tv_nsec = 10000000;
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

    // Inicializa thread principal
    dccthread_create("main", func, param);
    
    // Inicializa thread gerente
    getcontext(&manager_thread_context);

    sigemptyset(&preemption_mask);
    sigaddset(&preemption_mask, SIGUSR1);
	sigprocmask(SIG_SETMASK, &preemption_mask, NULL);

    sigemptyset(&sleep_mask);
    sigaddset(&sleep_mask, SIGUSR2);

	manager_thread_context.uc_sigmask = preemption_mask;
    init_timer();

    // Executa threads prontas para serem executadas
    while (!dlist_empty(ready_threads_list) || !dlist_empty(waiting_threads_list))
    {
        // aciona sinal para detectar se há threads aguardando execução
        sigprocmask(SIG_UNBLOCK, &sleep_mask, NULL);
		sigprocmask(SIG_BLOCK, &sleep_mask, NULL);

		main_thread = (dccthread_t *) dlist_pop_left(ready_threads_list);
        dccthread_t* waiting_thread = main_thread->waiting_thread;
        
        // verifica se está esperando por uma thread
        if (waiting_thread != NULL) {
            if (find_thread_in_list(ready_threads_list, waiting_thread) 
                || find_thread_in_list(waiting_threads_list, waiting_thread)) {
                dlist_push_right(ready_threads_list, main_thread);
                continue;
            } else {
                main_thread->waiting_thread = NULL;
            }
        }

		swapcontext(&manager_thread_context, &main_thread->context);
    }

    exit(EXIT_SUCCESS);
}

dccthread_t* dccthread_create(const char *name, void (*func)(int), int param) {
    sigprocmask(SIG_BLOCK, &preemption_mask, NULL);

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

    // configura, adiciona na lista de threads prontas e chama makecontext()
    sigemptyset(&new_thread->context.uc_sigmask);
    dlist_push_right(ready_threads_list, new_thread);
	makecontext(&new_thread->context, (void*) func, 1, param);
	sigprocmask(SIG_UNBLOCK, &preemption_mask, NULL);

    return new_thread;
}

void dccthread_yield(void) {
	sigprocmask(SIG_BLOCK, &preemption_mask, NULL);

    // realiza a substituição de contexto pra thread manager
    dccthread_t* current_thread = dccthread_self();
    dlist_push_right(ready_threads_list, current_thread);
    swapcontext(&current_thread->context, &manager_thread_context);
    sigprocmask(SIG_UNBLOCK, &preemption_mask, NULL);
}

dccthread_t* dccthread_self(void) {
    return main_thread;
}

const char* dccthread_name(dccthread_t* tid) {
    return tid->name;
}

void dccthread_exit(void) {
	sigprocmask(SIG_BLOCK, &preemption_mask, NULL);

    // remove thread atual
    dccthread_t* current_thread = dccthread_self();
    free(current_thread);

    // Setar o contexto da thread manager
    setcontext(&manager_thread_context);
    sigprocmask(SIG_UNBLOCK, &preemption_mask, NULL);
}

void dccthread_wait(dccthread_t *tid) {
	sigprocmask(SIG_BLOCK, &preemption_mask, NULL);

    // Trata o caso que a thread já finalizou
    if (!find_thread_in_list(ready_threads_list, tid) && !find_thread_in_list(waiting_threads_list, tid))
        return;

    // Pega a thread atual
    // Seta a thread a esperar execução na variavel waiting_thread
    // Troca o contexto da thread sendo executada pelo contexto da manager
    dccthread_t* current_thread = dccthread_self();
    current_thread->waiting_thread = tid;
    dlist_push_right(ready_threads_list, current_thread);
    swapcontext(&current_thread->context, &manager_thread_context);
    sigprocmask(SIG_UNBLOCK, &preemption_mask, NULL);
}

int cmp_equal(const void *e1, const void *e2, void *userdata){
	return e1 != e2;
}

void resume_thread_execution(int sig, siginfo_t *si, void *uc) {
    dccthread_t* thread_to_resume = (dccthread_t *)si->si_value.sival_ptr;
    dlist_find_remove(waiting_threads_list, thread_to_resume, cmp_equal, NULL);
    dlist_push_right(ready_threads_list, thread_to_resume);
}

void dccthread_sleep(struct timespec ts) {
	sigprocmask(SIG_BLOCK, &preemption_mask, NULL);

    dccthread_t* current_thread = dccthread_self();

    sleep_signal_action.sa_flags = SA_SIGINFO;
    sleep_signal_action.sa_sigaction  = resume_thread_execution;
    sleep_signal_action.sa_mask = preemption_mask;
    sigaction(SIGUSR2, &sleep_signal_action, NULL);

    sleep_signal_event.sigev_notify = SIGEV_SIGNAL;
    sleep_signal_event.sigev_value.sival_ptr = current_thread;
    sleep_signal_event.sigev_signo = SIGUSR2;
    timer_create(CLOCK_REALTIME, &sleep_signal_event, &sleep_timer);

    sleep_time_spent.it_value = ts;
    sleep_time_spent.it_interval.tv_nsec = 0;
    sleep_time_spent.it_interval.tv_sec = 0;
    timer_settime(sleep_timer, 0, &sleep_time_spent, NULL);

    dlist_push_right(waiting_threads_list, current_thread);
    swapcontext(&current_thread->context, &manager_thread_context);

    sigprocmask(SIG_UNBLOCK, &preemption_mask, NULL);
}