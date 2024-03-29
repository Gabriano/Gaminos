// file: "thread.h"

// Copyright (c) 2001 by Marc Feeley and Universit� de Montr�al, All
// Rights Reserved.
//
// Revision History
// 23 Oct 01  initial version (Marc Feeley)

#ifndef THREAD_H
#define THREAD_H

//-----------------------------------------------------------------------------

#include "general.h"
#include "intr.h"
#include "time.h"

//-----------------------------------------------------------------------------

#ifdef CHECK_ASSERTIONS

#define ASSERT_INTERRUPTS_DISABLED() \
do { \
     if (eflags_reg () & (1<<9)) \
       { \
         cout << __FILE__ << ":" << __LINE__ \
              << ", failed ASSERT_INTERRUPTS_DISABLED\n"; \
       } \
   } while (0)

#define ASSERT_INTERRUPTS_ENABLED() \
do { \
     if ((eflags_reg () & (1<<9)) == 0) \
       { \
         cout << __FILE__ << ":" << __LINE__ \
              << ", failed ASSERT_INTERRUPTS_ENABLED\n"; \
       } \
   } while (0)

#else

#define ASSERT_INTERRUPTS_DISABLED()
#define ASSERT_INTERRUPTS_ENABLED()

#endif

#define disable_interrupts() \
do { \
     ASSERT_INTERRUPTS_ENABLED (); \
     __asm__ __volatile__ ("cli" : : : "memory"); \
   } while (0)

#define enable_interrupts() \
do { \
     ASSERT_INTERRUPTS_DISABLED (); \
     __asm__ __volatile__ ("sti" : : : "memory"); \
   } while (0)

// Save and restore the CPU state.

#define save_context(receiver,data)                                           \
do {                                                                          \
     ASSERT_INTERRUPTS_DISABLED ();                                           \
     __asm__ __volatile__                                                     \
       ("pushal                                                            \n \
         pushl %1               # The fourth parameter of the receiver fn  \n \
         lea   -16(%%esp),%%eax # The third parameter of the receiver fn   \n \
         pushl %%eax                                                       \n \
         pushfl                 # Setup a stack frame with the same format \n \
         pushl %%cs             #  as expected by the ``iret'' instruction \n \
         call  %c0              #  so that ``iret'' can restore the context\n \
         addl  $8,%%esp         # Remove the third and fourth parameter    \n \
         popal"                                                               \
        :                                                                     \
        : "i" (receiver), "g" (data)                                          \
        : "memory");                                                          \
   } while (0)

#ifdef USE_IRET_FOR_RESTORE_CONTEXT

#define restore_context(sp)                                                   \
do {                                                                          \
     ASSERT_INTERRUPTS_DISABLED ();                                           \
     __asm__ __volatile__                                                     \
       ("movl  %0,%%esp  # Restore the stack pointer                       \n \
         iret            # Return from the ``call'' in ``save_context''"      \
        :                                                                     \
        : "g" (sp));                                                          \
   } while (0)

#endif

#ifdef USE_RET_FOR_RESTORE_CONTEXT

#define restore_context(sp)                                                   \
do {                                                                          \
     ASSERT_INTERRUPTS_DISABLED ();                                           \
     __asm__ __volatile__                                                     \
       ("movl  %0,%%esp  # Restore the stack pointer                       \n \
         popl  %%ebx     # Get return address                              \n \
         popl  %%eax     # Discard %%cs                                    \n \
         popfl                                                             \n \
         jmp   *%%ebx    # Return from the ``call'' in ``save_context''"      \
        :                                                                     \
        : "g" (sp));                                                          \
   } while (0)

#endif

//-----------------------------------------------------------------------------

// Available thread priorities.

typedef int priority;

#define low_priority    0
#define normal_priority 100
#define high_priority   200

//-----------------------------------------------------------------------------

// Select implementations.

#define USE_DOUBLY_LINKED_LIST_FOR_WAIT_QUEUE
#define USE_DOUBLY_LINKED_LIST_FOR_MUTEX_QUEUE
#define USE_DOUBLY_LINKED_LIST_FOR_SLEEP_QUEUE

//-----------------------------------------------------------------------------

// "wait_mutex_node" class declaration.

class wait_queue; // forward declaration
class mutex_queue; // forward declaration

class wait_mutex_node
  {
  public:

    // Wait queue part for maintaining the set of threads waiting on a
    // synchronization object or the run queue.  If this object is a
    // thread it is an element of the queue; if this object is a
    // synchronization object or the run queue it is the queue of
    // waiting threads.

#ifdef USE_DOUBLY_LINKED_LIST_FOR_WAIT_QUEUE
    wait_mutex_node* volatile _next_in_wait_queue;
    wait_mutex_node* volatile _prev_in_wait_queue;
#endif

    // Mutex queue part for maintaining the set of mutexes owned by a
    // thread.  If this object is a mutex it is a queue element; if
    // this object is a thread it is the queue of owned mutexes.

#ifdef USE_DOUBLY_LINKED_LIST_FOR_MUTEX_QUEUE
    wait_mutex_node* volatile _next_in_mutex_queue;
    wait_mutex_node* volatile _prev_in_mutex_queue;
#endif
  };

