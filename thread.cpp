// file: "thread.cpp"

// Copyright (c) 2001 by Marc Feeley and Université de Montréal, All
// Rights Reserved.
//
// Revision History
// 23 Oct 01  initial version (Marc Feeley)

//-----------------------------------------------------------------------------

#include "thread.h"
#include "asm.h"
#include "pic.h"
#include "apic.h"
#include "pit.h"
#include "intr.h"
#include "time.h"
#include "rtlib.h"
#include "term.h"

//-----------------------------------------------------------------------------

// "mutex" class implementation.

mutex::mutex ()
{
  wait_queue_init (this);
  _locked = FALSE;
}

void mutex::lock ()
{
  disable_interrupts ();

  if (_locked)
    save_context (&scheduler::suspend_on_wait_queue, this);
  else
    _locked = TRUE;

  enable_interrupts ();
}

bool mutex::lock_or_timeout (time timeout)
{
  disable_interrupts ();

  thread* current = scheduler::current_thread;

  if (_locked)
    {
      if (!less_time (current_time_no_interlock (), timeout))
        {
          enable_interrupts ();
          return FALSE;
        }

      current->_timeout = timeout;
      current->_did_not_timeout = TRUE;

      wait_queue_remove (current);
      wait_queue_insert (current, this);
      save_context (&scheduler::suspend_on_sleep_queue, NULL);

      bool did_not_timeout = current->_did_not_timeout;

      enable_interrupts ();

      return did_not_timeout;
    }

  _locked = TRUE;

  enable_interrupts ();

  return TRUE;
}

void mutex::unlock ()
{
  disable_interrupts ();

  thread* t = wait_queue_head (CAST(wait_queue*,this));

  if (t == NULL)
    _locked = FALSE;
  else
    {
      sleep_queue_remove (t);
      sleep_queue_detach (t);
      scheduler::reschedule_thread (t);
    }

  enable_interrupts ();
}

//-----------------------------------------------------------------------------

// "condvar" class implementation.

condvar::condvar ()
{
  wait_queue_init (this);
}

void condvar::wait (mutex* m)
{
#ifdef SOLUTION

  disable_interrupts ();

  thread* t = wait_queue_head (CAST(wait_queue*,m));

  if (t == NULL)
    m->_locked = FALSE;
  else
    {
      sleep_queue_remove (t);
      sleep_queue_detach (t);
      scheduler::reschedule_thread (t);
    }

  save_context (&scheduler::suspend_on_wait_queue, this);

  if (m->_locked)
    save_context (&scheduler::suspend_on_wait_queue, m);
  else
    m->_locked = TRUE;

  enable_interrupts ();

#else

  // **** il faut corriger cette implantation de "wait" ****

  m->unlock ();
  m->lock ();

#endif
}

bool condvar::wait_or_timeout (mutex* m, time timeout)
{
#ifdef SOLUTION

  disable_interrupts ();

  thread* current = scheduler::current_thread;

  thread* t = wait_queue_head (CAST(wait_queue*,m));

  if (t == NULL)
    m->_locked = FALSE;
  else
    {
      sleep_queue_remove (t);
      sleep_queue_detach (t);
      scheduler::reschedule_thread (t);
    }

  if (!less_time (current_time_no_interlock (), timeout))
    {
      enable_interrupts ();
      return FALSE;
    }

  current->_timeout = timeout;
  current->_did_not_timeout = TRUE;

  wait_queue_remove (current);
  wait_queue_insert (current, this);
  save_context (&scheduler::suspend_on_sleep_queue, NULL);

  if (current->_did_not_timeout)
    {
      enable_interrupts ();
      return m->lock_or_timeout (timeout);
    }

  enable_interrupts ();

  return FALSE;

#else

  // **** il faut corriger cette implantation de "wait_or_timeout" ****

  // **** inspirez-vous de l'implantation de "lock_or_timeout" ****

  return FALSE;

#endif
}

void condvar::signal ()
{
  disable_interrupts ();

  thread* t = wait_queue_head (CAST(wait_queue*,this));

  if (t != NULL)
    {
      sleep_queue_remove (t);
      sleep_queue_detach (t);
      scheduler::reschedule_thread (t);
    }

  enable_interrupts ();
}

void condvar::broadcast ()
{
  disable_interrupts ();

  thread* t;

  while ((t = wait_queue_head (CAST(wait_queue*,this))) != NULL)
    {
      sleep_queue_remove (t);
      sleep_queue_detach (t);
      scheduler::reschedule_thread (t);
    }

  enable_interrupts ();
}

void condvar::mutexless_wait ()
{
  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point

  save_context (&scheduler::suspend_on_wait_queue, this);

  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point
}

