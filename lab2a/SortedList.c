#include "SortedList.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element){
  if(list == NULL || element == NULL)
    return;
  
  SortedListElement_t *front = list->next;
  SortedListElement_t *back = list;
  
  while(front != NULL && (strncmp(front->key, element->key, 3) < 0)){
    /*
    //check if list is corrupted
    if(front->next != NULL)
      if(front->next->prev != front)
	return;
    if(front->prev != NULL)
      if(front->prev->next != front)
	return;
    */
    back = front;
    front = front->next;
  }

  if(opt_yield & INSERT_YIELD)
    sched_yield();
  
  if(front == NULL){
    //put element at the end of the list
    back->next = element;
    element->prev = back;
    element->next = NULL;
  }
  else{

    //element goes between front and back
    back->next = element;
    front->prev = element;
    element->prev = back;
    element->next = front;
  }
}

int SortedList_delete(SortedListElement_t *element){
  //check for null pointers
  if(element == NULL || element->prev == NULL){
    return 1;
  }

  //if element is last on the list
  if(element->next == NULL){
    if(element->prev->next != element)
      return 1;
    element->prev->next = NULL;
    return 0;
  }
  
  //check to make sure next->prev and prev->next point both point to this node
  if(element->next->prev != element || element->prev->next != element){
    return 1;
  }
  
  if(opt_yield & DELETE_YIELD)
    sched_yield();
  element->next->prev = element->prev;
  element->prev->next = element->next;
  return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key){
  if(list == NULL){
    return NULL;
  }
  SortedListElement_t *cur = list->next;
  while(cur != NULL && (strncmp(cur->key, key, 3) != 0)){
    /*
    //check if list is corrupted
    if(cur->prev != NULL)
      if(cur->prev->next != cur)
	return NULL;
    if(cur->next != NULL)
      if(cur->next->prev != cur)
	return NULL;
    */
    if(opt_yield & LOOKUP_YIELD)
      sched_yield();
    cur = cur->next;
  }
  return cur;
}

int SortedList_length(SortedList_t *list){
  int length = 0;
  if(list == NULL)
    return -1;

  if(opt_yield & LOOKUP_YIELD)
    sched_yield();
  SortedListElement_t *cur = list->next;
  
  for(; cur != NULL; length++){
    if(cur->prev->next != cur){
      //list is corrupted
      return -1;
    }
    if(opt_yield & LOOKUP_YIELD)
      sched_yield();
    cur = cur->next;
  }
  
  return length;
}

