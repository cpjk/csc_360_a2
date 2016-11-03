#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

typedef struct Flow {
  int number;
  int input_file_order; // lower is closer to start - higher priority
  int arrival_time_100ms;
  int trans_time_100ms;
  int priority; // 1-10 inclusive, highest to lowest priority
} Flow;

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

  Flow *flow_list = malloc(sizeof(Flow) * num_flows);

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

  for(int i = 0; i < num_flows; i++) {
    printf("%i:%i,%i,%i, %i\n", flow_list[i].number, flow_list[i].input_file_order,
        flow_list[i].arrival_time_100ms, flow_list[i].trans_time_100ms, flow_list[i].priority);
  }
}
