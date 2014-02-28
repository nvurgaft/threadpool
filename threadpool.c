#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "threadpool.h"

#define TRUE 1
#define FALSE 0

	/*	perrors are used for lib function which may return the desired errno
	*	printfs were disabled for assignment detail
	*/

// creates and inits the threadpool queue
threadpool* create_threadpool(int num_threads_in_pool) {

 	//1. input sanity check  	
	if (num_threads_in_pool<=0) {
		printf("No threads alloted for jobs, exiting!\n");
		return NULL;
	}
	if (num_threads_in_pool>MAXT_IN_POOL) {
		printf("Exceeding maximum allowed threads in pool!\n");
		return NULL;
	}

 	//2. initialize the threadpool structure
	threadpool* tpool = (threadpool*) malloc(sizeof(threadpool));
	if (tpool==NULL) {
		printf("Creating Threadpool returned null!\n");
		return NULL;
	}
 	
	tpool->num_threads = num_threads_in_pool;
	tpool->qsize = 0;
	tpool->threads = NULL;
	tpool->qhead = NULL;
	tpool->qtail = NULL;
	tpool->shutdown = 0;
	tpool->dont_accept = 0;

 	//3. initialized mutex and conditional variables
	if (pthread_mutex_init(&(tpool->qlock), NULL)!=0) {
		perror("unable to init mutex");
	}
	if (pthread_cond_init(&(tpool->q_empty), NULL)!=0) {
		perror("unable to init cond q_empty");
	}
	if (pthread_cond_init(&(tpool->q_not_empty), NULL)!=0) {
		perror("unable to init cond q_not_empty");
	}

 	//4. create the threads, the thread init function is do_work and its argument is the initialized threadpool. 
	pthread_t* thread = (pthread_t*) malloc(sizeof(pthread_t)*tpool->num_threads);
	tpool->threads = thread;

	int i;
	// the threads are created here and are joined at "destroy_threadpool(threadpool*)"" !
	for (i=0; i<tpool->num_threads; i++) {
		if (pthread_create(&(tpool->threads[i]), NULL, do_work, tpool)!=0) {
			perror("Unable to create thread!");
		}
	}

	// eventually return the finished threadpool
	return tpool;
}

// PRODUCER: enter a job to the queue
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg) {
	
	threadpool* tpool = (threadpool*) from_me;

	// 1. Create work_t structure and init it with the routine and argument.
	work_t* work = (work_t*) malloc(sizeof(work_t)); //work is freed in do_work
	work->arg = arg;
	work->routine = dispatch_to_here;
	work->next = NULL;

	// lock mutex
	pthread_mutex_lock(&(tpool->qlock));

	// we shouldn't be accepting new jobs if the don't accept flag is lit
	if (tpool->dont_accept==1) {
		printf("threadpool is set to don't accept, returning!\n");
		return;
	}
	// add work to the queue (attach a new head to the queue) !!!
	if (tpool->qsize==0) {
		// if list is empty
		tpool->qhead = work;
		tpool->qtail = work;
		pthread_cond_signal(&(tpool->q_not_empty));
	} else {
		// if not, add to and replace qtail
		tpool->qtail->next = work;
		tpool->qtail = work;
	}
	tpool->qsize++;

	// unlock mutex
	pthread_mutex_unlock(&(tpool->qlock));
	// in case of good dispatch
	//printf("routine [%d] dispatched\n", (int) work->arg);

}

// CONSUMER: the work function (runnable) of the thread
void* do_work(void* p) {

	//intf("thread function is up!\n");
	threadpool* tpool = (threadpool*) p;
	work_t* work;
	while (TRUE) {
		pthread_mutex_lock(&(tpool->qlock));
		// 1. If destruction process has begun, exit thread
		while (tpool->qsize==0) {
			if (tpool->shutdown==1) {
				// printf("Shutdown flag raised, shutting down thread\n");
				pthread_mutex_unlock(&(tpool->qlock));
				pthread_exit(0);
			}

			// 2. If the queue is empty, wait (no job to make, so wait until cond not empty is lit)
			pthread_mutex_unlock(&(tpool->qlock));
			pthread_cond_wait(&(tpool->q_not_empty), &(tpool->qlock));
		
			//3. Check again destruction flag.
			if (tpool->shutdown==1) {
				// printf("Shutdown flag raised, shutting down thread\n");
				pthread_mutex_unlock(&(tpool->qlock));
				pthread_exit(0);
			}
		}

		// if by some extreme case qsize got a negative value, kill everything!
		if (tpool->qsize<=0) {
			printf("attempting to execute a job while qsize is 0\n");
			pthread_exit(NULL);
		}

		// remove the task from the queue (remove the tail from the queue) !!!
		work = tpool->qhead;	
		if (tpool->qsize)tpool->qsize--;

		if (tpool->qsize==0) {
			tpool->qtail = NULL;
			tpool->qhead = NULL;
		} else {
			tpool->qhead = work->next;
		}

		if (tpool->qsize==0 && tpool->shutdown==0) {
			// if the queue is empty and we're not shutting down yet
			pthread_cond_signal(&(tpool->q_empty));
		}

		// unlock mutex
		pthread_mutex_unlock(&(tpool->qlock));

		//	now we can finally run the routine :P
		(work->routine) (work->arg);
		//printf("routine [%d] done!\n", (int) work->arg);
		free(work);	// work is malloc'd is dispatch
	}
}

// destroy and deallocate the threadpool
void destroy_threadpool(threadpool* destroyme) {

	threadpool* tpool = (threadpool*) destroyme;

	//printf("\n1. shutting down threadpool\n");
	pthread_mutex_lock(&(tpool->qlock));
	// set don’t_accept flag to 1
	tpool->dont_accept = 1;
	
	// wait for queue to become empty
	while (tpool->qsize>0) {
		pthread_cond_wait(&(tpool->q_empty), &(tpool->qlock));
	}
	
	// set shutdown flag to 1
	tpool->shutdown = 1;
	
	// signal threads that wait on empty queue, so they can wake up, see shutdown flag and exit.	
	pthread_cond_broadcast(&(tpool->q_not_empty));
	pthread_mutex_unlock(&(tpool->qlock));
	//printf("2. joining the threads\n");
	// join the threads
	int i;
	for (i=0; i<tpool->num_threads; i++) {
		
		if (pthread_join(tpool->threads[i], NULL)!=0) {
			perror("unable to join thread!\n");
		}
	}
	// dealloc everything
	//printf("3. destroying conditionals and mutex\n");
	pthread_mutex_destroy(&(tpool->qlock));
	pthread_cond_destroy(&(tpool->q_empty));
	pthread_cond_destroy(&(tpool->q_not_empty));
	
	//rintf("4. freeing the threadpool\n");
	free(tpool->threads);
	free(tpool);
}
