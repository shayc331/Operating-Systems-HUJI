//
// Created by shaharshaked on 03/05/2021.
//

#ifndef OS_EX2_THREAD_H
#define OS_EX2_THREAD_H
#include <vector>
#include <queue>
#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>
#include <signal.h>


#define STACK_SIZE 4096 /* stack size per thread (in bytes) */


using namespace std;

typedef unsigned long address_t;

/*
 * Class for a user-level thread.
 */
class Thread{
private:
    int id;
    bool blocked;
    char* stack;
    sigjmp_buf env{};
    int quantum_counter;

public:
    /*
     * Constructs a new Thread with the given id that performs the function f.
     */
    Thread(int id, void (*f)());

    /*
     * Sets the blocked state of this Thread.
     */
    void setBlocked(bool state) { blocked = state; }

    /*
     * Returns True if the Thread is blocked and False otherwise.
     */
    bool isBlocked() const { return blocked; }

    /*
     * Returns the id of this Thread.
     */
    int getID() const { return id; }

    /*
     * Returns the amount of quantums this Thread spent on a running state.
     */
    int getQuantum() const { return quantum_counter; }

    /*
     * Increments the quantum count for this Thread.
     */
    void incQuantum() { quantum_counter++; }

    /*
     * Returns the environment saved for this Thread by sigsetjmp.
     */
    sigjmp_buf *getEnv() { return &env; }

};

#endif //OS_EX2_THREAD_H
