#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

struct task{
  int id; // unique id
  int priority; // unique priority
  int periodic; // repeating periodicity
  void (*function)(); // pointer to function to be performed
  time_t executeTime; // time to execute
  struct task **dependencies; // task it's depended on
  int depCount; // number of dependencies
  int completed; // whether task is finished
};

struct heap{
  struct task **arr;
  int size;
  int capacity;
  pthread_mutex_t lock; // lock to prevent simultaneous use
};

void swap(struct task** a,struct task** b){
  struct task *temp = *a;
  *a = *b;
  *b = temp;
}

// after insertion maintain max heap property
void heapifyUp(struct heap *h, int index) {
    int parent = (index - 1) / 2;
    if (index && h->arr[parent]->priority < h->arr[index]->priority) {
        swap(&h->arr[parent], &h->arr[index]);
        heapifyUp(h, parent);
    }
}

// after extraction maintain max heap property
void heapifyDown(struct heap *h, int index) {
    int left = 2 * index + 1;
    int right = 2 * index + 2;
    int largest = index;

    if (left < h->size && h->arr[left]->priority > h->arr[largest]->priority)
        largest = left;
    if (right < h->size && h->arr[right]->priority > h->arr[largest]->priority)
        largest = right;

    if (largest != index) {
        swap(&h->arr[index], &h->arr[largest]);
        heapifyDown(h, largest);
    }
}

// insert task to be executed later based on priority
void insertTask(struct heap *h, struct task *t) {
    pthread_mutex_lock(&h->lock); // locks the mutex (cant be used by other functions)
    if (h->size == h->capacity) {
        h->capacity *= 2; // if full double the capacity
        h->arr = realloc(h->arr, h->capacity * sizeof(struct task*));
    }
    h->arr[h->size] = t;
    heapifyUp(h, h->size);
    h->size++;
    pthread_mutex_unlock(&h->lock); // unlock the mutex (open to be used by other functions)
}

// extract task to be executed 
struct task* extractMax(struct heap *h) {
    pthread_mutex_lock(&h->lock); // locks the mutex (cant be used by other functions)
    if (h->size == 0){
      pthread_mutex_unlock(&h->lock);  // unlock the mutex (open to be used by other functions)
      return NULL; // empty heap
    }
    struct task *top = h->arr[0];
    h->arr[0] = h->arr[h->size - 1];
    h->size--;
    heapifyDown(h, 0);
    pthread_mutex_unlock(&h->lock); // unlock the mutex (open to be used by other functions)
    return top;
}

// to check if the tasks it is dependent on is completed (completed = 1) 
int dependenciesMet(struct task *t) {
    for(int i=0; i<t->depCount; i++) {
        if(t->dependencies[i]->completed == 0)
            return 0;
    }
    return 1;
}

// to prevent starvation increase priority
void ageTasks(struct heap *h) {
    pthread_mutex_lock(&h->lock); // locks the mutex (cant be used by other functions)
    for(int i=0; i<h->size; i++) {
        h->arr[i]->priority += 1; // simple aging
        heapifyUp(h, i);
    }
    pthread_mutex_unlock(&h->lock); // unlock the mutex (open to be used by other functions)
}

// tasks 
void taskA() { printf("Task A executed!\n"); }
void taskB() { printf("Task B executed!\n"); }
void taskC() { printf("Task C executed!\n"); }
void taskD() { printf("Task D executed!\n"); }
void taskE() { printf("Task E executed!\n"); }

// Moves tasks from timerHeap → readyHeap when time arrives
// Picks highest-priority ready task
// Checks dependencies
// Runs task in a new thread
// Reschedules if periodic
// Applies aging
// Sleeps briefly
void* schedulerLoop(void *arg) {
    struct heap **heaps = (struct heap**)arg;
    struct heap *readyHeap = heaps[0]; // ready to be executed 
    struct heap *timerHeap = heaps[1]; // next to be executed

    while(1) {
        time_t now = time(NULL); // now = 00:00:00 
      
        // Move tasks from timerHeap to readyHeap
        pthread_mutex_lock(&timerHeap->lock); // locks the mutex (cant be used by other functions)
        while(timerHeap->size > 0 && timerHeap->arr[0]->executeTime <= now) {
            struct task *t = extractMax(timerHeap);
            insertTask(readyHeap, t);
        }
        pthread_mutex_unlock(&timerHeap->lock);// unlock the mutex (open to be used by other functions)

        // Execute highest priority and ready task
        struct task *t = extractMax(readyHeap);
        if(t && dependenciesMet(t)) {
            pthread_t thread;
            pthread_create(&thread /*id*/, NULL/*thread attributes*/, (void*(*)(void*))t->function/*the function it runs*/, NULL/*argument to the function*/);
          /* we wrote (void*(*)(void*))t->function..... expected void *(*start_routine)(void*)..... (void*(*)(void*)) = force cast so C allows it*/
            pthread_detach(thread);// detach thread from parent

            t->completed = 1;

            // Reschedule if periodic
            if(t->periodic > 0) {
                t->executeTime = now + t->periodic;
                t->completed = 0;
                insertTask(timerHeap, t);
            } else {
                free(t);
            }
        } else if(t) {
            // reinsert if dependencies not met
            insertTask(readyHeap, t);
        }

        ageTasks(readyHeap); // Apply aging
        usleep(50000);       // Sleep 50ms prevent busy waiting (ie. do work or sleep prevent wastage of cpu cycles)
    }
    return NULL;
}

int main() {
    struct heap readyHeap = { malloc(10 * sizeof(struct task*)), 0, 10, PTHREAD_MUTEX_INITIALIZER };
    struct heap timerHeap = { malloc(10 * sizeof(struct task*)), 0, 10, PTHREAD_MUTEX_INITIALIZER };
    struct heap *heaps[2] = { &readyHeap, &timerHeap };

    pthread_t schedulerThread;
    pthread_create(&schedulerThread, NULL, schedulerLoop, heaps);

    int taskId = 1;
    while (1) {
        int choice;
        printf("1. Add Task\n2.Exit\nChoice: ");
        scanf("%d", &choice);

        if (choice == 1){
            int priority, periodic, funcChoice, delay;
            printf("Enter priority: "); 
            scanf("%d", &priority);
            printf("Enter delay in seconds (0 = run now): "); 
            scanf("%d", &delay);
            printf("Periodic interval in sec (0=no): ");
            scanf("%d", &periodic);
            printf("Task function (1-A 2-B 3-C 4-D 5-E): "); 
            scanf("%d", &funcChoice);
    
            task *t = malloc(sizeof(struct task));
            t->id = taskId++;
            t->priority = priority;
            t->periodic = periodic;
            t->executeTime = time(NULL) + delay;
            t->depCount = 0;
            t->dependencies = NULL;
            t->completed = 0;

            if(funcChoice == 1) t->function = taskA;
            else if(funcChoice == 2) t->function = taskB;
            else if(funcChoice == 3) t->function = taskC;
            else if(funcChoice == 4) t->function = taskD;
            else t->function = taskE;
    
            if(delay > 0)
              insertTask(&timerHeap, t);
            else
              insertTask(&readyHeap, t);

             printf("Task %d added (priority=%d, delay=%d, periodic=%d)\n",t->id, t->priority, delay, periodic);
        }else if (choice == 2) {
            printf("Exiting\n");
            break;
        }

        else printf("Invalid choice\n");
    }

    pthread_join(schedulerThread, NULL);
    return 0;
}
