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
int time_from_start_us();

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
    if(flow_1_ptr->arrival_time_us < flow_2_ptr->arrival_time_us) { // flow_1 goes first
      return -1;
    }
    else if(flow_1_ptr->arrival_time_us > flow_2_ptr->arrival_time_us) { // flow_2 goes first
      return 1;
    }
    else { // fall back to transmission time
      if(flow_1_ptr->trans_time_us < flow_2_ptr->trans_time_us) { // flow_1 goes first
        return -1;
      }
      else if(flow_1_ptr->trans_time_us > flow_2_ptr->trans_time_us) { // flow_2 goes first
        return 1;
      }
      else { // fall back to input file order
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
  pthread_cond_broadcast(&trans_cvar); // signal on convar for other threads to wake up
  pipe_activity = NOT_IN_USE; // release the output pipe
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
  pthread_mutex_lock(&trans_mtx);

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
    printf("Flow %i waiting at  %ld\n", flow->number, time_from_start_us());
    fflush(stdout);
    pthread_cond_wait(&trans_cvar, &trans_mtx);
  }

  // update queue
  remove_flow_from_queue(flow);

  pipe_activity = IN_USE;
  // unlock mutex
  pthread_mutex_unlock(&trans_mtx);
}

int time_from_start_us() {
      gettimeofday(&curr_time_timeval, NULL);
      return ((curr_time_timeval.tv_sec - start_time_timeval.tv_sec) * 1000000L
       + curr_time_timeval.tv_usec) - start_time_timeval.tv_usec;
}

// entry point for each thread created
// flow is the Flow corresponding to this thread.
void *thread_func(void *flowItem) {
  Flow *flow = (Flow *)flowItem ;

  usleep(flow->arrival_time_us); // sleep for given number of microseconds until arriving
  printf("Flow %i arriving at %ld\n", flow->number, time_from_start_us());
  fflush(stdout);

  request_pipe(flow);
  printf("Flow %i starting at %ld\n", flow->number, time_from_start_us());
  fflush(stdout);

  // sleep for transmission time
  usleep(flow->trans_time_us);

  printf("Flow %i finished at %ld\n", flow->number, time_from_start_us());
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
