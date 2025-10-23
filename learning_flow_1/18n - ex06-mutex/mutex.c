/*
 * mutex.c
 *
 *  Created on: Mar 19, 2016
 *      Author: jiaziyi
 */
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define NTHREADS 50
void *increase_counter(void *);

void *counter_monitor(void *);

int  counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t counter_cond = PTHREAD_COND_INITIALIZER;


int main()
{
	int ret;

	// Create monitoring thread
	pthread_t monitor_thread;
	ret = pthread_create(&monitor_thread, NULL, counter_monitor, NULL);
	if (ret){
		printf("Error in creating monitor thread: %d\n", ret);
		return ret;
	}

	//create 50 threads of increase_counter, each thread adding 1 to the counter
	pthread_t threads[NTHREADS];
	for (int i=0; i<50; i++){
		ret = pthread_create(&threads[i], NULL, increase_counter, NULL);
		if (ret){
			printf("Error in creating thread %d: %d\n", i, ret);
			return ret;
		} else {
			printf("Created thread %d\n", i);
		}

	}

	// Wait for all threads to complete
	for (int i=0; i<50; i++){
		pthread_join(threads[i], NULL);
	}

	// Wait for monitor thread to complete
	pthread_join(monitor_thread, NULL);

	printf("\nFinal counter value: %d\n", counter);

	// Clean up
	pthread_mutex_destroy(&counter_mutex);
	pthread_cond_destroy(&counter_cond);

	return 0;
}

void *increase_counter(void *arg)
{
	// Venter på at counter_mutex gives
	pthread_mutex_lock(&counter_mutex);

	printf("Thread number %ld, working on counter. The current value is %d\n", (long)pthread_self(), counter);
	int i = counter;
	usleep (10000); //simulate the data processing
	counter = i+1;

	// Signal the monitor thread that counter has changed
	pthread_cond_signal(&counter_cond);

	pthread_mutex_unlock(&counter_mutex);
	return 0;
}

void *counter_monitor(void *arg)
{
	int last_percentage = 0;

	// Låser couter_mutex
	pthread_mutex_lock(&counter_mutex);

	while (counter < NTHREADS) {
		// Venter på counter_cond signal
		// Så venter den på counter_mutex gives
		// Så låser den counter_mutex og returnerer
		pthread_cond_wait(&counter_cond, &counter_mutex);

		// Calculate percentage
		int percentage = (counter * 100) / NTHREADS;

		// Check if we've reached a new 10% milestone
		if (percentage >= last_percentage + 10 && percentage <= 90) {
			printf("%d%% finished!\n", percentage);
			last_percentage = percentage;
		}

		// Check if all threads are done
		if (counter >= NTHREADS) {
			printf("All finished!\n");
			break;
		}
	}

	pthread_mutex_unlock(&counter_mutex);
	return 0;
}