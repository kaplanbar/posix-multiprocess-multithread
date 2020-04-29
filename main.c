#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>


struct prime_list {
	int size;
	int primes[1024];
	// To guarentee interprocess multi-thread safety
	pthread_mutex_t lock;
};

// From https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c
void* create_shared_memory(size_t size) {
	int protection = PROT_READ | PROT_WRITE;
	int visibility = MAP_SHARED | MAP_ANONYMOUS;
	return (struct prime_list*) mmap(NULL, size, protection, visibility, -1, 0);
}


// Returns if the number `x` is prime or not
int is_prime(int x) {
	for(int i=2;i*i<=x;i++) {
		if(x % i == 0) {
			return 0;
		}
	}
	return 1;
}

// Pushes the number `x` to given prime_list struct multi-thread safely
void push_prime(int x, struct prime_list* pr, pthread_mutex_t* lock) {
	pthread_mutex_lock(lock);

	pr->primes[pr->size] = x;
	pr->size = pr->size + 1;

	pthread_mutex_unlock(lock);

}

struct thread_data {
	int range_start;
	int range_end;
	struct prime_list* shmem;
	int index1;
	int index2;
};

// The function that will handle the operations done by threads
void* handle_thread(void* arg) {
	struct thread_data* td = (struct thread_data*) arg;
	int range_start = td->range_start;
	int range_end = td->range_end;
	struct prime_list* shmem = td->shmem;
	int index1 = td->index1;
	int index2 = td->index2;
	pthread_mutex_t* lock = &(td->shmem->lock);
	printf("Thread %d.%d: searching in %d-%d\n", index1, index2, range_start, range_end);
	fflush(stdout);
	for(int i=range_start;i<=range_end;i++) {
		if(is_prime(i)) {
			push_prime(i, shmem, lock);
		}
	}
}

// The function that will handle the operations done by the child processes of the master process
void handle_child_process(int range_start, int range_end,
	   						struct prime_list* shmem, int index, int nt) {

	printf("Slave %d: Started. Interval %d-%d\n", index, range_start, range_end);
	fflush(stdout);

	// In this array, we will hold the id's of the threads.
	pthread_t tid[nt];

	// This will hold the data that will be passed to threads.
	struct thread_data td[nt];

	for(int i=0;i<nt;i++) {
		if(i != nt-1) {
			int len = (range_end - range_start + 1) / nt;
			td[i].range_start = range_start + i * len;
			td[i].range_end = td[i].range_start + len - 1;
			td[i].shmem = shmem;
			td[i].index1 = index;
			td[i].index2 = i + 1;
			pthread_create(&tid[i], NULL, handle_thread, (void*)(&td[i]));
		}
		else {
			int len = (range_end - range_start + 1) / nt + (range_end - range_start + 1) % nt;
			td[i].range_start = range_end - len + 1;
			td[i].range_end = range_end;
			td[i].shmem = shmem;
			td[i].index1 = index;
			td[i].index2 = i + 1;
			pthread_create(&tid[i], NULL, handle_thread, (void*)(&td[i]));
		}
	}

	// Wait until all of the threads finish their execution.
	for(int i=0;i<nt;i++) {
		pthread_join(tid[i], NULL);
	}
	printf("Slave %d: Done.\n", index);
	fflush(stdout);
	exit(0);
}

int main(int argc, char* argv[]) {
	if(argc < 5) {
		fprintf(stderr, "You should provide all of the arguments.");
		return 1;
	}

	int range_start = atoi(argv[1]);

	int range_end = atoi(argv[2]);

	// The number of processes
	int np = atoi(argv[3]);

	// The number of threads
	int nt = atoi(argv[4]);

	// The `np` and `nt` should be positive numbers.
	if(np <= 0 || nt <= 0) {
		fprintf(stderr, "Number of processes or number of threads are invalid");
		return 1;
	}

	printf("Master: Started.\n");
	fflush(stdout);

	
	struct prime_list* shmem = create_shared_memory(sizeof(struct prime_list));

	// To make the mutex process shared
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&(shmem->lock), &attr);

	pthread_mutexattr_destroy(&attr);

	// It will create `np` processes
	for(int i=0;i<np;i++) {
		int pid = fork();
		if(pid == 0) {
			// Only the newly created process enters this block.
			if(i != np - 1) {
				// We will give each process the range with length `floor(length / np)`
				int len = (range_end - range_start + 1) / np;
				int start = range_start + i * len;
				handle_child_process(start, start + len - 1, shmem, i + 1, nt);
			}
			else {
				// Last process will also get the remaining numbers to simplify
				// the implementation of the program
				int len = (range_end - range_start + 1) / np + (range_end - range_start + 1) % np;
				handle_child_process(range_end - len + 1, range_end, shmem, i + 1, nt);
			}
			break;
		}
	}

	// Wait for child processes to wait finish execution.
	int status;
	do {
		status = wait(0);
		if(status == -1 && errno != ECHILD) {
			fprintf(stderr, "Error occured in one of child processes.");
			return 1;
		}
	} while(status > 0);

	printf("Master: Done. Prime numbers are: ");
	for(int i=0;i<shmem->size;i++) {
		if(i != shmem->size - 1)
			printf("%d, ", shmem->primes[i]);
		else
			printf("%d\n", shmem->primes[i]);
	}

	// Free the allocated shared memory
	munmap(shmem, sizeof(struct prime_list));


	return 0;
}
