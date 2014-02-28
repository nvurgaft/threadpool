/** C source file stub */

#include "threadpool.c"

// routine signature
int dispatch_function(void*);

// ****************************************************
// * the dispatch function every threadpool job calls *
// ****************************************************
int dispatch_function(void *arg) {

	printf("%d\n", (int) arg);
	return 0;
}

// ********
// * main *
// ********
int main(int argc, char const *argv[]) {
	
	// check params
	// ./threadpool <pool-size> <max-number-of-jobs>
	if (argc<3) {
		printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
		exit(-1);
	}
	int NTHREADS = atoi(argv[1]); 	//max number of threads
	int NJOBS = atoi(argv[2]);		//max number of jobs
	
	// number of jobs in the pool cannot be negetive
	if (NJOBS<=0) {
		perror("Invalid <max-number-of-jobs>, should be a positive number\n");
		exit(-1);
	}
	
	//printf("IN MAIN: creating a threadpool with %d threads for max %d jobs ===\n", NTHREADS, NJOBS);
	// create the threadpool and arg the number of threads it should handle
	threadpool* working_threadpool = create_threadpool(NTHREADS);
	if (working_threadpool==NULL) {
		printf("pool is null\n");
		exit(-1);
	}
	
	//printf("IN MAIN: dispatching jobs to threadpool.\n");
	int i, j;
	for (i=0; i<NJOBS; i++) {
		// enter jobs in here
		for (j=0; j<3; j++) {
			dispatch(working_threadpool, dispatch_function, (void*) i);
			sleep(1);
		}
	}
	//printf("IN MAIN: destroying threadpool.\n");
	// destroy the threadpool
	destroy_threadpool(working_threadpool);

	//printf("IN MAIN: done! goodbye ..\n");
	return 0;
}
