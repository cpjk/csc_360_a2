/*
 *
 * time calculation may be a nightware! 
 * Beware of float, int, unsigned int conversion.
 * you could use gettimeofday(...) to get down to microseconds!
 *
 * */

/* typedef struct _flow */
/* { */
/*     float arrivalTime ; */
/*     float transTime ; */
/*     int priority ; */
/*     int id ; */
/* } flow; */

// ALGORITHM:
// read thread info
// start all threads
// each thread sleeps until it arrives, then tries to write to the output pipe by calling requestPipe
// lock mutex
// if pipe is available and the wait queue is empty, transmit and then release the lock
// else, add yourself to the queue (we already have the lock, which will guard writing to the queue),
//   re-sort the queue
//   then enter a loop that will wait for you to be at the front of the queue and for the convar to be signalled
// when convar is signalled, we try to acquire the lock, and then check if we are at the front of the queue. if not, re-wait
// on the convar to be signalled again.
// if we are at the front of the queue, remove ourself from the queue, release the lock, then return from requestPipe,
// which causes us to transmit.
// after transmitting, we call releasePipe, which signals to the convar, causing the waiting threads to contend for the transmission
//
// num threads: main thread and one for each flow
// the threads work independently
// mutexes: 1. to guard writing to the queue.
// main thread will be idle
// flows will be represented with structs that hold all the data about that thread
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

typedef struct Flow {
  int number;
  int input_file_order; // lower is closer to start - higher priority
  int arrival_time_mis;
  int trans_time_mis;
  int priority; // 1-10 inclusive, highest to lowest priority
} Flow;

#define MAXFLOW 500 // the maximum number of flows

Flow flow_list[MAXFLOW];   // parse input in an array of flow
Flow *queueList[MAXFLOW];  // store waiting flows while transmission pipe is occupied.
pthread_t thread_list[MAXFLOW]; // each thread executes one flow
pthread_mutex_t trans_mtx = PTHREAD_MUTEX_INITIALIZER ;
pthread_cond_t trans_cvar = PTHREAD_COND_INITIALIZER ;

// item is the Flow corresponding to this thread
/* void requestPipe(Flow *item) { */
/*   // lock mutex */
/*   pthread_mutex_lock(&trans_mtx); */

/*   /1* if transmission pipe available && queue is empty { *1/ */
/*   /1*   ...do some stuff.. *1/ */
/*   /1*     unlock mutex; *1/ */
/*   /1*   return ; *1/ */
/*   /1* } *1/ */

/*   // add item in queue, sort the queue according rules */

/*   // printf(Waiting...); */
/*   // key point here.. */
/*   // wait till pipe to be available and be at the top of the queue */
/*   while( not at front of queue ) { */
/*     // wait till signalled AND then wait until can acquire lock */
/*     // if signalled and can acquire the lock, we enter the next iteration of the loop, which will check if at front of queue. */
/*     // if not, we re-wait on trans_cvar (unlock and wait) */
/*     pthread_cond_wait(&trans_cvar, &trans_mtx); */
/*   } */

/*   // update queue */

/*   // unlock mutex; */
/*   pthread_mutex_unlock; */
/* } */

void releasePipe() {
  // signal on convar for other threads to wake up
  // release the output pipe
}

// entry point for each thread created
// item is the Flow corresponding to this thread.
void *thread_func(void *flowItem) {

  Flow *item = (Flow *)flowItem ;

  /* printf("inside flow %i.\n", item->number); */
  usleep(item->arrival_time_mis); // sleep for given number of microseconds until arriving
  printf("flow %i Arrived\n", item->number);

  /* requestPipe(item) ; */
  /* printf(Start...) */

  /* // sleep for transmission time */
  /* usleep(...) */

  /* releasePipe(item); */
  /* printf(Finish..); */
}

/* int main() { */
/*   for(0 - numFlows) */
/*     // create a thread for each flow */ 
/*     pthread_create(&thread_list[i], NULL, thread_func, (void *)&flow_list[i]) ; */

/*   // wait for all threads to terminate */
/*   pthread_join(...) */

/*     // destroy mutex & condition variable */

/*     return 0; */
/* } */

int main(int argc, char **argv) {
  if(!argv[1]) {
    printf("No file argument provided. Exiting.\n");
    exit(-1);
  }

  // parse input file
  FILE *fp;
  fp = fopen(argv[1], "r");
  int num_flows;

  fscanf(fp, "%i\n", &num_flows);

  printf("num flows: %i\n", num_flows);
  for(int i = 0; i < num_flows; i++) {
    int flow_number, arrival_time, trans_time, priority;
    fscanf(fp, "%i:%i,%i,%i\n", &flow_number, &arrival_time, &trans_time, &priority);
    flow_list[i].number = flow_number;
    flow_list[i].input_file_order = i;
    flow_list[i].arrival_time_mis = (int) arrival_time * 100000;
    flow_list[i].trans_time_mis = (int) trans_time * 100000;
    flow_list[i].priority = priority;
    printf("read %i:%i,%i,%i\n", flow_number, arrival_time, trans_time, priority);
    /* printf("read number: %i file_order: %i arrival_time: %i trans_time: %i priority: %i\n", flow_list[i].number, flow_list[i].input_file_order, flow_list[i].arrival_time_mis, flow_list[i].trans_time_mis, flow_list[i].priority); */
  }
  fclose(fp);

  // initialize all of the flow threads
  for(int i = 0; i < num_flows; i++) {
    pthread_create(&thread_list[i], NULL, thread_func, (void *)&flow_list[i]) ;
  }

  // wait for all threads to complete
  for(int i = 0; i < num_flows; i++) {
    pthread_join(thread_list[i], NULL) ;
  }

  // destroy the condition variable and mutex
  pthread_mutex_destroy(&trans_mtx);
  pthread_cond_destroy(&trans_cvar);
}