//-----------------------------------------------------------------------------

// "wait_queue" class declaration.

class wait_queue : public wait_mutex_node
  {
  public:

#ifdef USE_DOUBLY_LINKED_LIST_FOR_WAIT_QUEUE
#endif
  };

//-----------------------------------------------------------------------------

// "mutex_queue" class declaration.

class mutex_queue : public wait_mutex_node
  {
  public:

#ifdef USE_DOUBLY_LINKED_LIST_FOR_MUTEX_QUEUE
#endif
  };

//-----------------------------------------------------------------------------

// "wait_mutex_sleep_node" class declaration.

class sleep_queue; // forward declaration

class wait_mutex_sleep_node : public mutex_queue
  {
  public:

    // Sleep queue part for maintaining the set of threads waiting for
    // a timeout.  If this object is a thread it is an element of the
    // queue; if this object is the run queue it is the queue of
    // threads waiting for a timeout.

#ifdef USE_DOUBLY_LINKED_LIST_FOR_SLEEP_QUEUE
    wait_mutex_sleep_node* volatile _next_in_sleep_queue;
    wait_mutex_sleep_node* volatile _prev_in_sleep_queue;
#endif
  };

//-----------------------------------------------------------------------------

// "sleep_queue" class declaration.

class sleep_queue : public wait_mutex_sleep_node
  {
  public:

#ifdef USE_DOUBLY_LINKED_LIST_FOR_SLEEP_QUEUE
#endif
  };

//-----------------------------------------------------------------------------

// "mutex" class declaration.

class mutex : public wait_queue
  {
  public:

    mutex (); // constructs an unlocked mutex

    void lock (); // waits until mutex is unlocked, and then lock the mutex
    bool lock_or_timeout (time timeout); // returns FALSE if timeout reached
    void unlock (); // unlocks a locked mutex

    // The inherited "wait queue" part of wait_queue is used to
    // maintain the set of threads waiting on this mutex.

    // The inherited "mutex queue" part of wait_queue is unused.

  protected:

    volatile bool _locked; // boolean indicating if mutex is locked or unlocked

    friend class condvar;
  };

//-----------------------------------------------------------------------------

// "condvar" class declaration.

class condvar : public wait_queue
  {
  public:

    condvar (); // constructs a condition variable with no waiting threads

    void wait (mutex* m); // suspends current thread on the condition variable
    bool wait_or_timeout (mutex* m, time timeout); // returns FALSE on timeout

    void signal (); // resumes one of the waiting threads
    void broadcast (); // resumes all of the waiting threads

    void mutexless_wait (); // like "wait" but uses interrupt flag as mutex
    void mutexless_signal (); // like "signal" but assumes disabled interrupts

    // The inherited "wait queue" part of wait_queue is used to
    // maintain the set of threads waiting on this condvar.

    // The inherited "mutex queue" part of wait_queue is unused.
  };

//-----------------------------------------------------------------------------

// "thread" class declaration.

typedef void (*void_fn) ();

class thread : public wait_mutex_sleep_node
  {
  public:

    // constructs a thread that will call "run"
    thread ();

    virtual ~thread (); // thread destructor

    thread* start (); // starts the execution of a newly constructed thread

    void join (); // waits for the termination of the thread

    static void yield (); // immediately ends the thread's quantum
    static thread* self (); // returns a pointer to currently running thread

    // The inherited "wait queue" part of wait_mutex_sleep_node
    // is used to maintain this thread in the wait_queue of the mutex
    // or condvar on which it is waiting.

    // The inherited "mutex queue" part of wait_mutex_sleep_node
    // is unused.

    // The inherited "sleep queue" part of wait_mutex_sleep_node
    // is used to maintain this thread in the sleep queue.

#ifdef USE_DOUBLY_LINKED_LIST_FOR_WAIT_QUEUE
#endif

#ifdef USE_DOUBLY_LINKED_LIST_FOR_SLEEP_QUEUE
#endif

    time _timeout; // when to end sleeping
    bool _did_not_timeout; // to tell if synchronization operation timed out

  protected:

    virtual void run () = 0; // thread body

    uint32* _stack; // the thread's stack
    uint32* _sp;    // the thread's stack pointer

    time _quantum;        // duration of the quantum for this thread
    time _end_of_quantum; // moment in time when current quantum ends

    int _prio; // the thread's priority

    mutex _m; // mutex to access termination flag
    condvar _joiners; // threads waiting for this thread to terminate
    volatile bool _terminated; // the thread's termination flag

    friend class mutex;
    friend class condvar;
    friend class scheduler;
  };

//-----------------------------------------------------------------------------

// "scheduler" class declaration.

