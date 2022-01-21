#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <pthread.h>

#include "../../lib/utils/utils.h"

typedef struct threadpool_t {
    pthread_mutex_t  lock;    // mutua esclusione nell'accesso all'oggetto
    pthread_cond_t   cond;    // usata per notificare un worker thread 
    pthread_t  * threads; // array di worker id
    int nWorker;           // numero di thread (size dell'array threads)
    ServerRequest* queue; // coda interna per task pendenti
    int queueMax;           // massima size della coda, puo' essere anche -1 ad indicare che non si vogliono gestire task pendenti
    int head, tail;           // riferimenti della coda
    int count;                // numero di task nella coda dei task pendenti
    int exiting;              // se > 0 e' iniziato il protocollo di uscita, se 1 il thread aspetta che non ci siano piu' lavori in coda
} Threadpool;

Threadpool* createThreadPool(int nWorker, int dimQueue);
int destroyThreadPool(Threadpool *pool, int force);
int addRequestToPool(Threadpool *pool, ServerRequest* req) ;


/**
 * @function spawnThread
 * @brief lancia un thread che esegue la funzione fun passata come parametro, il thread viene lanciato in modalità detached e non fa parte del pool.
 * @param fun  funzione da eseguire per eseguire il task
 * @param arg  argomento della funzione
 * @return 0 se successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
//int spawnThread(void (*f)(void*), void* arg);


#endif