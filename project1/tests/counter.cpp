#include "../lib/uthread.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#define DEFAULT_TIME_SLICE 1000
#define MAX_COUNT 6
#define NUM_THREADS 3

void *worker(void *arg) {
	/* Retrieve our thread ID */
	int my_tid = uthread_self();
	int ret_val = 0;
	for (int i = 0; i < MAX_COUNT; i++){
		printf("From thread : %d, count: %d\n", my_tid, i);
		if (i != 0 && i % 3 == 0){
			uthread_yield();
		} 
		ret_val++;
	} 
	uthread_exit(&ret_val);
}

int main(int argc, char *argv[]){
	/* Initialize the default time slice (only overridden if passed in) */
	int quantum_usecs = DEFAULT_TIME_SLICE;
	int *threads = new int[NUM_THREADS];

	int ret = uthread_init(quantum_usecs);
	if (ret != 0)
	{
		cerr << "uthread_init FAIL!\n"
			<< endl;
		exit(1);
	}
	srand(time(NULL));

	/* Create a thread pool of threads passing in the points per thread */
	for (int i = 0; i < NUM_THREADS; i++)
	{
		int tid = uthread_create(worker, nullptr);
		threads[i] = tid;
	}

	uthread_yield();

	int ret_val = 0;
	for (int i = 0; i < NUM_THREADS; i++){
		int val;
		uthread_join(threads[i], &val);
		ret_val += val;
	}
	printf("Final calculated value: %d", ret_val);

	return 0;
}
