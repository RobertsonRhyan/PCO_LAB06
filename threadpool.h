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

    /**
     * @brief ThreadPool     Constructeur
     * @param maxThreadCount Nombre maximal de threads dans le pool
     * @param maxNbWaiting   Nombre maximal de Runnable en attente
     */
    ThreadPool(int maxThreadCount, int maxNbWaiting)
        :nbThread(0), nbWaitingThread(0),nbWaitingRunnable(0),
         threadPoolFinish(false)
    {
        this->maxThreadCount = maxThreadCount;
        this->maxNbWaiting = maxNbWaiting;
    }

    /**
     * @brief ~ThreadPool Destructeur
     */
    ~ThreadPool(){
        //Passe l'information aux thread que le programme se termine
        threadPoolFinish = true;

        //On relâche tous les threads en attente pour les terminer proprement
        if(nbWaitingThread > 0){
            cond.notifyAll();
        }

        //Terminaison des threads et suppression
        for(PcoThread* const &t: threadQueue){
            t->join();
            delete t;
        }
    }

    /**
     * @brief processRunnable Utilisée par un thread pour traiter un runnable,
     *                        chaque thread créé dans le Thread Pool va l'executer
     */
    void processRunnable(){
        //Runnable traité par le thread
        Runnable *currentRunnable;

        while(1){
             mutex.lock();

             //Incrémentation du nombre de thread en attente
             nbWaitingThread++;

             //Utilisation d'un moniteur de Mesa pour l'attente des threads
             while(RunnableQueue.size() < 1 && threadPoolFinish == false){
                 cond.wait(&mutex);
             }

             //Le thread n'est plus en attente, on décrémente
             nbWaitingThread--;

             //Sortir de la boucle while si le programme se termine et qu'aucun
             //runnable n'est encore en attente
             if(threadPoolFinish == true && nbWaitingRunnable == 0){
                 mutex.unlock();
                 break;
             }

             //Traitement du runnable dans la file d'attente
             currentRunnable = RunnableQueue.front();
             RunnableQueue.pop();
             nbWaitingRunnable--;

             mutex.unlock();
             //Lancement de la fonction du runnable
             currentRunnable->run();
             mutex.lock();

             //Sortir de la boucle while, si le programme se termine
             if(threadPoolFinish == true){
                 mutex.unlock();
                 break;
             }

             mutex.unlock();
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

        //Si le nombre de runnable qui attendent dans la file d'attente >
        //maxNbWaiting, on abandonne la requête
        if(nbWaitingRunnable >= maxNbWaiting){

            //Arrêt du runnable lancé
            runnable->cancelRun();

            mutex.unlock();
            return false;
        }

        //Si un thread est en attente, on met le runnable dans la file d'attente
        //et on relâche un thread en attente
        if(nbWaitingThread > 0){

            //Incrémentation du nombre de runnable en attente
            nbWaitingRunnable++;

            //Ajout du runnable dans la file d'attente
            RunnableQueue.push(runnable);

            //Relâchement d'un thread en attente
            cond.notifyOne();
        }

        //Si pas de thread disponible, on en créé un
        else if(nbThread < maxThreadCount){

            //Incrémentation du nombre de threads dans le Thread Pool ainsi que
            //du nombre de runnable en attente
            nbThread++;
            nbWaitingRunnable++;

            //Ajout du runnable dans la file d'attente
            RunnableQueue.push(runnable);

            //Création d'un thread
            threadQueue.push_front(new PcoThread (&ThreadPool::processRunnable, this));

            //Relâchement d'un thread en attente
            cond.notifyOne();
        }

        //Si pas de thread disponible, pool plein et nombre de runnable qui
        //attendent dans la file d'attente > maxNbWaiting, on met le runnable
        //dans la file d'attente qui sera traité lorsqu'un thread sera disponible
        else{

            //Incrémentation du nombre de runnable en attente
            nbWaitingRunnable++;

            //Ajout du runnable dans la file d'attente
            RunnableQueue.push(runnable);
        }

        mutex.unlock();

        //Si la requête a été lancée (traitée), on retourne true
        return true;
    }

private:
    /*Nombre max de threads possibles dans le pool
     *Utilisée par : start()   --> (Lecture)
     *Protegée     : Par un PcoMutex mutex
     */
    int maxThreadCount;

    /*Nombre max de Runnable qui peuvent attendre dans la queue
     *Utilisée par : start()   --> (Lecture)
     *Protegée     : Par un PcoMutex mutex
     */
    int maxNbWaiting;

    /*Nombre de threads dans le pool
     *Utilisée par : start()   --> (Lecture/Ecriture)
     *Protegée     : Par un PcoMutex mutex
     */
    int nbThread;

    /*Nombre de threads attente dans le pool
     *Utilisée par : start()             --> (Lecture)
     *Utilisée par : processRunnable()   --> (Ecriture)
     *Utilisée par : ~ThreadPool()       --> (Lecture)
     *Protegée     : Par un PcoMutex mutex
     */
    int nbWaitingThread;

    /*Nombre de runnable en attente dans le pool
     *Utilisée par : start()             --> (Lecture/Ecriture)
     *Utilisée par : processRunnable()   --> (Lecture/Ecriture)
     *Protegée     : Par un PcoMutex mutex
     */
    int nbWaitingRunnable;

    /*Indique à un thread réveillé qu'il doit se terminer
     *(Lors de la destruction du threadPool)
     *Utilisée par : ~ThreadPool()       --> (Ecriture)
     *Utilisée par : processRunnable()   --> (Lecture)
     *Protegée     : Par un PcoMutex mutex
     */
    bool threadPoolFinish;

    //Queue de pointeurs de Runnable pour les runnables en atente (FIFO)
    std::queue<Runnable*> RunnableQueue;
    //Deque pour stocker les threads du pool
    std::deque<PcoThread*> threadQueue;

    //Mutex pour la protection des variables partagées
    PcoMutex mutex;
    //Variable de condition pour le moniteur de Mesa
    PcoConditionVariable cond;
};

#endif // THREADPOOL_H