void condvar::mutexless_signal ()
{
  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point

  thread* t = wait_queue_head (CAST(wait_queue*,this));

  if (t != NULL)
    {
      sleep_queue_remove (t);
      sleep_queue_detach (t);
      scheduler::reschedule_thread (t);
    }
}

//-----------------------------------------------------------------------------

// "thread" class implementation.

thread::thread ()
{
  static const int stack_size = 65536; // size of thread stacks in bytes

  mutex_queue_init (this);
  sleep_queue_detach (this);

  uint32* s = CAST(uint32*,kmalloc (stack_size));

  if (s == NULL)
    fatal_error ("out of memory");

  _stack = s;

  s += stack_size / sizeof (uint32);

  *--s = 0;              // the (dummy) return address of "run_thread"
  *--s = eflags_reg ();  // space for "EFLAGS"
  *--s = cs_reg ();      // space for "%cs"
  *--s = CAST(uint32,&scheduler::run_thread); // to call "run_thread"

  // Note: the 3 topmost words on the thread's stack are in the same
  // layout as expected by the "iret" instruction.  When an "iret"
  // instruction is executed to restore the thread's context, the
  // function "run_thread" will be called and this function will get a
  // dummy return address (it is important that the function
  // "run_thread" never returns).

  _sp = s;

  _quantum = frequency_to_time (10000); // quantum is 1/10000th of a second

  _terminated = FALSE;
}

thread::~thread ()
{
  kfree (_stack);
}

thread* thread::start ()
{
  disable_interrupts ();
  scheduler::reschedule_thread (this);
  enable_interrupts ();
  return this;
}

void thread::join ()
{
  _m.lock ();
  while (!_terminated)
    _joiners.wait (&_m);
  _m.unlock ();
}

void thread::yield ()
{
  disable_interrupts ();
  save_context (&scheduler::switch_to_next_thread, NULL);
  enable_interrupts ();
}

thread* thread::self ()
{
  return scheduler::current_thread;
}

//-----------------------------------------------------------------------------

// "primordial_thread" class.

class primordial_thread : public thread
  {
  public:

    primordial_thread (void_fn continuation);
    virtual void run ();

  private:

    void_fn _continuation;
  };

primordial_thread::primordial_thread (void_fn continuation)
{
  _continuation = continuation;
}

void primordial_thread::run ()
{
  _continuation ();
}

//-----------------------------------------------------------------------------

// "scheduler" class implementation.

void scheduler::setup (void_fn continuation)
{
  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point

  readyq = new wait_queue;
  wait_queue_init (readyq);

  sleepq = new sleep_queue;
  sleep_queue_init (sleepq);

  the_primordial_thread = new primordial_thread (continuation);
  current_thread = the_primordial_thread;

  wait_queue_insert (current_thread, readyq);

  setup_timer ();

  scheduler::resume_next_thread ();

  // ** NEVER REACHED ** (this function never returns)
}

void scheduler::reschedule_thread (thread* t)
{
  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point

  wait_queue_remove (t);
  wait_queue_insert (t, readyq);
}

void scheduler::run_thread ()
{
  current_thread->run ();
  current_thread->_terminated = TRUE;
  current_thread->_joiners.broadcast ();

  disable_interrupts ();
  wait_queue_remove (current_thread);
  resume_next_thread ();

  // ** NEVER REACHED ** (this function never returns)
}

void scheduler::resume_next_thread ()
{
  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point

  thread* current = wait_queue_head (readyq);

  if (current != NULL)
    {
      current_thread = current;
      time now = current_time_no_interlock ();
      current->_end_of_quantum = add_time (now, current->_quantum);
      set_timer (current->_end_of_quantum, now);
      restore_context (current->_sp);

      // ** NEVER REACHED **
    }

  fatal_error ("deadlock detected");

  // ** NEVER REACHED ** (this function never returns)
}

void scheduler::switch_to_next_thread
  (uint32 cs,     // The parameters "cs" and "eflags" are only on
   uint32 eflags, // the stack as a byproduct of using "iret".
   uint32* sp,
   void* dummy)
{
  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point

  thread* current = current_thread;

  current->_sp = sp;
  reschedule_thread (current);
  resume_next_thread ();

  // ** NEVER REACHED ** (this function never returns)
}

void scheduler::suspend_on_wait_queue
  (uint32 cs,     // The parameters "cs" and "eflags" are only on
   uint32 eflags, // the stack as a byproduct of using "iret".
   uint32* sp,
   void* q)
{
  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point

  thread* current = current_thread;

  current->_sp = sp;
  wait_queue_remove (current);
  wait_queue_insert (current, CAST(wait_queue*,q));
  resume_next_thread ();

  // ** NEVER REACHED ** (this function never returns)
}

