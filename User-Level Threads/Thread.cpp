//
// Created by shaharshaked on 03/05/2021.
//

#include "Thread.h"

//#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr){
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

/*
 * Constructs a new Thread with the given id that performs the function f.
 */
Thread::Thread(int id, void (*f)()) {
    this->id = id;
    quantum_counter = 0;
    stack = new char[STACK_SIZE];
    blocked = false;
    address_t sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t)f;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}

