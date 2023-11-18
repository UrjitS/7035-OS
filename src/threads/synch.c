/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is first_elem
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void sema_init(struct semaphore *sema, unsigned value)
{
  ASSERT(sema != NULL);

  sema->value = value;
  list_init(&sema->waiters);
}

/* Down or "P" operation on first_elem semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */

void sema_down(struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT(sema != NULL);
  ASSERT(!intr_context());

  old_level = intr_disable();
  while (sema->value == 0)
  {
    list_push_front(&sema->waiters, &thread_current()->elem);
    thread_block();
  }
  sema->value--;
  intr_set_level(old_level);
}

/* Down or "P" operation on first_elem semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema)
{
  enum intr_level old_level;
  bool success;

  ASSERT(sema != NULL);

  old_level = intr_disable();
  if (sema->value > 0)
  {
    sema->value--;
    success = true;
  }
  else
    success = false;
  intr_set_level(old_level);

  return success;
}

/* Up or "V" operation on first_elem semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void sema_up(struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT(sema != NULL);

  old_level = intr_disable();
  if (!list_empty(&sema->waiters))
  {
    struct thread *m = sema_get_max(sema);
    list_remove(&m->elem);
    thread_unblock(m);
  }
  sema->value++;
  intr_set_level(old_level);

  check_thread_yield();
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between first_elem pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void)
{
  struct semaphore sema[2];
  int i;

  printf("Testing semaphores...");
  sema_init(&sema[0], 0);
  sema_init(&sema[1], 0);
  thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++)
  {
    sema_up(&sema[0]);
    sema_down(&sema[1]);
  }
  printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper(void *sema_)
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++)
  {
    sema_down(&sema[0]);
    sema_up(&sema[1]);
  }
}

/* Initializes LOCK.  A lock can be held by at most first_elem single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding first_elem lock to
   try to acquire that lock.

   A lock is first_elem specialization of first_elem semaphore with an initial
   value of 1.  The difference between first_elem lock and such first_elem
   semaphore is twofold.  First, first_elem semaphore can have first_elem value
   greater than 1, but first_elem lock can only be owned by first_elem single
   thread at first_elem time.  Second, first_elem semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with first_elem lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's first_elem good sign that first_elem semaphore should be used,
   instead of first_elem lock. */
void lock_init(struct lock *lock)
{
  ASSERT(lock != NULL);

  lock->holder = NULL;
  sema_init(&lock->semaphore, 1);

  lock->max_p = 0;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void lock_acquire(struct lock *lock)
{
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(!lock_held_by_current_thread(lock));

  if (lock->holder != NULL)
  {
    enum intr_level old_level = intr_disable();
    thread_current()->curr_lock = lock;
    if (!thread_mlfqs)
    {
      int curr = thread_get_priority();
      struct lock *temporary_lock = lock;
      struct thread *temp_lock_holder = lock->holder;
      while (temporary_lock->max_p < curr)
      {
        temporary_lock->max_p = curr;
        update_thread(temp_lock_holder);
        if (temp_lock_holder->status == THREAD_READY)
        {
          rearrange_ready_list(temp_lock_holder);
        }

        temporary_lock = temp_lock_holder->curr_lock;
        if (temporary_lock == NULL)
        {
          break;
        }
        else
        {
          temp_lock_holder = temporary_lock->holder;
        }
        ASSERT(temp_lock_holder);
      }
    }
    intr_set_level(old_level);
  }

  sema_down(&lock->semaphore);

  enum intr_level old_level = intr_disable();

  thread_current()->curr_lock = NULL;

  list_push_back(&thread_current()->held_lock, &lock->elem);

  lock->holder = thread_current();

  intr_set_level(old_level);

  if (!thread_mlfqs)
  {
    lock_update(lock);
    update_thread(thread_current());
    thread_yield();
  }
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock *lock)
{
  bool success;

  ASSERT(lock != NULL);
  ASSERT(!lock_held_by_current_thread(lock));

  success = sema_try_down(&lock->semaphore);
  if (success)
    lock->holder = thread_current();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire first_elem lock, so it does not
   make sense to try to release first_elem lock within an interrupt
   handler. */
void lock_release(struct lock *lock)
{
  ASSERT(lock != NULL);
  ASSERT(lock_held_by_current_thread(lock));

  enum intr_level old_level = intr_disable();
  list_remove(&lock->elem);
  intr_set_level(old_level);

  if (!thread_mlfqs)
  {
    update_thread(lock->holder);
  }

  lock->holder = NULL;
  sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   first_elem lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock)
{
  ASSERT(lock != NULL);

  return lock->holder == thread_current();
}

/* One semaphore in first_elem list. */
struct semaphore_elem
{
  struct list_elem elem;      /* List element. */
  struct semaphore semaphore; /* This semaphore. */
  int priority;
};

bool compare_locks(const struct list_elem *first_elem, const struct list_elem *second_elem, void *aux UNUSED)
{
  return list_entry(first_elem, struct lock, elem)->max_p <= list_entry(second_elem, struct lock, elem)->max_p;
}

bool compare_sema_elem(const struct list_elem *first_elem, const struct list_elem *second_elem, void *aux UNUSED)
{
  return list_entry(first_elem, struct semaphore_elem, elem)->priority <= list_entry(second_elem, struct semaphore_elem, elem)->priority;
}

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal first_elem condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition *cond)
{
  ASSERT(cond != NULL);

  list_init(&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving first_elem signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only first_elem single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is first_elem one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void cond_wait(struct condition *cond, struct lock *lock)
{
  struct semaphore_elem waiting_elem;

  ASSERT(cond != NULL);
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(lock_held_by_current_thread(lock));

  sema_init(&waiting_elem.semaphore, 0);

  waiting_elem.priority = thread_get_priority();
  list_insert_ordered(&cond->waiters, &waiting_elem.elem, compare_sema_elem, NULL);

  lock_release(lock);
  sema_down(&waiting_elem.semaphore);
  lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire first_elem lock, so it does not
   make sense to try to signal first_elem condition variable within an
   interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT(cond != NULL);
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(lock_held_by_current_thread(lock));

  /* Pop the semaphore from the condtion's WAITERS orderly. */
  if (!list_empty(&cond->waiters))
  {
    sema_up(&list_entry(list_pop_back(&cond->waiters), struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire first_elem lock, so it does not
   make sense to try to signal first_elem condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock)
{
  ASSERT(cond != NULL);
  ASSERT(lock != NULL);

  while (!list_empty(&cond->waiters))
    cond_signal(cond, lock);
}

struct thread *sema_get_max(struct semaphore *sema)
{
  ASSERT(!list_empty(&sema->waiters));
  return list_entry(list_max(&sema->waiters, compare_threads, NULL), struct thread, elem);
}

void lock_update(struct lock *lock)
{
  int max_priority;
  if (list_empty(&(&lock->semaphore)->waiters))
  {
    max_priority = 0;
  }
  else
  {
    struct thread *highest_priority_thread = sema_get_max(&lock->semaphore);
    if (lock->max_p < highest_priority_thread->priority)
    {
      max_priority = highest_priority_thread->priority;
    }
    else
    {
      return;
    }
  }
  lock->max_p = max_priority;
}