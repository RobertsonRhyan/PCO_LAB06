
#include <chrono>

#include <gtest/gtest.h>

#include <pcosynchro/pcologger.h>
#include <pcosynchro/pcothread.h>

#include "threadpool.h"


#define RUNTIME 100000
#define RUNTIMEINMS 100

///
/// \brief The ThreadpoolTest class
/// This class embeds all the tests of the ThreadPool
class ThreadpoolTest : public ::testing::Test
{

public:
    ThreadpoolTest(){};

    ///
    /// \brief runnableStarted Called before sending the Runnable to the pool
    /// \param id Id of the Runnable
    ///
    void runnableStarted(std::string id)
    {
        // No concurrent access, no need to protect it
        m_runningState[id] = true;
    }

    ///
    /// \brief runnableTerminated Called when the runnable finishes
    /// \param id Id of the Runnable
    /// It has to be called by the Runnable itself, and record the ending time
    void runnableTerminated(std::string id)
    {
        mutex.lock();
        // Erases the last value, protection between runnables is mandatory
        endingTime = std::chrono::system_clock::now();
        mutex.unlock();
        // Only a single access, no need to protect it
        m_runningState[id] = false;
    }    

    /// \brief m_runningState A map to represent the running state of the Runnables
    /// True : running. False : not running
    std::map<std::string, bool> m_runningState;

    /// \brief m_started A map to represent the fact that the Runnables have been started
    /// True : started. False : not started
    std::map<std::string, bool> m_started;

    ///
    /// \brief startingTime To store the time the testcase started
    ///
    std::chrono::time_point<std::chrono::system_clock> startingTime;

    ///
    /// \brief endingTime To store the time of the end of the last Runnable
    ///
    std::chrono::time_point<std::chrono::system_clock> endingTime;

    //! A mutex to protect internal variables
    std::mutex mutex;

    ///
    /// \brief initTestCase Function to init the testcase.
    /// Needs to be called at the beginning of each testcase.
    void initTestCase()
    {
        startingTime = std::chrono::system_clock::now();
    }

    ///
    /// \brief testCase1 A testcase with a pool of 10 threads running 10 runnables
    /// Each runnable just waits 10 ms and finishes.
    /// Check is done on the termination of the Runnables (everyone finished),
    /// and on the time required to run all the runnables.
    ///
    void testCase1();

    ///
    /// \brief testCase2 A testcase with a pool of 10 threads running 100 runnables
    /// Each runnable just waits 10 ms and finishes.
    /// Check is done on the termination of the Runnables (everyone finished),
    /// and on the time required to run all the runnables.
    ///
    void testCase2();

    ///
    /// \brief testCase3 A testcase with a pool of 10 threads running 10x10 runnables
    /// Each runnable just waits 10 ms and finishes. A batch of 10 runnables is started,
    /// and after its completion, 10 other start, and so on.
    /// Check is done on the termination of the Runnables (everyone finished),
    /// and on the time required to run all the runnables.
    ///
    void testCase3();

    ///
    /// \brief testCase4 A testcase with a pool of 10 threads running 30 runnables
    /// Each runnable is launch in a thread to create a pile of waiting runnables
    /// Check is done on the termination of the Runnables (everyone finished),
    /// on the amount of failed runnables,
    /// and on the time required to run all the runnables.
    ///
    void testCase4();
};




///
/// \brief The TestRunnable class
/// This class is a subclass of Runnable, and therefore is used by the testcases
/// as Runnables
class TestRunnable : public Runnable
{
    //! A reference on the main Test class
    ThreadpoolTest *m_tester;

    //! The Id of the Runnable
    std::string m_id;

public:

    ///
    /// \brief TestRunnable Simple constructor
    /// \param tester Reference to the main Test class
    /// \param id Id of the Runnable (should be unique)
    ///
    TestRunnable(ThreadpoolTest *tester, std::string id) : m_tester(tester), m_id(id){
    }

    //! Empty destructor
    virtual ~TestRunnable() {}

    //! Method ran by the thread of the pool
    virtual void run() {
        logger() << "[TEST] RUNNING " << this->m_id << "\n";
        PcoThread::usleep(RUNTIME);
        m_tester->runnableTerminated(m_id);
    }

    ///
    /// \brief id returns the Id of the Runnable
    /// \return The Id of the Runnable
    ///
    virtual std::string id() {
        return m_id;
    }

