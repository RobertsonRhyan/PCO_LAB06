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

    /**
     * @brief ThreadPool     Constructor
     * @param maxThreadCount Max size of pool
     * @param maxNbWaiting   Max number of waiting runnables
     */
    ThreadPool(unsigned maxThreadCount, unsigned maxNbWaiting): stop(false),
        nbWaitingThread(0), nbWaitingRunnable(0), threadCount(0) {
        this->maxThreadCount = maxThreadCount;
        this->maxNbWaiting = maxNbWaiting;

    }

    /**
     * @brief ~ThreadPool Destructor
     *        Wakes all sleeping threads and waits for them to finish
     */
    ~ThreadPool(){

        stop = true;

        /* Optional, not sure if required
         * Cancels all waiting runnables and removes them from the queue
        while(!runnableQueue.empty()){
            runnableQueue.front()->cancelRun();
            runnableQueue.pop();
        }*/

        // Signal to all waiting threads to continue
        waitForRunnable.notifyAll();

        // Wait for all threads to terminate
        for(auto& thread: threads){
                thread->join();
                delete thread;
        }

    }




    /* \brief start
     *      Start a runnable. If a thread in the pool is available, assign the
     *      runnable to it. If no thread is available but the pool can grow,
     *      create a new pool thread and assign the runnable to it. If no thread
     *      is available and the pool is at max capacity and there are less than
     *      maxNbWaiting threads waiting, block the caller until a thread
     *      becomes available again, and else do not run the runnable.
     *      If the runnable has been started, returns true, and else
     *      (the last case), return false.
     *  \param runnable Runnable to run in pool
     *  \return true:   if runnable has run
     *          false:  if refused by pool
     */
    bool start(Runnable* runnable) {
        mutex.lock();

        // If stop requested, cancel runnable.
        if(stop){
            runnable->cancelRun();
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
            // Add runnable to queue
            runnableQueue.push(runnable);

            // Create a new thread and insert into threads vector
            threads.push_back(new PcoThread (&ThreadPool::processRunnable,
                                             this));
            ++threadCount;
        }
        // If no available threads but still below
        // waiting runnable count threshold
        else if(nbWaitingRunnable < maxNbWaiting){

            ++nbWaitingRunnable;

            // Block caller until a thread becomes available again
            // or pool terminated
            while(!stop && nbWaitingThread < 1){
                waitForThread.wait(&mutex);
            }

            --nbWaitingRunnable;

            // If stop was requested while
            if(stop){
                runnable->cancelRun();
                return false;
            }
            runnableQueue.push(runnable);

            // Wake waiting thread
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
    /* Max pool size
     * Used by      : start() --> R
     * Protected by : PcoMutex mutex
     */
    unsigned maxThreadCount;

    /* Max number of waiting runnables
     * Used by      : start() --> R
     * Protected by : PcoMutex mutex
     */
    unsigned maxNbWaiting;


    /* Stop flag
     * Used by      : start()           --> R
     *                processRunnable() --> R
     *                ~ThreadPool()     --> W
     * Protected by : PcoMutex mutex
     */
    bool stop;

    /* Current count of threads waiting
     * Used by      : start()           --> R
     *                processRunnable() --> W
     * Protected by : PcoMutex mutex
     */
    unsigned nbWaitingThread;

    /* Current count of runnables waiting for a thread
     * Used by      : start()           --> R / W
     * Protected by : PcoMutex mutex
     */
    unsigned nbWaitingRunnable;

    /* Count of created threads (running or waiting)
     * Used by      : start()           --> R / W
     * Protected by : PcoMutex mutex
     */
    unsigned threadCount;

    // Protect shared variables
    PcoMutex mutex;

    // Monitor condition variables
    PcoConditionVariable waitForRunnable, waitForThread;

    // Store runnables for threads to run
    std::queue<Runnable *> runnableQueue;

    // Store created threads
    std::vector<PcoThread *> threads;

    /* \brief processRunnable Method call by thread
     * Waits for a runnbale to be added to queue and calls it's run() method
     */
    void processRunnable(){
        // Used to store the current runnable so the mutex can be unlocked
        // while run() is called (so it doesn't block other threads)
        Runnable *currentRunnable;

        while (1) {
            mutex.lock();

            // Signal to start() that a thread is free
            waitForThread.notifyOne();
            ++nbWaitingThread;

            // Wait for a runnable while the pool hasen't been terminate and
            // the runnableQueue is empty (meaning another thread "stole" the
            // runnable that unlocked this one).
            while (!stop && runnableQueue.size() < 1){
                waitForRunnable.wait(&mutex);
            }

            --nbWaitingThread;

            // If this pool is being destroyed, stop this thread
            if(stop){
                // Unlock before killing thread
                mutex.unlock();
                // Exit the loop so thread can die
                break;
            }

            // Store runnable in temp var to avoid blocking other threads
            currentRunnable = runnableQueue.front();
            runnableQueue.pop();

            mutex.unlock();

            // Run "runnable"
            currentRunnable->run();
        }

    }




};

#endif // THREADPOOL_H
