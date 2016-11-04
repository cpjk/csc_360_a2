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
// each thread sleeps until it arrives, then tries to write to the output pipe by calling request_pipe
// lock mutex
// if pipe is available and the wait queue is empty, transmit and then release the lock
// else, add yourself to the queue (we already have the lock, which will guard writing to the queue),
//   re-sort the queue
//   then enter a loop that will wait for you to be at the front of the queue and for the convar to be signalled
// when convar is signalled, we try to acquire the lock, and then check if we are at the front of the queue. if not, re-wait
// on the convar to be signalled again.
// if we are at the front of the queue, remove ourself from the queue, release the lock, then return from request_pipe,
// which causes us to transmit.
// after transmitting, we call release_pipe, which signals to the convar, causing the waiting threads to contend for the transmission
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
#include <unistd.h>
#include <sys/time.h>

typedef struct Flow {
  int number;
  int input_file_order; // lower is closer to start - higher priority
  int arrival_time_us;
  int trans_time_us;
  int priority; // 1-10 inclusive, highest to lowest priority
} Flow;

#define MAXFLOW 500 // the maximum number of flows
#define IN_USE 1
#define NOT_IN_USE 0

Flow flow_list[MAXFLOW];   // parse input in an array of flow
Flow *queue[MAXFLOW];  // store waiting flows while transmission pipe is occupied.
unsigned int queue_size = 0;
pthread_t thread_list[MAXFLOW]; // each thread executes one flow
pthread_mutex_t trans_mtx = PTHREAD_MUTEX_INITIALIZER ;
pthread_cond_t trans_cvar = PTHREAD_COND_INITIALIZER ;
unsigned int pipe_activity;
struct timeval start_time_timeval, curr_time_timeval;

int comp_func(const void *, const void *);


void remove_flow_from_queue(Flow *flow) {
  int found = 0;

  Flow *temp_queue[queue_size-1];

  int temp_queue_index = 0;
  for(int i = 0; i < queue_size; i++) {
    if(queue[i]->number != flow->number) { // copy other queues into temp
      temp_queue[temp_queue_index++] = queue[i];
    }
    else {
      found = 1; // the flow was found in the queue
    }
  }

  if(found != 1) { // the given flow was not present in the queue
    printf("Flow %i not found in queue.\n", flow->number);
    fflush(stdout);
    return;
  }

  for(int i = 0; i < queue_size; i++) {
    queue[i] = NULL; // set queue index to NULL
  }

  queue_size--;

  for(int i = 0; i < queue_size; i++) {
    queue[i] = temp_queue[i]; // copy remaining flows back into the queue
  }


  qsort(queue, queue_size, sizeof(Flow*), comp_func);
}

void add_flow_to_queue(Flow *flow) {
  if(queue_size == MAXFLOW) {
    printf("Max queue size limit reached.");
    exit(-1);
  }

  queue[queue_size] = flow;
  queue_size++;

  qsort(queue, queue_size, sizeof(Flow*), comp_func);
}

// flow_1 and flow_2 are Flow **
int comp_func(const void *flow_1, const void *flow_2) {
  // -1 if flow_1 before flow_2
  // 0 if same
  // 1 if flow_2 goes before flow_1

  Flow *flow_1_ptr = *((Flow **) flow_1);
  Flow *flow_2_ptr = *((Flow **) flow_2);

 // lower priority goes first
  if(flow_1_ptr->priority < flow_2_ptr->priority) { // flow_1 goes first
    return -1;
  }
  else if(flow_1_ptr->priority > flow_2_ptr->priority) { // flow_2 goes first
    return 1;
  }
  else { // fall back to arrival time
    /* printf("%i and %i have same priority. falling back to arrival time\n", flow_1_ptr->number, flow_2_ptr->number); */
    if(flow_1_ptr->arrival_time_us < flow_2_ptr->arrival_time_us) { // flow_1 goes first
      return -1;
    }
    else if(flow_1_ptr->arrival_time_us > flow_2_ptr->arrival_time_us) { // flow_2 goes first
      return 1;
    }
    else { // fall back to transmission time
      /* printf("%i and %i have same arrival time. falling back to trans time\n", flow_1_ptr->number, flow_2_ptr->number); */
      if(flow_1_ptr->trans_time_us < flow_2_ptr->trans_time_us) { // flow_1 goes first
        return -1;
      }
      else if(flow_1_ptr->trans_time_us > flow_2_ptr->trans_time_us) { // flow_2 goes first
        return 1;
      }
      else { // fall back to input file order
        /* printf("%i and %i have same trans time. falling back to file order time\n", flow_1_ptr->number, flow_2_ptr->number); */
        if(flow_1_ptr->input_file_order < flow_2_ptr->input_file_order) { // flow_1 goes first
          return -1;
        }
        else if(flow_1_ptr->input_file_order > flow_2_ptr->input_file_order) { // flow_2 goes first
          return 1;
        }
        else {
          printf("We should never reach here. Somehow the threads were not able to agree on an ordering.\n");
          exit(-1);
        }
      }
    }
  }
}

