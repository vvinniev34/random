#include "uthread.h"
#include "TCB.h"
#include <cassert>
#include <deque>

#include <unordered_map>

using namespace std;

// Finished queue entry type
typedef struct finished_queue_entry
{
	TCB *tcb;	  // Pointer to TCB
	void *result; // Pointer to thread result (output)
} finished_queue_entry_t;

// Join queue entry type
typedef struct join_queue_entry
{
	TCB *tcb;			 // Pointer to TCB
	int waiting_for_tid; // TID this thread is waiting on
} join_queue_entry_t;

// You will need to maintain structures to track the state of threads
// - uthread library functions refer to threads by their TID so you will want
//   to be able to access a TCB given a thread ID
// - Threads move between different states in their lifetime (READY, BLOCK,
//   FINISH). You will want to maintain separate "queues" (doesn't have to
//   be that data structure) to move TCBs between different thread queues.
//   Starter code for a ready queue is provided to you
// - Separate join and finished "queues" can also help when supporting joining.
//   Example join and finished queue entry types are provided above

static int currentThreadIndex = 0;
static int totalThreads = 0;

static unordered_map<int, TCB*> threadTable;

struct sigaction _sigAction;
struct itimerval timer;
static bool interrupts_enabled = true;

// Queues
static deque<TCB *> ready_queue;
static deque<finished_queue_entry_t *> finishedQueue;  // Queue for finished threads
static deque<join_queue_entry_t *> joinQueue;      // Queue for threads waiting to join
static deque<TCB *> blockedQueue;      // Queue for threads waiting to join

ucontext_t mainContext;

// Interrupt Management --------------------------------------------------------

// Start a countdown timer to fire an interrupt
static void startInterruptTimer()
{
	// TODO
}

// Block signals from firing timer interrupt
static void disableInterrupts()
{
	// assert(interrupts_enabled);
    sigprocmask(SIG_BLOCK,&_sigAction.sa_mask, NULL);
    interrupts_enabled = false;
}

// Unblock signals to re-enable timer interrupt
static void enableInterrupts()
{
	// assert(!interrupts_enabled);
    interrupts_enabled = true;
    sigprocmask(SIG_UNBLOCK,&_sigAction.sa_mask, NULL);
}

// Queue Management ------------------------------------------------------------

// Add TCB to the back of the ready queue
void addToReadyQueue(TCB *tcb)
{
	ready_queue.push_back(tcb);
}

void addToJoinQueue(TCB *tcb, int tid)
{
	join_queue_entry* entry;
	entry->tcb = tcb;
	entry->waiting_for_tid = tid;
	joinQueue.push_back(entry);
}

void addToFinishedQueue(TCB *tcb, void* result)
{
	finished_queue_entry_t* entry;
	entry->tcb = tcb;
	entry->result = result;
	finishedQueue.push_back(entry);
}

void addToBlockedQueue(TCB *tcb)
{
	blockedQueue.push_back(tcb);
}

// Removes and returns the first TCB on the ready queue
// NOTE: Assumes at least one thread on the ready queue
TCB *popFromReadyQueue()
{
	// assert(!ready_queue.empty());
	if (ready_queue.empty()){
		return NULL;
	}

	TCB *ready_queue_head = ready_queue.front();
	ready_queue.pop_front();
	return ready_queue_head;
}

finished_queue_entry_t *popFromFinishedQueue()
{
	// assert(!finishedQueue.empty());
	if (finishedQueue.empty()){
		return NULL;
	}

	finished_queue_entry_t *finishedQueueHead = finishedQueue.front();
	finishedQueue.pop_front();
	return finishedQueueHead;
}

TCB *popFromBlockedQueue()
{
	// assert(!ready_queue.empty());
	if (blockedQueue.empty()){
		return NULL;
	}

	TCB *blockedQueueHead = blockedQueue.front();
	blockedQueue.pop_front();
	return blockedQueueHead;
}

