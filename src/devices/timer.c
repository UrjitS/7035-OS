#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/fixed-point.h"
#include "threads/init.h"  

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/*
Contains inofrmation to keep track of sleeping threads 
*/
struct sleeping_thread
{
  struct thread * current_thread;
  int64_t ticks_till_release;
  struct list_elem next_thread;
};

//Semaphore to ensure only one thread is modifying the sleeping_threads list at a time
struct semaphore sleeping_threads_semaphore;

// Global list of sleeping threads
static struct list sleeping_threads = LIST_INITIALIZER(sleeping_threads);

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);
static bool order_sleeping_threads (const struct list_elem * first_elem, const struct list_elem * second_elm, void * aux UNUSED);
void update_load_avg (void);
void update_recent_cpu_for_all_threads(void);
void update_priority_for_all_threads(void);


int count_running_or_ready_threads1(void);
void check_thread_status1(struct thread *t, void *aux);
/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
  sema_init(&sleeping_threads_semaphore, 1); //initialize sleeping_threads_semaphore
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (loops_per_tick | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Comparison function for sleeping_thread elements in the list, used to keep the global sleeping_threads list ordered*/
static bool
order_sleeping_threads (const struct list_elem * first_elem, const struct list_elem * second_elm, void * aux UNUSED) 
{
  const struct sleeping_thread * thread_one = list_entry(first_elem, struct sleeping_thread, next_thread);
  const struct sleeping_thread * thread_two = list_entry(second_elm, struct sleeping_thread, next_thread);
  return thread_one->ticks_till_release < thread_two->ticks_till_release;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);

  // Add thread to sleeping threads list
  struct sleeping_thread temp_sleeping_thread;
  temp_sleeping_thread.current_thread = thread_current();
  temp_sleeping_thread.ticks_till_release = start + ticks;

  // Ensure interrupts are disabled while we are modifying the sleeping_threads list
  enum intr_level old_level = intr_disable();

  // Insert the sleeping thread into the sleeping_threads list in sorted order
  list_insert_ordered(&sleeping_threads, &temp_sleeping_thread.next_thread, order_sleeping_threads, NULL);

  // Block the thread
  thread_block();

  // Re-enable interrupts
  intr_set_level(old_level);
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. Checks if any sleeping threads are complete. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;
  thread_tick ();


  /* Increment recent_cpu for the running thread, unless the idle thread is running. */
  struct thread *current_thread = thread_current ();
  if (current_thread != get_idle_thread()) {
    current_thread->recent_cpu = add_int_to_fixed_point_number(current_thread->recent_cpu, 1);
  }

  /* Update load_avg once per second. */
  if (ticks % TIMER_FREQ == 0) {
    update_load_avg();
    update_recent_cpu_for_all_threads();
  }

 /* Recalculate priority once every fourth clock tick for every thread */
  if (ticks % 4 == 0) {
    update_priority_for_all_threads();
  }

  // Check the sleeping_threads list to unlbock any threads that are done sleeping
  struct list_elem * temp_elm;
  for (temp_elm = list_begin(&sleeping_threads); temp_elm != list_end(&sleeping_threads); temp_elm = list_next(temp_elm))
  {
    struct sleeping_thread * temp_sleeping_thread = list_entry(temp_elm, struct sleeping_thread, next_thread);
    if (temp_sleeping_thread->ticks_till_release <= ticks)
    {
      // Remove the thread from the sleeping_threads list
      list_remove(temp_elm);
      // Unblock the thread
      thread_unblock(temp_sleeping_thread->current_thread);
    } else {
      break;
    }
  }

}

void 
update_priority_for_all_threads(void) {
  struct list_elem * temp_elm;
  for (temp_elm = list_begin(get_all_list()); temp_elm != list_end(get_all_list()); temp_elm = list_next(temp_elm))
  {
    struct thread * temp_thread = list_entry(temp_elm, struct thread, allelem);
    if (temp_thread != get_idle_thread()) {
      /* Split the equation into separate parts */
      temp_thread->priority = calculate_priority();
    }
  }
}


/* Function to update load_avg. */
void
update_load_avg (void)
{
  LOAD_AVG = calculate_load_avg1();
}

/* Function to count the number of threads that are either running or ready */
int count_running_or_ready_threads1(void) {
  int count = 0;
  // Disable interrupts
  enum intr_level old_level;
  old_level = intr_disable();
  // Iterate through all threads and check if they are running or ready
  thread_foreach(check_thread_status1, &count);
  // Re-enable interrupts
  intr_set_level(old_level);
  
  return count;
}

/* Function to check the status of a thread and increment the count if the thread is running or ready */
void check_thread_status1(struct thread *t, void *aux) {
  int *count = (int *)aux;
  /* Check if the thread is not the idle thread */
  if (t != get_idle_thread() && (t->status == THREAD_RUNNING || t->status == THREAD_READY)) {
    (*count)++;
  }
}
int 
calculate_load_avg1(void) {
  /* Convert integers to fixed-point representation */
  int32_t fifty_nine_fp = convert_int_to_fixed_point(59);
  int32_t sixty_fp = convert_int_to_fixed_point(60);

  /* Calculate (59/60)*load_avg */
  int32_t part1 = divide_fixed_point_numbers(fifty_nine_fp, sixty_fp);
  int32_t part1_divided = multiply_fixed_point_numbers(part1, LOAD_AVG);

  /* Calculate (1/60)*ready_threads */
  int32_t part2 = divide_fixed_point_numbers(convert_int_to_fixed_point(1), sixty_fp);
  int32_t part2_mul = multiply_fixed_point_number_by_int(part2, count_running_or_ready_threads1());
  /* Add the two parts together */
  int32_t load_avg_new_fp = add_fixed_point_numbers(part1_divided, part2_mul);
  /* Convert result back to integer */
  int load_avg_new = load_avg_new_fp;
  return load_avg_new;
}


void 
update_recent_cpu_for_all_threads(void) {
  struct list_elem * temp_elm;
  for (temp_elm = list_begin(get_all_list()); temp_elm != list_end(get_all_list()); temp_elm = list_next(temp_elm))
  {
    struct thread * temp_thread = list_entry(temp_elm, struct thread, allelem);
    /* Split the equation into separate parts */
    int32_t load_avg_times_two = multiply_fixed_point_numbers(LOAD_AVG, convert_int_to_fixed_point(2));
    int32_t denominator = add_fixed_point_numbers(load_avg_times_two, convert_int_to_fixed_point(1));
    int32_t ratio = divide_fixed_point_numbers(load_avg_times_two, denominator);
    int32_t product = multiply_fixed_point_numbers(ratio, temp_thread->recent_cpu);
    int32_t addnums = add_int_to_fixed_point_number(product, temp_thread->nice_value);
    temp_thread->recent_cpu = addnums;
  }
}


/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
