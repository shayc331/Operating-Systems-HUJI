//
// Created by shaharshaked on 03/05/2021.
//

#include "uthreads.h"
#include "Thread.h"
#include <vector>
#include <queue>
#include <unordered_map>
#include <iostream>
#include <signal.h>
#include <set>
#include <string.h>

#define FAILURE (-1)
#define LIBRARY_ERROR "thread library error: "


using namespace std;

typedef struct Mutex{
    bool locked;
    Thread* holder;
}Mutex;

static deque<int> ready_threads = deque<int>();
static set<int> waitingForMutex = set<int>();
static unordered_map<int, Thread*> threads = unordered_map<int, Thread*>();
static priority_queue<int, vector<int>, greater<int>> free_ids =
        priority_queue<int, vector<int>, greater<int>>();
static Thread* running_thread = nullptr;
static int quantum_count = 1;
static struct sigaction sa = {0};
static struct itimerval timer;
static Mutex mutex;
static bool self_terminating = false;
sigset_t _set;

/*
 * Description: performes a switch between two threads and updates their attributes accordingly.
 */
void timer_handler(int){
    sigprocmask(SIG_SETMASK, &_set, NULL);
    if (!self_terminating) {
        if (sigsetjmp(*running_thread->getEnv(), 1) == 1) {
            sigprocmask(SIG_UNBLOCK, &_set, NULL);
            return;
        }
        if (!running_thread->isBlocked() && waitingForMutex.find(running_thread->getID()) == waitingForMutex.cend())
            ready_threads.push_front(running_thread->getID());
    }
    running_thread = threads[ready_threads.back()];
    ready_threads.pop_back();

    if (waitingForMutex.find(running_thread->getID()) != waitingForMutex.cend() && !mutex.locked){
        mutex.locked = true;
        mutex.holder = running_thread;
        waitingForMutex.erase(running_thread->getID());
    }

    running_thread->incQuantum();
    quantum_count++;
    self_terminating = false;
    sigprocmask(SIG_UNBLOCK, &_set, NULL);
    siglongjmp(*running_thread->getEnv(),1);
}
/*
 * Description: print library error.
 */
void library_error(string msg){
    cerr<<LIBRARY_ERROR<<msg<<endl;
    fflush(stderr);
}

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0){
        library_error("quantum_usecs must be positive.");
        return FAILURE;
    }

    sigemptyset(&_set);
    sigaddset(&_set, SIGVTALRM);

    running_thread = new Thread(0, NULL);
    running_thread->incQuantum();
    threads[0] = running_thread;
    mutex.locked = false;
    mutex.holder = nullptr;

    sa.sa_handler = &timer_handler;

    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
        cerr << "sigaction error.\n";

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = quantum_usecs;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = quantum_usecs;

    if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
        cerr << "system error: timer error\n";

    return EXIT_SUCCESS;
}

/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void)) {
    sigprocmask(SIG_SETMASK, &_set, NULL);
    int new_thread_id;
    if (threads.size() == MAX_THREAD_NUM) {
        library_error("you reached the max number of threads.");
        sigprocmask(SIG_UNBLOCK, &_set, NULL);
        return FAILURE;
    }

    if (free_ids.empty())
        new_thread_id = threads.size();
    else{
        new_thread_id = free_ids.top();
        free_ids.pop();
    }

    auto* new_thread = new Thread(new_thread_id, f);
    ready_threads.push_front(new_thread_id);
    threads[new_thread_id] = new_thread;
    sigprocmask(SIG_UNBLOCK, &_set, NULL);
    return new_thread_id;
}

/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid) {
    sigprocmask(SIG_SETMASK, &_set, NULL);
    if (threads.find(tid) == threads.end()){
        library_error("invalid thread id.");
        sigprocmask(SIG_UNBLOCK, &_set, NULL);
        return FAILURE;
    }

    if (tid == 0) {
        for (auto t : threads)
            delete t.second;
        threads.clear();
        exit(0);
    }

    for (auto iter = ready_threads.cbegin(); iter < ready_threads.cend(); ++iter){
        if (*(iter) == tid) {
            ready_threads.erase(iter);
            break;
        }
    }

    if (waitingForMutex.find(tid) != waitingForMutex.cend())
        waitingForMutex.erase(tid);

    if (mutex.locked && mutex.holder->getID() == tid){
        mutex.locked = false;
        mutex.holder = nullptr;
    }

    if (running_thread->getID() == tid)
        self_terminating = true;

    free_ids.push(tid);
    delete threads[tid];
    threads.erase(tid);
    sigprocmask(SIG_UNBLOCK, &_set, NULL);
    if (self_terminating)
        timer_handler(0);
    return EXIT_SUCCESS;
}