// Removes the thread specified by the TID provided from the ready queue
// Returns 0 on success, and -1 on failure (thread not in ready queue)
int removeFromReadyQueue(int tid)
{
	for (deque<TCB *>::iterator iter = ready_queue.begin(); iter != ready_queue.end(); ++iter)
	{
		if (tid == (*iter)->getId())
		{
			ready_queue.erase(iter);
			return 0;
		}
	}

	// Thread not found
	return -1;
}

TCB* removeFromFinishedQueue(int tid)
{

	for (deque<TCB *>::iterator iter = finishedQueue.begin(); iter != finishedQueue.end(); ++iter)
	{
		if (tid == (*iter)->getId())
		{
			TCB* finishedTCB = *iter;
			finishedQueue.erase(iter);
			return finishedTCB;
		}
	}

	// Thread not found
	return NULL;
}

TCB* removeFromBlockedQueue(int tid)
{

	for (deque<TCB *>::iterator iter = blockedQueue.begin(); iter != blockedQueue.end(); ++iter)
	{
		if (tid == (*iter)->getId())
		{
			TCB* blockedTCB = *iter;
			blockedQueue.erase(iter);
			return blockedTCB;
		}
	}

	// Thread not found
	return NULL;
}

// Helper functions ------------------------------------------------------------

// Switch to the next ready thread
static void switchThreads()
{
    // flag is a local stack variable to each thread
    volatile int flag = 0;

    // getcontext() will "return twice" - Need to differentiate between the two
	TCB* currentTCB = threadTable[currentThreadIndex];
	int ret_val = currentTCB->saveContext();
    cout << "SWITCHING: currentThread = " << currentThreadIndex<< endl;

    // If flag == 1 then it was already set below so this is the second return
    // from getcontext (run this thread)
    if (flag == 1) {
        return;
    }

    // This is the first return from getcontext (switching threads)
    flag = 1;
	TCB* nextReady = popFromReadyQueue();
	if (nextReady != NULL){
		currentThreadIndex = nextReady->getId();
		cout << "NEW THREAD: currentThread = " << currentThreadIndex<< endl;
		nextReady->loadContext();
		nextReady->setState(RUNNING);
	}
	else{
		cout << "NO READY THREAD; REMAINING ON "<<currentThreadIndex<<endl;
	}
}

// Library functions -----------------------------------------------------------

// The function comments provide an (incomplete) summary of what each library
// function must do

// Starting point for thread. Calls top-level thread function
void stub(void *(*start_routine)(void *), void *arg)
{
	// TODO
    void *retval = start_routine(arg);
    uthread_exit(retval);
}

// Define a signal handler function for SIGVTALRM
void timerHandler(int signum) {
    uthread_yield();
}

// unsure if main thread should be a tcb or just a maincontext object in this file; might create default construcotr without stub() call
int uthread_init(int quantum_usecs)
{
	// Initialize any data structures
	// Setup timer interrupt and handler
	// Create a thread for the caller (main) thread

	_sigAction.sa_handler = timerHandler;
    if (sigemptyset(&_sigAction.sa_mask) < -1) {
        cout << "ERROR: Failed to empty to set" << endl;
        exit(1);
    }

    if (sigaddset(&_sigAction.sa_mask, SIGVTALRM)) {
        cout << "ERROR: Failed to add to set" << endl;
        exit(1);
    }

    if (sigaction(SIGVTALRM, &_sigAction, NULL) != 0) {
            return -1;
    }

    //setting up the timer
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = quantum_usecs;
    timer.it_interval = timer.it_value;
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) != 0) {
            return -1;
    }

	// Create a TCB for the main thread
	currentThreadIndex = 0;  // Set the index of the main thread
	TCB *mainTCB = new TCB(currentThreadIndex, GREEN, nullptr, nullptr, RUNNING);
	mainTCB->saveContext();

	// Add the main thread's TCB to the thread table
	threadTable[currentThreadIndex] = mainTCB;
	totalThreads++;
	
	return 0;
}

