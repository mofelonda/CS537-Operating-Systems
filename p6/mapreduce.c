/**
 * Merge Sort and utility functions sourced from geeksforgeeks.org
 * MergeSort(): https://www.geeksforgeeks.org/merge-sort-for-linked-list/
 * FrontBackSplit(): https://www.geeksforgeeks.org/merge-sort-for-linked-list/
 * SortedMerge(): https://www.geeksforgeeks.org/merge-two-sorted-linked-lists/
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>
#include "unistd.h"
#include "mapreduce.h"

// Linked list that maps keys to values
struct keyVal {
  char *key;
  char *val;
  struct keyVal *next;
};

// Structure for partition information
typedef struct partStruct {
    struct keyVal *head;
    pthread_mutex_t lock;
} partStruct;

// Function pointers
Partitioner partitioner;
Reducer reducer;
Mapper mapper;

// Trackers
int NUM_PARTITIONS;
int NUM_FILES;

// Counters for multi-threading
int currFile;
int nextPart;

// Locks
pthread_key_t glob_var_key;
pthread_mutex_t fileLock;

// Structs
struct partStruct *partitions;
struct partStruct *backups;
int *nextKey;
char **FILES;

/**
 * Initializes global variables
 */
void initialize(int argc, char *argv[], Mapper map, int num_mappers,
Reducer reduce, int num_reducers, Partitioner partition, int num_partitions) {
  // Function pointers
  partitioner = partition;
  mapper = map;
  reducer = reduce;

  // Trackers
  NUM_PARTITIONS = num_partitions;
  NUM_FILES = argc - 1;

  // Data structures
  partitions = malloc((num_partitions + 1) * sizeof(struct partStruct));
  backups = malloc((num_partitions + 1) * sizeof(struct partStruct));
  nextKey = calloc(num_partitions, sizeof(int));
  FILES = &argv[1];
}

/** 
 * Provided function
 * Take a given key and map it to a number, from 0 to num_partitions - 1
 */
unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
  unsigned long hash = 5381;
  int c;
  while ((c = *key++) != '\0')
    hash = hash * 33 + c;
  return hash % num_partitions;
}

/**
 * Ensures that keys are in a sorted order across the partitions
 * If there is only 1 partition, return 0
 * If the key is empty, return -1
 */
unsigned long MR_SortedPartition(char *key, int num_partitions) {
  if (num_partitions == 1) {
    return 0;
  }
  if (strlen(key) == 0) {
    return -1;
  }

  int sigBits = 0;
  int temp = num_partitions;

  // Calculate log2(num_partitions)
  while (temp != 1) {
    temp /= 2;
    sigBits++;
  }

  // Shift significant bits and assign to partition
  unsigned long partition = (unsigned)atoi(key) >> (32 - sigBits);

  return partition;
}

/**
 * Sourced from geeksforgeeks.org's Merge Sort for Linked Lists
 *
 * "Split the nodes of the given list into front and back halves,  
 * and return the two lists using the reference parameters.  
 * If the length is odd, the extra node should go in the front list.  
 * Uses the fast/slow pointer strategy." 
 */
void FrontBackSplit(struct keyVal* source, struct keyVal** frontRef,
                                            struct keyVal** backRef) {
  if (source == NULL || source->next == NULL) {
    *frontRef = source;
    *backRef = NULL;
    return;
  }

  struct keyVal* slow = source;
  struct keyVal* fast = source->next;

  /* Advance 'fast' two nodes, and advance 'slow' one node */
  while (fast != NULL) {
    fast = fast->next;
    if (fast != NULL) {
      slow = slow->next;
      fast = fast->next;
    }
  }

  /* 'slow' is before the midpoint in the list, so split it in two
  at that point. */
  *frontRef = source;
  *backRef = slow->next;
  slow->next = NULL;
}

/**
 * Sourced from geeksforgeeks.org's Merge two sorted linked lists
 *
 * "Takes two lists sorted in increasing 
 * order, and splices their nodes together 
 * to make one big sorted list which  
 * is returned." 
 */
struct keyVal* SortedMerge(struct keyVal* a, struct keyVal* b) {
  struct keyVal* result = NULL;

  /* Base cases */
  if (a == NULL)
    return b;
  else if (b == NULL)
    return a;

  /* Pick either a or b, and recur */
  if (strcmp(a->key, b->key) <= 0) {
    result = a;
    result->next = SortedMerge(a->next, b);
  } else {
    result = b;
    result->next = SortedMerge(a, b->next);
  }
  return result;
}

/**
 * Sourced from geeksforgeeks.org's Merge Sort for Linked Lists
 *
 * "Sorts the linked list by changing next pointers (not data)"
 */
void MergeSort(struct keyVal** head) {
  struct keyVal* a;
  struct keyVal* b;

  /* Base case -- length 0 or 1 */
  if (*head == NULL || (*head)->next == NULL)
    return;

  /* Split head into 'a' and 'b' sublists */
  FrontBackSplit(*head, &a, &b);

  /* Recursively sort the sublists */
  MergeSort(&a);
  MergeSort(&b);

  /* answer = merge the two sorted lists together */
  *head = SortedMerge(a, b);
}

/**
 * Sets head of backups equal to the passed partition
 * If the head is not null, set next partition equal to current head
 * Then set head equal to passed partition
 */
void backup(struct keyVal *currPart, int partition_number) {
  if (backups[partition_number].head == NULL) {
    backups[partition_number].head = currPart;
    currPart->next = NULL;
  } else {
    currPart->next = backups[partition_number].head;
    backups[partition_number].head = currPart;
  }
}

