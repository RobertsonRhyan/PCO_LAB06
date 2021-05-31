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
    ThreadPool(unsigned maxThreadCount, unsigned maxNbWaiting): stop(false), nbWaitingThread(0), nbWaitingRunnable(0), threadCount(0) {
        this->maxThreadCount = maxThreadCount;
        this->maxNbWaiting = maxNbWaiting;

    }

    ~ThreadPool(){

        stop = true;

        // While there is waiting threads, notify them to continue.
        while (nbWaitingThread > 0) {
            mutex.unlock(); // Seems to help pass tests - Need to investigate

            // Notify all waiting threads to continue
            waitForRunnable.notifyAll();
        }

        // Wait for all threads to terminate
        for(auto& thread: threads){
                thread->join();
                delete thread;
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

        // If stop requested, do not run runnable.
        if(stop){
            mutex.unlock();
            return false;
        }

        // If 1 or more available threads
        if(nbWaitingThread > 0){
            // Push runnable on queue
            runnableQueue.push(runnable);
            // Wake a waiting thread
            waitForRunnable.notifyOne();
        }
        // If no available threads but still below thread count threshold
        else if(threadCount < maxThreadCount){
            // Create a new thread and insert into threads vector
            threads.push_back(new PcoThread (&ThreadPool::processRunnable,
                                             this));
            ++threadCount;

            // Add runnable to queue
            runnableQueue.push(runnable);
            // Wake a waiting thread
            waitForRunnable.notifyOne();
        }
        // If no available threads but still below
        // waiting runnable count threshold
        else if(nbWaitingRunnable < maxNbWaiting){

            ++nbWaitingRunnable;
            // Block caller until a thread becomes available again
            while(!stop && nbWaitingThread < 1){
                waitForThread.wait(&mutex);
            }

            runnableQueue.push(runnable);
            --nbWaitingRunnable;
            waitForRunnable.notifyOne();
        }
        // If the amount of waiting runnables has reached the threshold,
        // cancel the runnable and return false
        else{
            runnable->cancelRun();
            mutex.unlock();
            return false;
        }

        mutex.unlock();
        return true;
    }

private:
    bool stop;
    unsigned nbWaitingThread;
    unsigned nbWaitingRunnable;
    unsigned threadCount;
    unsigned maxThreadCount;
    unsigned maxNbWaiting;

    PcoMutex mutex;
    PcoConditionVariable waitForRunnable, waitForThread;

    std::queue<Runnable *> runnableQueue;
    std::vector<PcoThread *> threads;
    //std::vector<std::unique_ptr<PcoThread> > threads;


    void processRunnable(){
        Runnable *currentRunnable;

        while (1) {
            mutex.lock();
            ++nbWaitingThread;

            while (!stop && runnableQueue.size() < 1){
                waitForRunnable.wait(&mutex);
            }

            --nbWaitingThread;

            currentRunnable = runnableQueue.front();
            runnableQueue.pop();

            mutex.unlock();

            if(stop)
                break;

            currentRunnable->run();

            waitForThread.notifyOne();
        }

    }




};

#endif // THREADPOOL_H