int uthread_create(void *(*start_routine)(void *), void *arg)
{
	// Create a new thread and add it to the ready queue

	// Allocate TCB and stack
	TCB *tcb = new TCB(totalThreads, GREEN, start_routine, arg, READY);
	threadTable[totalThreads] = tcb;
	totalThreads++;

	addToReadyQueue(tcb);

	return tcb->getId();
}

int uthread_join(int tid, void **retval)
{
	// If the thread specified by tid is already terminated, just return
	// If the thread specified by tid is still running, block until it terminates
	// Set *retval to be the result of thread if retval != nullptr
	if (threadTable[tid]->getState() == FINISHED){
		return 0;
	}

	addToJoinQueue(threadTable[currentThreadIndex], tid);
	while (threadTable[tid]->getState() != FINISHED){
		uthread_suspend(uthread_self());
		// called through suspend
		// uthread_yield();
	}
	

	finished_queue_entry_t* finishedEntryTCB = removeFromFinishedQueue(tid);
	TCB* finishedTCB = finishedEntryTCB->tcb;
	if (retval != nullptr){
		retval = finishedEntryTCB->result;
	}

	// Delete exited thread, will prob have to account for possible deletion in other thread
	delete finishedTCB;

	return 0;
}

int uthread_yield(void)
{
	// Prevent an interrupt from stopping us in the middle of a switch.
	disableInterrupts();

	TCB* runningThread = threadTable[currentThreadIndex];
	// Move running thread onto the ready list. if it's blocked, it will be on blocked queue already instead, if it's finished it will be on finished queue
	if (runningThread->getState == RUNNING){
		runningThread->setState(READY);
		if (currentThreadIndex != 0)
			addToReadyQueue(runningThread);
	}
	switchThreads(); // Switch to the new thread.
	runningThread->setState(RUNNING);

	// Delete any threads on the finished list.
	// finished_queue_entry_t* finishedTCB;
	// while ((finishedTCB = popFromFinishedQueue()) != NULL) {
	// 	delete finishedTCB->tcb;
	// }

	enableInterrupts();
}

void uthread_exit(void *retval)
{
	// If this is the main thread, exit the program
	if (currentThreadIndex == 0)	exit(0);
	// Move any threads joined on this thread back to the ready queue
	// Move this thread to the finished queue

	TCB* tcb = threadTable[currentThreadIndex];
	tcb->setState(FINISHED);
	addToFinishedQueue(tcb, retval);


	for (deque<join_queue_entry_t>::iterator iter = joinQueue.begin(); iter != joinQueue.end(); ++iter)
	{
		if (tid == (*iter)->waiting_for_tid)
		{
			uthread_resume(tid);
			joinQueue.erase(iter);
		}
	}

	uthread_yield();
}

int uthread_suspend(int tid)
{
	// Move the thread specified by tid from whatever state it is
	// in to the block queue
	TCB* suspendTCB = threadTable[tid];
	if (tcb == NULL)	return -1;

	State suspendState = suspendTCB->getState()
	suspendTCB->setState(BLOCK);
	if (suspendState == READY){
		int suspendTid = suspendTCB->getId();
		removeFromReadyQueue(suspendTid);
	}
	addToBlockedQueue(suspendTCB);
	if (tid == uthread_self())	uthread_yield();

	return 0;
}

int uthread_resume(int tid)
{
	// Move the thread specified by tid back to the ready queue
	TCB* resumeThread = removeFromBlockedQueue(tid);
	if (resumeThread == NULL)	return -1;
	resumeThread->setState(READY);

	addToReadyQueue(resumeThread);
	return 0;
}

int uthread_self()
{
	// should be the same as currentThreadIndex
	return threadTable[currentThreadIndex]->getId();
}

int uthread_get_total_quantums()
{
	// TODO
	return 0;
}

int uthread_get_quantums(int tid)
{
	// TODO
	return 0;
}
