#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

// constant number of flows


// FUNCTIONS:

// when multiple flows arrive at the same time and no others are transmitting
// int highest_priority_same_arrival(list of flows) - resolve highest priority between multiple flows
//   receive list of flows and return (id of?) highest priority one
//   can do this by sorting based on custom sort function and return ary[0]
//
// int flow_comp_same_arrival(const void * flow1, const void * flow2) 
//   return -1 if flow1 higher (goes BEFORE) flow2, 0 if equal,
//   1 if flow1 is lower (goes AFTER flow2

// when a flow finishes
// int flow_comp(const void * flow1, const void * flow2) 
//   return -1 if flow1 higher (goes BEFORE) flow2, 0 if equal,
//   1 if flow1 is lower (goes AFTER flow2


// execution:
// 1 main thread.
// main thread maintains array of worker flow threads
// main thread starts ticking every tenths of a second
//
// when one of the threads' starting time is reached, start the thread with the current s tarting time that has should be started based on the priority ranking function
//
// join on that thread
//
// after join, pick next thread from the threads that have arrival times less than or equal to the current time based on appropriate function
//
// every tick, output appropriate status messages:
// flow arrives
// flow waits
// flow starts transmission
// flow finishes transmission
//


/*
 *
 * time calculation may be a nightware! 
 * Beware of float, int, unsigned int conversion.
 * you could use gettimeofday(...) to get down to microseconds!
 *
 * */

# header files


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
// mutexes: 

typedef struct Flow {
  int number;
  int input_file_order; // lower is closer to start - higher priority
  int arrival_time_100ms;
  int trans_time_100ms;
  int priority; // 1-10 inclusive, highest to lowest priority
} Flow;

#define MAXFLOW 500; the maximum number of flows

Flow flowList[MAXFLOW];   // parse input in an array of flow
Flow *queueList[MAXFLOW];  // store waiting flows while transmission pipe is occupied.
pthread_t thrList[MAXFLOW]; // each thread executes one flow
pthread_mutex_t trans_mtx = PTHREAD_MUTEX_INITIALIZER ;
pthread_cond_t trans_cvar = PTHREAD_COND_INITIALIZER ;

void requestPipe(Flow *item) {
  // lock mutex
  pthread_mutex_lock(&trans_mtx);

  if transmission pipe available && queue is empty {
    ...do some stuff..
      unlock mutex;
    return ;
  }

  // add item in queue, sort the queue according rules

  // printf(Waiting...);
  // key point here..
  // wait till pipe to be available and be at the top of the queue
  while( not at front of queue ) {
    // wait till signalled AND then wait until can acquire lock
    // if signalled and can acquire the lock, we enter the next iteration of the loop, which will check if at front of queue.
    // if not, we re-wait on trans_cvar (unlock and wait)
    pthread_cond_wait(&trans_cvar, &trans_mtx);
  }

  // update queue

  // unlock mutex;
  pthread_mutex_unlock;
}

void releasePipe() {
  // signal on convar for other threads to wake up
  // release the output pipe
}

// entry point for each thread created
void *thrFunction(void *flowItem) {

  Flow *item = (Flow *)flowItem ;

  // wait for arrival
  usleep(...)
    printf(Arrive...);

  requestPipe(item) ;
  printf(Start...)

  // sleep for transmission time
  usleep(...)

  releasePipe(item);
  printf(Finish..);
}

/* int main() { */
/*   for(0 - numFlows) */
/*     // create a thread for each flow */ 
/*     pthread_create(&thrList[i], NULL, thrFunction, (void *)&flowList[i]) ; */

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

  // Flow *flow_list = malloc(sizeof(Flow) * num_flows);

  printf("num flows: %i\n", num_flows);
  for(int i = 0; i < num_flows; i++) {
    int flow_number, arrival_time, trans_time, priority;
    fscanf(fp, "%i:%i,%i,%i\n", &flow_number, &arrival_time, &trans_time, &priority);
    flow_list[i].number = flow_number;
    flow_list[i].input_file_order = i;
    flow_list[i].arrival_time_100ms = arrival_time;
    flow_list[i].trans_time_100ms = trans_time;
    flow_list[i].priority = priority;
    printf("read %i:%i,%i,%i\n", flow_number, arrival_time, trans_time, priority);
  }
  fclose(fp);

  for(int i = 0; i < num_flows; i++) {
    printf("%i:%i,%i,%i, %i\n", flow_list[i].number, flow_list[i].input_file_order,
        flow_list[i].arrival_time_100ms, flow_list[i].trans_time_100ms, flow_list[i].priority);
  }

  free(flow_list);
}
