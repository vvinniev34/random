#include "TCB.h"


TCB::TCB(int tid, Priority pr, void *(*start_routine)(void*), void *arg, State state)
{
    _tid = tid;
    _pr = pr;
    _quantum = 0;
    _state = state;
    

    // Initialize the thread context
    if (getcontext(&_context) == -1)
    {
        perror("getcontext");
        exit(EXIT_FAILURE);
    }

    if (start_routine != nullptr){
        _stack = new char[STACK_SIZE];
        _context.uc_stack.ss_sp = _stack;
        _context.uc_stack.ss_size = STACK_SIZE;
        _context.uc_stack.ss_flags = 0;

        // Create a context for the stub function and set it as the thread's entry point
        makecontext(&_context, (void (*)()) stub, 2, start_routine, arg);
    }
}

TCB::~TCB()
{
    delete[] _stack;
}

void TCB::setState(State state)
{
    _state = state;
}

State TCB::getState() const
{
    return _state;
}

int TCB::getId() const
{
    return _tid;
}

Priority TCB::getPriority() const
{
    return _pr;
}

void TCB::increaseQuantum()
{
    _quantum++;
}

int TCB::getQuantum() const
{
    return _quantum;
}

int TCB::saveContext()
{
    return getcontext(&_context);
}

void TCB::loadContext()
{
    setcontext(&_context);
}
