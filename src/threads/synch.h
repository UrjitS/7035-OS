#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore
{
  unsigned value;      /* Current value. */
  struct list waiters; /* List of waiting threads. */
};

void sema_init(struct semaphore *, unsigned value);
void sema_down(struct semaphore *);
bool sema_try_down(struct semaphore *);
void sema_up(struct semaphore *);
void sema_self_test(void);

/* Lock. */
struct lock
{
  struct thread *holder;      /* Thread holding lock (for debugging). */
  struct semaphore semaphore; /* Binary semaphore controlling access. */
  struct list_elem elem;      /* List of lock elem. */
  int max_p;
};

void lock_init(struct lock *);
void lock_acquire(struct lock *);
bool lock_try_acquire(struct lock *);
void lock_release(struct lock *);
bool lock_held_by_current_thread(const struct lock *);

/* Condition variable. */
struct condition
{
  struct list waiters; /* List of waiting threads. */
};

void cond_init(struct condition *);
void cond_wait(struct condition *, struct lock *);
void cond_signal(struct condition *, struct lock *);
void cond_broadcast(struct condition *, struct lock *);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile("" : : : "memory")

/**
 * @brief Retrieves the highest priority thread waiting on a semaphore.
 *
 * This function checks the list of threads waiting on a given semaphore and
 * returns the thread with the highest priority. The function assumes that
 * the list of waiting threads is not empty.
 *
 * @param sema The semaphore to check.
 * @return The highest priority thread waiting on the semaphore.
 */
struct thread *sema_get_max(struct semaphore *);

/**
 * @brief Updates the maximum priority of a lock.
 *
 * This function checks the list of threads waiting on the lock's semaphore and
 * updates the lock's maximum priority (`max_p`) to the highest priority of these threads.
 * If the list of waiting threads is empty, the maximum priority is set to 0.
 *
 * @param lock The lock whose maximum priority is to be updated.
 */
void lock_update(struct lock *);

/**
 * @brief Compares the maximum priority of two lock elements.
 *
 * This function is used for ordering lock elements in a list based on their maximum priority.
 * It takes two list elements as parameters, retrieves the lock elements from them,
 * and compares their maximum priority.
 *
 * @param first_elem The first list element to compare.
 * @param second_elem The second list element to compare.
 * @param aux Auxiliary data. This parameter is unused in this function.
 * @return True if the maximum priority of the first lock element is less than or equal to the maximum priority of the second lock element, false otherwise.
 */
bool compare_locks(const struct list_elem *, const struct list_elem *, void *);

/**
 * @brief Compares the priority of two semaphore elements.
 *
 * This function is used for ordering semaphore elements in a list based on their priority.
 * It takes two list elements as parameters, retrieves the semaphore elements from them,
 * and compares their priority.
 *
 * @param first_elem The first list element to compare.
 * @param second_elem The second list element to compare.
 * @param aux Auxiliary data. This parameter is unused in this function.
 * @return True if the priority of the first semaphore element is less than or equal to the priority of the second semaphore element, false otherwise.
 */
bool compare_sema_elem(const struct list_elem *, const struct list_elem *, void *);

#endif /* threads/synch.h */