void release_pipe() {
  // signal on convar for other threads to wake up
  pthread_cond_broadcast(&trans_cvar);
  // release the output pipe
  pipe_activity = NOT_IN_USE;
}

// wait on mutex to check queue and write to it
// --> next, check if someone else is transmitting
// if not, and no one else is on the queue, set pipe_activity = IN_USE
// we set IN_USE inside the lock, and read it inside the lock.
//
// we set NOT_IN_USE outside the lock, but only the currently using thread can do this. will this be ok?
// possibilities: pipe in use, other thread checks it, sees in use, waits.
//                pipe in use, release use, other thread checks it, sees not in use, starts using
//
//                need to make sure we set NOT_IN_USE before signalling end of transmission

// release mutex,

// flow is the Flow corresponding to this thread
void request_pipe(Flow *flow) {
  // lock mutex
  /* printf("Flow %i is outside the critical section\n", flow->number); */
  /* fflush(stdout); */
  pthread_mutex_lock(&trans_mtx);

  /* printf("Flow %i is inside the critical section.\n", flow->number); */
  /* fflush(stdout); */

  // if the queue is empty and the pipe is not in use, start transmitting
  if(!queue[0] && pipe_activity == NOT_IN_USE) {
    /* printf("Flow %i Found the queue empty.\n", flow->number); */
    fflush(stdout);
    pipe_activity = IN_USE;
    pthread_mutex_unlock(&trans_mtx);
    return;
  }

  // add flow to queue, sort the queue according rules
  add_flow_to_queue(flow);

  // wait till signalled AND then wait until can acquire lock
  // if signalled and can acquire the lock, we enter the next iteration of the loop, which will check if at front of queue.
  // if not, we re-wait on trans_cvar (unlock and wait)
  while(queue[0]->number != flow->number || pipe_activity == IN_USE) {
    gettimeofday(&curr_time_timeval, NULL);
    printf("Flow %i waiting at  %ld\n",
        flow->number,
        ((curr_time_timeval.tv_sec - start_time_timeval.tv_sec) * 1000000L
         + curr_time_timeval.tv_usec) - start_time_timeval.tv_usec);
    fflush(stdout);
    pthread_cond_wait(&trans_cvar, &trans_mtx);
  }

  // update queue
  remove_flow_from_queue(flow);

  // unlock mutex
  pthread_mutex_unlock(&trans_mtx);
}

// entry point for each thread created
// flow is the Flow corresponding to this thread.
void *thread_func(void *flowItem) {

  Flow *flow = (Flow *)flowItem ;

  usleep(flow->arrival_time_us); // sleep for given number of microseconds until arriving
  gettimeofday(&curr_time_timeval, NULL);
  printf("Flow %i arriving at %ld\n",
      flow->number,
      ((curr_time_timeval.tv_sec - start_time_timeval.tv_sec) * 1000000L
       + curr_time_timeval.tv_usec) - start_time_timeval.tv_usec);
  fflush(stdout);

  request_pipe(flow) ;
  gettimeofday(&curr_time_timeval, NULL);
  printf("Flow %i starting at %ld\n",
      flow->number,
      ((curr_time_timeval.tv_sec - start_time_timeval.tv_sec) * 1000000L
       + curr_time_timeval.tv_usec) - start_time_timeval.tv_usec);
  fflush(stdout);

  // sleep for transmission time
  usleep(flow->trans_time_us);

  gettimeofday(&curr_time_timeval, NULL);
  printf("Flow %i finished at %ld\n",
      flow->number,
      ((curr_time_timeval.tv_sec - start_time_timeval.tv_sec) * 1000000L
       + curr_time_timeval.tv_usec) - start_time_timeval.tv_usec);
  fflush(stdout);
  release_pipe(flow);
}

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

  for(int i = 0; i < num_flows; i++) {
    int flow_number, arrival_time, trans_time, priority;
    fscanf(fp, "%i:%i,%i,%i\n", &flow_number, &arrival_time, &trans_time, &priority);
    flow_list[i].number = flow_number;
    flow_list[i].input_file_order = i;
    flow_list[i].arrival_time_us = (int) arrival_time * 100000;
    flow_list[i].trans_time_us = (int) trans_time * 100000;
    flow_list[i].priority = priority;
  }
  fclose(fp);

  gettimeofday(&start_time_timeval, NULL);
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