void scheduler::suspend_on_sleep_queue
  (uint32 cs,     // The parameters "cs" and "eflags" are only on
   uint32 eflags, // the stack as a byproduct of using "iret".
   uint32* sp,
   void* dummy)
{
  ASSERT_INTERRUPTS_DISABLED (); // Interrupts should be disabled at this point

  thread* current = current_thread;

  current->_sp = sp;
  sleep_queue_insert (current, sleepq);
  resume_next_thread ();

  // ** NEVER REACHED ** (this function never returns)
}

void scheduler::setup_timer ()
{
  // When the timer elapses an interrupt is sent to the processor,
  // causing it to call the function "timer_elapsed".  This is how CPU
  // multiplexing is achieved.  Unfortunately, it takes quite a bit of
  // time to service an interrupt and this can be an important part of
  // the cost of a preemptive context switch on a fast machine.  On a
  // 400 MHz Pentium III based Compaq Presario 5830, each timer
  // interrupt takes about 900 to 1000 nanoseconds and a voluntary
  // context switch ("yield" with no timer reprogramming) takes about
  // 700 nanoseconds.

#ifdef USE_PIT_FOR_TIMER

#ifdef USE_PIT_1_BYTE_COUNT
#define PIT_COUNT_FORMAT PIT_CW_LSB
#else
#define PIT_COUNT_FORMAT PIT_CW_LSB_MSB
#endif

  outb (PIT_CW_CTR(0) | PIT_COUNT_FORMAT | PIT_CW_MODE(0),
        PIT_PORT_CW(PIT1_PORT_BASE));

  ENABLE_IRQ(0);

#endif

#ifdef USE_APIC_FOR_TIMER

  uint32 x;

  x = APIC_LVTT;
  x &= ~APIC_LVT_MASKED; // Unmask timer interrupt
  APIC_LVTT = x;

#endif
}

void scheduler::set_timer (time t, time now)
{
  // t must be >= now

  ASSERT_INTERRUPTS_DISABLED ();

  int64 count;

#ifdef USE_PIT_FOR_TIMER

  count = time_to_pit_counts (subtract_time (t, now))
          + 2; // 2 is added to avoid timer undershoot cascades when
               // PIT is running fast compared to RTC or TSC

  if (count > 0xffff)
    count = 0;
#ifdef USE_PIT_1_BYTE_COUNT
  else if (count > 0xff)
    count = 0xff;
#endif

  // The following "outb" instructions for sending the count to the
  // PIT are really slow and can be an important part of the cost of a
  // context switch on a fast machine.  On a 400 MHz Pentium III based
  // Compaq Presario 5830, each "outb" instruction takes about 900 to
  // 1000 nanoseconds and a voluntary context switch ("yield" with no
  // timer reprogramming) takes about 700 nanoseconds.

  outb (count, PIT_PORT_CTR(0,PIT1_PORT_BASE));      // send LSB

#ifndef USE_PIT_1_BYTE_COUNT
  outb (count >> 8, PIT_PORT_CTR(0,PIT1_PORT_BASE)); // send MSB
#endif

#endif

#ifdef USE_APIC_FOR_TIMER

  count = time_to_apic_timer_counts (subtract_time (t, now))
          + 100; // 100 is added to avoid timer undershoot cascades when
                 // APIC timer is running fast compared to RTC or TSC

  if (count > 0xffffffff)
    count = 0xffffffff;

  APIC_INITIAL_TIMER_COUNT = count;

#endif
}

void scheduler::timer_elapsed ()
{
  ASSERT_INTERRUPTS_DISABLED ();

  time now = current_time_no_interlock ();

  for (;;)
    {
      thread* t = sleep_queue_head (sleepq);

      if (t == NULL || less_time (now, t->_timeout))
        break;

      t->_did_not_timeout = FALSE;
      sleep_queue_remove (t);
      sleep_queue_detach (t);
      reschedule_thread (t);
    }

  thread* current = current_thread;

  if (less_time (now, current->_end_of_quantum))
    set_timer (current->_end_of_quantum, now);
  else
    save_context (&switch_to_next_thread, NULL);
}

#ifdef USE_PIT_FOR_TIMER

void irq0 ()
{
  ASSERT_INTERRUPTS_DISABLED ();

#ifdef SHOW_TIMER_INTERRUPTS
  cout << "\033[41m irq0 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(0);

  scheduler::timer_elapsed ();
}

#endif

#ifdef USE_APIC_FOR_TIMER

void APIC_timer_irq ()
{
  ASSERT_INTERRUPTS_DISABLED ();

#ifdef SHOW_TIMER_INTERRUPTS
  cout << "\033[41m APIC timer irq \033[0m";
#endif

  APIC_EOI = 0;

  scheduler::timer_elapsed ();
}

#endif

wait_queue* scheduler::readyq;
sleep_queue* scheduler::sleepq;
thread* scheduler::the_primordial_thread;
thread* scheduler::current_thread;

//-----------------------------------------------------------------------------

// Local Variables: //
// mode: C++ //
// End: //