    virtual void cancelRun() {
        m_tester->runnableTerminated(m_id);
    }
};


typedef struct {
    int thread_id;
    TestRunnable *runnable;
    ThreadPool *pool;
    ThreadpoolTest *test;
} ThreadParameter;


void startInThread(ThreadParameter *param) {

    if(!param->pool->start(param->runnable)) {
        param->test->m_started[param->runnable->id()] = true;
        logger() << "[TEST] Runnable start failed " << param->thread_id << "\n";
    }
}



TEST_F(ThreadpoolTest, testCase1)
{
    initTestCase();
    ThreadPool pool(10, 50);

    // Starts the runnables
    for(int i = 0; i < 10; i++) {
        std::string runnableId = "Run" + std::to_string(i);
        TestRunnable *runnable = new TestRunnable(this, runnableId);
        runnableStarted(runnableId);
        pool.start(runnable);
    }


    // Check that every runnable is not finished yet
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, true) << "Failed";
    }

    PcoThread::usleep(1000 * (RUNTIMEINMS + 5));

    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed";
    }

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (RUNTIMEINMS + 5)) << "Too long execution time";

    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (RUNTIMEINMS - 2)) << "Too short execution time";
}

TEST_F(ThreadpoolTest, testCase2)
{
    initTestCase();
    ThreadPool pool(10, 100);

    // Starts the runnables
    for(int i = 0; i < 100; i++) {
        std::string runnableId = "Run" + std::to_string(i);
        TestRunnable *runnable = new TestRunnable(this, runnableId);
        runnableStarted(runnableId);
        pool.start(runnable);
    }

    PcoThread::usleep(1000 * (10 * RUNTIMEINMS + 30));

    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed";
    }

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (10 * RUNTIMEINMS + 30)) << "Too long execution time";

    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (10 * RUNTIMEINMS - 30)) << "Too short execution time";
}




TEST_F(ThreadpoolTest, testCase3)
{
    initTestCase();
    ThreadPool pool(10, 100);

    for(int nbBatch = 0; nbBatch < 10; nbBatch ++) {
        // Starts the runnables
        for(int i = 0; i < 10; i++) {
            std::string runnableId = "Run" + std::to_string(nbBatch) + "_" + std::to_string(i);
            TestRunnable *runnable = new TestRunnable(this, runnableId);
            runnableStarted(runnableId);
            pool.start(runnable);
        }

        // Wait until completion of each runnable
        PcoThread::usleep(1000 * (RUNTIMEINMS + 30));

    }


    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed";
    }

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (10 * RUNTIMEINMS + 300)) << "Too long execution time";

    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (10 * RUNTIMEINMS - 300)) << "Too short execution time";
}




TEST_F(ThreadpoolTest, testCase4)
{
    initTestCase();
    ThreadPool pool(10, 5);
    std::vector<std::unique_ptr<PcoThread>> m_threads;

    ThreadParameter param[100];

    // Starts the runnables
    for(int i = 0; i < 30; i++) {
        std::string runnableId = "Run" + std::to_string(i);
        TestRunnable *runnable = new TestRunnable(this, runnableId);
        runnableStarted(runnableId);
        logger() << "[TEST] " << runnableId << "\n";
        param[i].thread_id = i;
        param[i].pool = &pool;
        param[i].runnable = runnable;
        param[i].test = this;

        m_threads.push_back(std::make_unique<PcoThread>(startInThread, &param[i]));
    }

    //Wait for the threads to finish
    for (size_t i = 0; i < 30; i++) {
        m_threads[i]->join();
    }
    m_threads.clear();

    // Wait until completion of each runnable
    PcoThread::usleep(1000 * (2 * RUNTIMEINMS + 30));

    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed";
    }

    // Check that the right amount of runnable failed
    int nbStarted = 0;
    for (const auto& [key, value] : m_started) {
        if (value) {
            nbStarted ++;
        }
    }
    EXPECT_EQ(nbStarted, 15) << "Not the right amount of lost runnables";


    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (2 * RUNTIMEINMS + 30)) << "Too long execution time";

    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (2 * RUNTIMEINMS - 30)) << "Too short execution time";
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    logger().initialize(argc, argv);
    return RUN_ALL_TESTS();
}
