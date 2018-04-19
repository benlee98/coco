#include "P1.h"

#define REQUEST 0
#define PICKUP  1
#define DROP    2
#define WAIT    3
#define THINK   4
#define EAT     5
#define PHINUM  5

int min(int a, int b) {
  if (a < b) {
    return a;
  } else {
    return b;
  }
}

int max(int a, int b) {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}

size_t my_strlen(const char *s)
{
    size_t len = 0;
    while (*s++)
        len++;

    return len;
}

void myPrint (const char *s)
{
    write (STDOUT_FILENO, s, my_strlen(s));
}

void printBehaviour(int id, int chopstick, int select){
  char idBuffer[2];
  char semIDBuffer[2];
  itoa(idBuffer, id);
  itoa(semIDBuffer, chopstick);

  switch(select){
    case(REQUEST): {
      // myPrint("\033[1;31m");
      myPrint("\nPhilosopher "); myPrint(idBuffer); myPrint(" requests chopstick ");
      myPrint(semIDBuffer); myPrint("\n");
      break;
    }
    case(PICKUP): {
      // myPrint("\033[1;32m");
      myPrint("\nPhilosopher "); myPrint(idBuffer); myPrint(" picks up chopstick ");
      myPrint(semIDBuffer); myPrint("\n");
      break;
    }
    case(DROP): {
      // myPrint("\033[1;33m");
      myPrint("\nPhilosopher "); myPrint(idBuffer); myPrint(" puts down chopstick ");
      myPrint(semIDBuffer); myPrint("\n");
      break;
    }
    case(WAIT): {
      // myPrint("\033[1;34m");
      myPrint("\nPhilosopher "); myPrint(idBuffer); myPrint(" patiently waits for ");
      myPrint("chopstick "); myPrint(semIDBuffer); myPrint("\n");
      break;
    }
    case(THINK): {
      // myPrint("\033[1;35m");
      myPrint("\nPhilosopher "); myPrint(idBuffer); myPrint(" is thinking...\n");
      break;
    }
    case(EAT): {
      // myPrint("\033[1;36m");
      myPrint("\nPhilosopher "); myPrint(idBuffer); myPrint(" is EATING...\n");
      break;
    }
  }
  // myPrint("\033[0m");
}

void think(int i) {
  printBehaviour(i, 0, THINK);
  for (int i = 0; i < 1000000000; i++){
    asm("nop");
  }
}

void eat(int i) {
  printBehaviour(i, 0, EAT);
  for (int i = 0; i < 1000000000; i++){
    asm("nop");
  }
}

void sem_wait(int id, int semID, sem_t* ptr) {
  // If the semaphore is less than 1, add request to queue
  printBehaviour(id, semID, REQUEST);
  if ((ptr + semID)->semaphore-- < 1) {
    // (ptr + semID)->nextInQueue = (ptr + semID)->arraySize;
    // Place process in waiting state and put pid in queue
    (ptr + semID)->array[(ptr + semID)->arraySize] = getpid();
    (ptr + semID)->idArray[(ptr + semID)->arraySize++] = id;
    printBehaviour(id, semID, WAIT);
    wait();
  } else {
    printBehaviour(id, semID, PICKUP);
  }
}

void sem_signal(int id, int semID, sem_t* ptr) {
  printBehaviour(id, semID, DROP);
  if ((ptr + semID)->arraySize != 0) {
    unwait((ptr + semID)->array[0]);
    printBehaviour((ptr + semID)->idArray[0], semID, PICKUP);
    for (int i = 0; i != (ptr + semID)->arraySize; i++) {
      (ptr + semID)->array[i] = (ptr + semID)->array[i+1];
      (ptr + semID)->idArray[i] = (ptr + semID)->idArray[i+1];
    }
    (ptr + semID)->arraySize--;
  } else {
    (ptr + semID)->semaphore += 1;
  }
}

void thinkAndEat(int i, sem_t* ptr) {
  while( 1 ) {
    think(i);
    sem_wait(i, min(i, (i+1)%PHINUM), ptr);
    sem_wait(i, max(i, (i+1)%PHINUM), ptr);
    eat(i);
    sem_signal(i, min(i, (i+1)%PHINUM), ptr);
    sem_signal(i, max(i, (i+1)%PHINUM), ptr);
  }
}

void main_P1() {
  // Create some shared memory and return id
  int id = shm_make(PHINUM * sizeof(sem_t));
  // Return the pointer to shared memory
  sem_t* ptr = (sem_t*) shm_get(id);
  memset( ptr, 0, PHINUM * sizeof( sem_t ) );

  for (int i = 0; i < PHINUM; i++) {
    (ptr + i)->semaphore = 1;
    int pid = fork();
    if (pid == 0) {
      thinkAndEat(i, ptr);
    }
  }
  exit( EXIT_SUCCESS );
}