class scheduler
  {
  public:

    static void setup (void_fn continuation); // initializes the scheduler

  protected:

    static void reschedule_thread (thread* t); // makes thread "t" runnable
    static void run_thread (); // begins the thread's execution
    static void resume_next_thread (); // resumes the next runnable thread

    // transfers the current thread to the tail of the queue of
    // runnable threads and resumes the thread at the head of the
    // queue of runnable threads
    static void switch_to_next_thread (uint32 cs,
                                       uint32 eflags,
                                       uint32* sp,
                                       void* dummy);

    // transfers the current thread to the tail of the given wait
    // queue and resumes the thread at the head of the queue of
    // runnable threads
    static void suspend_on_wait_queue (uint32 cs,
                                       uint32 eflags,
                                       uint32* sp,
                                       void* q);

    // transfers the current thread to the sleep queue and resumes the
    // thread at the head of the queue of runnable threads
    static void suspend_on_sleep_queue (uint32 cs,
                                        uint32 eflags,
                                        uint32* sp,
                                        void* dummy);

    static void setup_timer ();     // initializes the interval timer
    static void set_timer (time t, time now); // sets the timer to time "t"
    static void timer_elapsed ();   // called when the interval timer expires

    static wait_queue* readyq;            // the ready queue
    static sleep_queue* sleepq;           // the sleep queue
    static thread* the_primordial_thread; // the primordial thread
    static thread* current_thread;        // the current thread

    friend class mutex;
    friend class condvar;
    friend class thread;
#ifdef USE_PIT_FOR_TIMER
    friend void irq0 ();
#endif
#ifdef USE_APIC_FOR_TIMER
    friend void APIC_timer_irq ();
#endif
  };

//-----------------------------------------------------------------------------

// "wait_mutex_node" class implementation.

#define NODETYPE wait_mutex_node
#define QUEUETYPE wait_queue
#define ELEMTYPE thread
#define NAMESPACE_PREFIX(name) wait_queue_##name
#define BEFORE(elem1,elem2) FALSE

#ifdef USE_DOUBLY_LINKED_LIST_FOR_WAIT_QUEUE
#define USE_DOUBLY_LINKED_LIST
#define NEXT(node) (node)->_next_in_wait_queue
#define NEXT_SET(node,next_node) NEXT (node) = (next_node)
#define PREV(node) (node)->_prev_in_wait_queue
#define PREV_SET(node,prev_node) PREV (node) = (prev_node)
#include "queue.h"
#undef USE_DOUBLY_LINKED_LIST
#undef NEXT
#undef NEXT_SET
#undef PREV
#undef PREV_SET
#endif

#undef NODETYPE
#undef QUEUETYPE
#undef ELEMTYPE
#undef NAMESPACE_PREFIX
#undef BEFORE

//-----------------------------------------------------------------------------

// "wait_mutex_node" class implementation.

#define NODETYPE wait_mutex_node
#define QUEUETYPE mutex_queue
#define ELEMTYPE mutex
#define NAMESPACE_PREFIX(name) mutex_queue_##name
#define BEFORE(elem1,elem2) FALSE

#ifdef USE_DOUBLY_LINKED_LIST_FOR_MUTEX_QUEUE
#define USE_DOUBLY_LINKED_LIST
#define NEXT(node) (node)->_next_in_mutex_queue
#define NEXT_SET(node,next_node) NEXT (node) = (next_node)
#define PREV(node) (node)->_prev_in_mutex_queue
#define PREV_SET(node,prev_node) PREV (node) = (prev_node)
#include "queue.h"
#undef USE_DOUBLY_LINKED_LIST
#undef NEXT
#undef NEXT_SET
#undef PREV
#undef PREV_SET
#endif

#undef NODETYPE
#undef QUEUETYPE
#undef ELEMTYPE
#undef NAMESPACE_PREFIX
#undef BEFORE

//-----------------------------------------------------------------------------

// "wait_mutex_sleep_node" class implementation.

#define NODETYPE wait_mutex_sleep_node
#define QUEUETYPE sleep_queue
#define ELEMTYPE thread
#define NAMESPACE_PREFIX(name) sleep_queue_##name
#define BEFORE(elem1,elem2) less_time ((elem1)->_timeout, (elem2)->_timeout)

#ifdef USE_DOUBLY_LINKED_LIST_FOR_SLEEP_QUEUE
#define USE_DOUBLY_LINKED_LIST
#define NEXT(node) (node)->_next_in_sleep_queue
#define NEXT_SET(node,next_node) NEXT (node) = (next_node)
#define PREV(node) (node)->_prev_in_sleep_queue
#define PREV_SET(node,prev_node) PREV (node) = (prev_node)
#include "queue.h"
#undef USE_DOUBLY_LINKED_LIST
#undef NEXT
#undef NEXT_SET
#undef PREV
#undef PREV_SET
#endif

#undef NODETYPE
#undef QUEUETYPE
#undef ELEMTYPE
#undef NAMESPACE_PREFIX
#undef BEFORE

//-----------------------------------------------------------------------------

#endif

// Local Variables: //
// mode: C++ //
// End: //
