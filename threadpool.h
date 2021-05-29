/******************************************************************************
 * Auteurs     : Dylan Canton, Rhyan Robertson
 * Date        : 28.05.2021
 * Description : Gestion d'un threadpool
 * ***************************************************************************/

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <stack>
#include <cassert>
#include <queue>
#include <deque>

#include <pcosynchro/pcologger.h>
#include <pcosynchro/pcothread.h>
#include <pcosynchro/pcomutex.h>
#include <pcosynchro/pcoconditionvariable.h>

class Runnable {
public:
    /*
     * An empy virtual destructor
     */
    virtual ~Runnable() {}
    /*
     * Function executing the Runnable task.
     */
    virtual void run() = 0;

    /*
     * Function that can be called from the outside, to ask the cancellation
     * of the runnable.
     */
    virtual void cancelRun() = 0;

    /*
     * Simply retrieve an identifier for this runnable
     */
    virtual std::string id() = 0;
};

class ThreadPool
{
public:
    ThreadPool(int maxThreadCount, int maxNbWaiting)
        :nbThread(0), nbWaitingThread(0),nbWaitingRunnable(0),
         threadPoolFinish(false), runnableIsWaiting(false)
    {
        this->maxThreadCount = maxThreadCount;
        this->maxNbWaiting = maxNbWaiting;
    }

    ~ThreadPool(){
        //-Si on détruit le thread pool, on doit laisser tous les threads finirent
        // leur calculs avant de se terminer, si un thread est en attente, on
        // le réveille et on le termine.
        threadPoolFinish = true;

        if(nbWaitingThread > 0){
            cond.notifyAll();
        }

        for(PcoThread* const &t: threadQueue){
            t->join();
            delete t;
        }
    }

    void processRunnable(Runnable* runnable){
        mutex.lock();
        bool first = true;
        mutex.unlock();

        while(1){

             mutex.lock();
             nbWaitingThread++;
             mutex.unlock();

             while(runnableIsWaiting == false){
                 cond.wait(&mutex);
             }

             mutex.lock();

             runnableIsWaiting = false;

             nbWaitingThread--;

             //Sortir de la boucle while
             if(threadPoolFinish == true && nbWaitingRunnable == 0){
                 mutex.unlock();
                 break;
             }

             //Si c'est le premier thread, on va pas dans la Queue
             if(first == false){
                 runnable = RunnableQueue.front();
                 RunnableQueue.pop();
                 nbWaitingRunnable--;
             }

             mutex.unlock();
             runnable->run();
             mutex.lock();

             first = false;

             //Sortir de la boucle while
             if(threadPoolFinish == true){
                 mutex.unlock();
                 break;
             }
        }
    }

    /*
     * Start a runnable. If a thread in the pool is available, assign the
     * runnable to it. If no thread is available but the pool can grow, create a new
     * pool thread and assign the runnable to it. If no thread is available and the
     * pool is at max capacity and there are less than maxNbWaiting threads waiting,
     * block the caller until a thread becomes available again, and else do not run the runnable.
     * If the runnable has been started, returns true, and else (the last case), return false.
     */
    bool start(Runnable* runnable) {

        mutex.lock();

        //-Si pas de thread dispo, pool plein et nbRequêtes qui attendent
        // dans la file d'attente > maxNbWaiting, on drop la requête
        if(nbWaitingRunnable > maxNbWaiting){
            runnable->cancelRun();
            mutex.unlock();
            return false;
        }


        //-Si un thread est en attente, on l'assigne
        if(nbWaitingThread > 0){
            nbWaitingRunnable++;
            RunnableQueue.push(runnable);

            runnableIsWaiting = true;
            cond.notifyOne();
        }

        //-Si pas de thread, on en créé un et on l'assigne
        else if(nbThread < maxThreadCount){
            nbThread++;
            runnableIsWaiting = true;
            threadQueue.push_front(new PcoThread (&ThreadPool::processRunnable, this, runnable));
        }

        //-Si pas de thread dispo, pool plein et nbRequêtes qui attendent
        // dans la file d'attente < maxNbWaiting, on bloque
        // la requête le temps qu'un thread redevienne dispo
        else{
            nbWaitingRunnable++;
            RunnableQueue.push(runnable);
        }

        mutex.unlock();

        //Si la requête a été lancée (traitée), on retourne true
        return true;

        //TODO:
        // -Synchronisation avec mutex
        // -Mettre la var condition runnableIsWaitingà false quelque part
    }

private:
    //Nombre max de threads possibles dans le pool
    int maxThreadCount;
    //Nombre max de Runnable qui peuvent attendre dans la queue
    int maxNbWaiting;

    //Nombre de threads dans le pool
    int nbThread;
    //Nombre de threads attente dans le pool
    int nbWaitingThread;
    //Nombre de runnable en attente dans le pool
    int nbWaitingRunnable;

    //Indique à un thread réveillé qu'il doit se terminer tout de suite sans
    //faire la suite de ses opérations (Lors de la destruction du threadPool)
    bool threadPoolFinish;

    //Queue de pointeurs de Runnable pour les runnables en atente (FIFO)
    std::queue<Runnable*> RunnableQueue;
    //Queue pour stocker les threads du pool
    std::deque<PcoThread*> threadQueue;

    PcoMutex mutex;
    PcoConditionVariable cond;
    bool runnableIsWaiting;
};

#endif // THREADPOOL_H
