# PAGINADOR DE MEMÓRIA - RELATÓRIO
___

1. Termo de compromisso

Os membros do grupo afirmam que todo o código desenvolvido para este
trabalho é de autoria própria.  Exceto pelo material listado no item
3 deste relatório, os membros do grupo afirmam não ter copiado
material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

  * Bruno Wilson <brunowilson4@gmail.com> 50%
  * Gustavo Augusto <gustavo.augustoprs@gmail.com> 50%

3. Referências bibliográficas

  - Documentação biblioteca "ucontext.h" - <https://pubs.opengroup.org/onlinepubs/7908799/xsh/getcontext.html>, <https://pubs.opengroup.org/onlinepubs/7908799/xsh/makecontext.html>; acessado em 18/04/2023
  - Documentação biblioteca "signal.h" - <https://pubs.opengroup.org/onlinepubs/009695399/basedefs/signal.h.html>; acessado em 24/04/2023
  - Silberschatz, A. - Fundamentos de Sistemas Operacionais, 9ª edição
  - Tanenbaum, A. S. - Sistemas Operacionais Modernos, 4ª edição

4. Estruturas de dados

  1. Descreva e justifique as estruturas de dados utilizadas para gerência das threads de espaço do usuário (partes 1, 2 e 5).

  Começando pela criação do TAD **dccthread_t**, foi definida nele a variável de controle de contexto `ucontext_t context`, o nome da thread `char name`, com a quantidade máxima de caracteres definida em **dccthread.h**, e por fim, um ponteiro `dccthread_t* waiting_thread` apontando para o próprio tipo e que sua função é guardar a referência da thread que ele está aguardando o fim da execução.

  A seguir, foram definidas duas listas: a `struct dlist* ready_threads_list`, que armazena as thread prontas para serem executadas, e a `struct dlist* waiting_threads_list` que armazena as threads que estão em espera. O controle sobre quais threads estão na lista de espera ou prontas para execução será detalhada a seguir.

  Por fim, foi definido um tipo de dado que é comum em diversas linguagens de programação, e que não existe no C, que é o tipo `bool`. Foi definido sendo um `unsigned char`, que é o tipo de dado de menor tamanho disponível para que seja usado na função `bool find_thread_in_list()`, criada pelo grupo, que retorna se uma condição é verdadeira ou não.
  
  2. Descreva o mecanismo utilizado para sincronizar chamadas de dccthread_yield e disparos do temporizador (parte 4).

  Para sincronizar as chamadas de `dccthread_yield()` e fazer o controle dos disparos do temporizador, foi feito o uso da função `sigprocmask()` no início e fim da chamada, passando como argumento o tipo de tratamento do sinal de interesse (`SIG_BLOCK` e `SIG_UNBLOCK`), para que quando a chamada de **yield** fosse realizada mais de uma vez por outra thread, ela aguardasse a execução da chamada mais antiga antes de prosseguir. 
  
  Nos disparos do temporizador foi implementada a mesma ideia, só que com a adição das variáveis `timer_t timer`, que é responsável por guardar o id do timer a ser chamado, a `struct sigevent signal_event`, que guarda a configuração do sinal a ser emitido no fim do timer, a `struct sigaction signal_action`, que é responsável por definir qual ação a ser tomada quando o sinal for emitido, a `struct itimerspec time_spent`, em que configura o tempo do timer e usada no momento de inicialização, a `sigset_t preemption_mask`, que configura a **mask** nos trechos de espera de thread. Para o controle de espera de thread, foram criadas as mesmas variáveis, com os mesmos tipos e nomes parecidos, com a diferença que foi adicionada o nome **"sleep_"** na frente de cada variável, porém, a implentação do tratamento dos temporizadores foram os mesmos.
  