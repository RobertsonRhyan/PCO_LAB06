#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <stack>
#include <cassert>
#include <queue>


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
    ThreadPool(int maxThreadCount, int maxNbWaiting): stop(false), nbWaitingThread(0), threadCount(0) {
        this->maxThreadCount = maxThreadCount;
        this->maxNbWaiting = maxNbWaiting;

    }

    ~ThreadPool(){
        stop = true;
        waitForRunnable.notifyAll();

        /*
        for(auto& thread: threads){
                thread->join();
        }*/
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

        if(nbWaitingThread > 0){
            runnableQueue.push(runnable);
            waitForRunnable.notifyOne();
        }else if(threadCount < maxThreadCount){
            threads.push_back(new PcoThread (&ThreadPool::processRunnable, this));
            ++threadCount;
            runnableQueue.push(runnable);
            waitForRunnable.notifyOne();
        }else if(runnableQueue.size() < maxNbWaiting){
            runnableQueue.push(runnable);
            waitForThread.wait(&mutex);
            waitForRunnable.notifyOne();
        }else{
            return false;
        }

        mutex.unlock();
        return true;
    }

private:
    bool stop;
    int nbWaitingThread;
    int threadCount;
    int maxThreadCount;
    int maxNbWaiting;

    PcoMutex mutex;
    std::queue<Runnable *> runnableQueue;

    std::vector<PcoThread *> threads;
    PcoConditionVariable waitForRunnable, waitForThread;

    void processRunnable(){
        Runnable *currentRunnable;

        while (!stop) {

            mutex.lock();
            ++nbWaitingThread;
            while ((runnableQueue.size() < 1) && !stop){
                waitForRunnable.wait(&mutex);
            }

            if(stop)
                return;

            currentRunnable = runnableQueue.front();
            runnableQueue.pop();
            --nbWaitingThread;
            mutex.unlock();

            currentRunnable->run();
            waitForThread.notifyOne();
        }
    }


};

#endif // THREADPOOL_H
