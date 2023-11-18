#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include "fixed-point.h"
#include <debug.h>
#include <list.h>
#include <stdint.h>

/* States in a thread's life cycle. */
enum thread_status
{
   THREAD_RUNNING, /* Running thread. */
   THREAD_READY,   /* Not running but ready to run. */
   THREAD_BLOCKED, /* Waiting for an event to trigger. */
   THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */
/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
extern bool thread_mlfqs;
struct thread
{
   /* Owned by thread.c. */
   tid_t tid;                 /* Thread identifier. */
   enum thread_status status; /* Thread state. */
   char name[16];             /* Name (for debugging purposes). */
   uint8_t *stack;            /* Saved stack pointer. */
   int priority;              /* Priority. */
   struct list_elem all_threads;

   /* Shared between thread.c and synch.c. */
   struct list_elem elem; /* List element. */

#ifdef USERPROG
   /* Owned by userprog/process.c. */
   uint32_t *pagedir; /* Page directory. */
#endif

   /* Owned by thread.c. */
   unsigned magic; /* Detects stack overflow. */
   struct list_elem sleeping_elements;
   struct list held_lock;
   struct lock *curr_lock;
   int our_priority;
   int nice;
   int64_t remaining_time;
   fixed_point recent_cpu;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);
void thread_foreach(thread_action_func *, void *);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

/**
 * @brief Checks if the current thread should yield the CPU.
 *
 * This function checks if there is a higher priority thread in the ready list
 * than the current thread. If such a thread exists, the current thread yields
 * the CPU to allow the higher priority thread to run.
 */
void check_thread_yield(void);

/**
 * @brief Updates the system load average and the recent CPU usage for each thread.
 *
 * This function is called once every second. It first calculates the number of threads
 * that are either running or ready to run (not including the idle thread). It then updates
 * the system load average based on this number. After that, it updates the recent CPU usage
 * for each thread in the system.
 *
 * The function disables interrupts before updating the load average and the recent CPU usage
 * to avoid race conditions, and then restores the original interrupt level.
 */
void tick_every_second(void);

/**
 * @brief Sets the current thread to sleep for a specified number of ticks.
 *
 * This function retrieves the current thread and sets its remaining time to the
 * specified number of ticks. It then adds the thread to the sleeping list and
 * blocks the thread, causing it to sleep until it's unblocked.
 *
 * @param ticks The number of ticks for which the current thread should sleep.
 */
void set_sleeping_thread(int64_t);

/**
 * @brief Updates the priority of a given thread.
 *
 * This function updates the priority of a given thread based on the locks it's holding.
 * If the thread is not holding any locks, its priority is set to its original priority.
 * If the thread is holding one or more locks, its priority is set to the maximum of its
 * original priority and the maximum priority of the locks it's holding.
 *
 * @param t The thread whose priority is to be updated.
 */
void update_thread(struct thread *);
/**
 * @brief Rearranges the ready list based on thread priority.
 *
 * This function removes the given thread from the ready list and then inserts it back
 * in the correct position based on its priority. This ensures that the ready list is
 * always sorted in order of thread priority.
 *
 * @param t The thread that needs to be rearranged in the ready list.
 */
void rearrange_ready_list(struct thread *);

/**
 * @brief Updates the recent CPU usage of a thread.
 *
 * This function calculates the recent CPU usage of a given thread based on the system's
 * load average and the thread's nice value. It then updates the thread's priority based
 * on its recent CPU usage and nice value.
 *
 * @param t The thread whose recent CPU usage is to be updated.
 * @param aux Auxiliary data. This parameter is unused in this function.
 */
void thread_update_recent_cpu(struct thread *, void *);

/**
 * @brief Updates the priority of a thread in a multi-level feedback queue scheduler.
 *
 * This function calculates the new priority of a given thread based on the maximum
 * priority (PRI_MAX), the thread's nice value, and its recent CPU usage. The new
 * priority is then bounded by the maximum and minimum priority values (PRI_MAX and
 * PRI_MIN). Finally, the thread's priority is updated to the new priority.
 *
 * @param t The thread whose priority is to be updated.
 */
void thread_update_priority_mlfqs(struct thread *);

/**
 * @brief Compares the priority of two thread elements.
 *
 * This function is used for ordering thread elements in a list based on their priority.
 * It takes two list elements as parameters, retrieves the thread elements from them,
 * and compares their priority.
 *
 * @param a The first list element to compare.
 * @param b The second list element to compare.
 * @param aux Auxiliary data. This parameter is unused in this function.
 * @return True if the priority of the first thread element is less than or equal to the priority of the second thread element, false otherwise.
 */
bool compare_threads(const struct list_elem *, const struct list_elem *, void *);

#endif /* threads/thread.h */