/**
 * Returns a pointer to the value passed by MR_Emit(),
 * If the flag is set, reset flag and return NULL
 * If key is not found in the passed partition, return NULL
 */
char *get_next(char *key, int partition_number) {
  struct keyVal *currPart = partitions[partition_number].head;

  pthread_mutex_lock(&partitions[partition_number].lock);
  if (nextKey[partition_number] == 1) {
    nextKey[partition_number] = 0;
    pthread_mutex_unlock(&partitions[partition_number].lock);
    return NULL;
  }
  if (currPart != NULL) {
    if (strcmp(currPart->key, key) == 0) {
      partitions[partition_number].head = currPart->next;
      if (currPart->next != NULL) {
        if (strcmp(currPart->next->key, key) != 0) {
          nextKey[partition_number] = 1;
          pthread_mutex_unlock(&partitions[partition_number].lock);
          backup(currPart, partition_number);
          return currPart->val;
        }
        pthread_mutex_unlock(&partitions[partition_number].lock);
        backup(currPart, partition_number);
        return currPart->val;
      } else {
        pthread_mutex_unlock(&partitions[partition_number].lock);
        backup(currPart, partition_number);
        return currPart->val;
      }
    }
  }
  pthread_mutex_unlock(&partitions[partition_number].lock);
  return NULL;
}

/**
 * Takes key-value pairs from various mappers,
 * storing them in a partition so later reducers can access them
 */
void MR_Emit(char *key, char *value) {
  if (strlen(key) == 0) {
    return;
  }

  // Create new key-value node
  int partitionNum = partitioner(key, NUM_PARTITIONS);
  struct keyVal *new = malloc(sizeof(struct keyVal));
  new->key = malloc(sizeof(char) * (strlen(key) + 1));
  strcpy(new->key, key);
  new->val = value;

  pthread_mutex_lock(&partitions[partitionNum].lock);
  struct keyVal *iter = partitions[partitionNum].head;
  if (iter == NULL) {
    partitions[partitionNum].head = new;
    new->next = NULL;
    pthread_mutex_unlock(&partitions[partitionNum].lock);
    return;
  }

  // Add to linked list
  new->next = partitions[partitionNum].head;
  partitions[partitionNum].head = new;
  pthread_mutex_unlock(&partitions[partitionNum].lock);
  return;
}

/**
 * Helper function that calls the mapper,
 * assigning files to mapper threads
 */
void *mapping() {
  while (1) {
    char *file;

    pthread_mutex_lock(&fileLock);
    if (NUM_FILES <= currFile) {
      pthread_mutex_unlock(&fileLock);
      return NULL;
    }
    file = FILES[currFile];
    currFile++;

    pthread_mutex_unlock(&fileLock);
    mapper(file);
  }
}

/**
 * Helper function that calls the reducer,
 * assigning partitions to reducer threads
 */
void *reduction() {
  while (1) {
    pthread_mutex_lock(&fileLock);
    if (NUM_PARTITIONS <= nextPart) {
      pthread_mutex_unlock(&fileLock);
      return NULL;
    }

    struct keyVal *iter = partitions[nextPart].head;
    int *part = malloc(sizeof(int));
    *part = nextPart;
    pthread_setspecific(glob_var_key, part);
    nextPart++;
    pthread_mutex_unlock(&fileLock);

    int *glob_spec_var = pthread_getspecific(glob_var_key);
    MergeSort(&partitions[*glob_spec_var].head);
    iter = partitions[*glob_spec_var].head;

    while (iter != NULL) {
      reducer(iter->key, get_next, *glob_spec_var);
      iter = partitions[*glob_spec_var].head;
    }
    free(part);
  }
}

/**
 * Creates threads and runs the computation
 */
void MR_Run(int argc, char *argv[], Mapper map, int num_mappers,
Reducer reduce, int num_reducers, Partitioner partition, int num_partitions) {
  // Exit if there is no file specified
  if (argc < 2) {
    printf("No file specified\n");
    exit(0);
  }

  // Inititalize global variables
  initialize(argc, argv, map, num_mappers, reduce,
          num_reducers, partition, num_partitions);

  // Create mapper threads
  int kMapThreads = num_mappers;
  pthread_t mappers[kMapThreads];
  for (int i = 0; i < num_mappers; i++) {
    if (i < NUM_FILES) {
      pthread_create(&mappers[i], NULL, mapping, NULL);
    }
  }
  // Join mapper threads
  for (int i = 0; i < num_mappers; i++) {
    if (i < NUM_FILES) {
      pthread_join(mappers[i], NULL);
    }
  }

  // Create reducer threads
  int kRedThreads = num_reducers;
  pthread_t reducers[kRedThreads];
  pthread_key_create(&glob_var_key, NULL);
  for (int i = 0; i < num_reducers; i++) {
    if (i < NUM_PARTITIONS) {
      pthread_create(&reducers[i], NULL, reduction, NULL);
    }
  }
  // Join reducer threads
  for (int i = 0; i < num_reducers; i++) {
    if (i < NUM_PARTITIONS) {
      pthread_join(reducers[i], NULL);
    }
  }

  // Free partitions and corresponding keys
  for (int i = 0; i < NUM_PARTITIONS; i++) {
    struct keyVal *iter = backups[i].head;
    struct keyVal *temp;
    while (iter != NULL) {
      free(iter->key);
      temp = iter;
      iter = iter->next;
      free(temp);
    }
  }

  // Free structs
  free(backups);
  free(partitions);
  free(nextKey);
}