/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 */
int uthread_block(int tid) {
    sigprocmask(SIG_SETMASK, &_set, NULL);
    if (threads.find(tid) == threads.end()){
        library_error("invalid thread id.");
        sigprocmask(SIG_UNBLOCK, &_set, NULL);
        return FAILURE;
    }

    if (tid == 0){
        library_error("cannot block the main thread");
        sigprocmask(SIG_UNBLOCK, &_set, NULL);
        return FAILURE;
    }

    threads[tid]->setBlocked(true);

    for (auto iter = ready_threads.cbegin(); iter < ready_threads.cend(); ++iter){
        if (*(iter) == tid) {
            ready_threads.erase(iter);
            break;
        }
    }

    if (running_thread->getID() == tid)
        timer_handler(0);

    sigprocmask(SIG_UNBLOCK, &_set, NULL);
    return EXIT_SUCCESS;
}

/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    sigprocmask(SIG_SETMASK, &_set, NULL);
    if (threads.find(tid) == threads.end()){
        library_error("invalid thread id.");
        sigprocmask(SIG_UNBLOCK, &_set, NULL);
        return FAILURE;
    }

    if (threads[tid]->isBlocked()) {
        threads[tid]->setBlocked(false);
        ready_threads.push_front(threads[tid]->getID());
    }

    sigprocmask(SIG_UNBLOCK, &_set, NULL);
    return EXIT_SUCCESS;
}

/*
 * Description: This function tries to acquire a mutex.
 * If the mutex is unlocked, it locks it and returns.
 * If the mutex is already locked by different thread, the thread moves to BLOCK state.
 * In the future when this thread will be back to RUNNING state,
 * it will try again to acquire the mutex.
 * If the mutex is already locked by this thread, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_lock() {
    sigprocmask(SIG_SETMASK, &_set, NULL);
    if (mutex.locked){
        if (mutex.holder->getID() == running_thread->getID()) {
            library_error("invalid mutex lock request.");
            sigprocmask(SIG_UNBLOCK, &_set, NULL);
            return FAILURE;
        }
        waitingForMutex.insert(running_thread->getID());
        timer_handler(1);
    }
    else{
        mutex.locked = true;
        mutex.holder = running_thread;
    }
    sigprocmask(SIG_UNBLOCK, &_set, NULL);
    return EXIT_SUCCESS;
}

/*
 * Description: This function releases a mutex.
 * If there are blocked threads waiting for this mutex,
 * one of them (no matter which one) moves to READY state.
 * If the mutex is already unlocked, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_unlock() {
    sigprocmask(SIG_SETMASK, &_set, NULL);
    if (!mutex.locked || mutex.holder != running_thread){
        library_error("invalid mutex unlock request.");
        sigprocmask(SIG_UNBLOCK, &_set, NULL);
        return FAILURE;
    }

    mutex.locked = false;
    mutex.holder = nullptr;
    for (auto iter = waitingForMutex.cbegin(); iter != waitingForMutex.cend(); ++iter) {
        if (!threads[*iter]->isBlocked()){
            ready_threads.push_front(threads[*iter]->getID());
            break;
        }
    }
    sigprocmask(SIG_UNBLOCK, &_set, NULL);
    return EXIT_SUCCESS;
}

/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid() {
    return running_thread->getID();
}

/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums() {
    return quantum_count;
}

/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid) {
    sigprocmask(SIG_SETMASK, &_set, NULL);
    if (threads.find(tid) == threads.end()){
        library_error("invalid thread id.");
        sigprocmask(SIG_UNBLOCK, &_set, NULL);
        return FAILURE;
    }
    sigprocmask(SIG_UNBLOCK, &_set, NULL);
    return threads[tid]->getQuantum();
}

