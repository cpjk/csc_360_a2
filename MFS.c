#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

typedef struct Flow {
  int id;
  int input_file_order; // lower is closer to start - higher priority
  int arrival_time_10ms;
  int trans_time_10ms;
  unsigned int priority; // 1-10 inclusive, highest to lowest priority
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

int main(int argc, char **argv) {
}
