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
int transmitting_flow = 0;

int comp_func(const void *, const void *);
float time_from_start_us();

// Remove the given flow from the queue
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

// add the given flow to the queue
void add_flow_to_queue(Flow *flow) {
  if(queue_size == MAXFLOW) {
    printf("Max queue size limit reached.");
    exit(-1);
  }

  queue[queue_size] = flow;
  queue_size++;

  qsort(queue, queue_size, sizeof(Flow*), comp_func);
}

// Comparison function for qsort
// flow_1 and flow_2 are Flow**
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

// Release the output pipe for other threads to use
void release_pipe() {
  pipe_activity = NOT_IN_USE; // release the output pipe
  pthread_cond_broadcast(&trans_cvar); // signal on convar for other threads to wake up
}

// attempt to transmit on the output pipe. If the pipe is busy, wait our turn
// flow is the Flow corresponding to this thread
void request_pipe(Flow *flow) {
  // lock mutex
  pthread_mutex_lock(&trans_mtx);

  // if the queue is empty and the pipe is not in use, start transmitting
  if(!queue[0] && pipe_activity == NOT_IN_USE) {
    /* printf("Flow %i Found the queue empty.\n", flow->number); */
    fflush(stdout);
    pipe_activity = IN_USE;
    transmitting_flow = flow->number;
    pthread_mutex_unlock(&trans_mtx);
    return;
  }

  // add flow to queue, sort the queue according rules
  add_flow_to_queue(flow);

  // if signalled and can acquire the lock, we enter the next iteration of the loop, which will check if we can transmit.
  // if not, we re-wait on trans_cvar (unlock and wait)
  while(queue[0]->number != flow->number || pipe_activity == IN_USE) {
    printf("Flow %i waits for the finish of flow %2d at  %.2f\n", flow->number, transmitting_flow, time_from_start_us());
    fflush(stdout);
    pthread_cond_wait(&trans_cvar, &trans_mtx);
  }

  // update queue
  remove_flow_from_queue(flow);

  // mark pipe as being in use
  pipe_activity = IN_USE;

  transmitting_flow = flow->number;

  // unlock mutex
  pthread_mutex_unlock(&trans_mtx);
}

// obtain the current time relative to the start time
//
float secs_from_us(int us) {
  return ((float) us) / 1000000.0;
}
float time_from_start_us() {
      gettimeofday(&curr_time_timeval, NULL);
      float curr_time = (float) (
        (
           (curr_time_timeval.tv_sec - start_time_timeval.tv_sec)
           * 1000000L
           + curr_time_timeval.tv_usec
        )
        - start_time_timeval.tv_usec
      );
      return  curr_time / 1000000.0;
}

// entry point for each thread created
// flow is the Flow corresponding to this thread.
void *thread_func(void *flowItem) {
  Flow *flow = (Flow *)flowItem ;

  usleep(flow->arrival_time_us); // sleep for given number of microseconds until arriving
  printf("Flow %2d arrives: arrival time (%.2f), transmission time (%.1f), priority (%2d).\n",
      flow->number,
      time_from_start_us(),
      secs_from_us(flow->trans_time_us),
      flow->priority);
  fflush(stdout);

  request_pipe(flow);
  printf("Flow %i starts its transmission at time %.2f.\n", flow->number, time_from_start_us());
  fflush(stdout);

  usleep(flow->trans_time_us); // sleep for transmission time

  printf("Flow %i finishes its transmission at time %.2f.\n", flow->number, time_from_start_us());
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

  // mark the start time
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
