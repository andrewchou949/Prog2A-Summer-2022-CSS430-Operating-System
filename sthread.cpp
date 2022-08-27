// Name: Andrew Chou
// Class: CSS 430
// ----------------------------------------------- sthread.cpp -------------------------------------------
// This sthread.cpp is utilizing the driver.cpp to initiate multiple threads (3 in this assignment)
// Then using the time quantum of 5 secs each time to do context switch between each threads initiated
// The function being implemented by myself is the sthread_yield, and capture function
//      sthread_yield is responsible on using the timer (alarmed bool) to check if 5 secs elapses. if
//          it is, save current env with setjmp, perform capture() and switch control to scheduler thread 
//          with longjmp(scheduler), and lastly perform memory retrieving from stack (temporary storage)
//          to stack pointer (current pointer)
//      capture is responsible for enqueuing tcb* object to the queue, manually allocate memory space for 
//          cur_tcb->stack with malloc, and lastly performing memory saving to the stack temporary space 
//          which was allocated previously.
// -------------------------------------------------------------------------------------------------------

#include <setjmp.h> // setjmp( )
#include <signal.h> // signal( )
#include <unistd.h> // sleep( ), alarm( )
#include <stdio.h>  // perror( )
#include <stdlib.h> // exit( )
#include <iostream> // cout
#include <string.h> // memcpy
#include <queue>    // queue

#define scheduler_init( ) {			\
    if ( setjmp( main_env ) == 0 )		\
      scheduler( );				\
  }

#define scheduler_start( ) {			\
    if ( setjmp( main_env ) == 0 )		\
      longjmp( scheduler_env, 1 );		\
  }

// to be implemented in this assignment
// capture current thread's jmp-env + activation record to cur_tcb
  // sp and bp are being declared with asm language code
  // stack space are using malloc to allocate space equal to cur_tcb->size
#define capture( ) {							\
    register void *sp asm("sp");    \
    register void *bp asm("bp");    \
    thr_queue.push(cur_tcb);  \
    cur_tcb->size = (int)((long long int)bp - (long long int)sp);  \
    cur_tcb->stack = (void*) malloc(cur_tcb->size);    \
    cur_tcb->sp = sp;   \
    memcpy(cur_tcb->stack, cur_tcb->sp, cur_tcb->size);  \
  }  

// to be implemented in this assignment   
// to be called by each user thread to "voluntarily" stop using CPU
  // Note, only after timer (5 sec) interrupt ==> calls capture() and goes back to scheduler
  // When control comes back from scheduler (with longjmp), 
    // retrieve this thread activation record from cur_tcb->stack.
  // Utilizing alarmed bool to test time elapses of 5 sec
#define sthread_yield( ) {						\
    if(alarmed == true){    \
        alarmed = false; \
        if(setjmp(cur_tcb->env) == 0){   \
          capture();    \
          longjmp(scheduler_env, 1);    \
        }   \
        memcpy(cur_tcb->sp, cur_tcb->stack, cur_tcb->size);   \
    }   \
  }

#define sthread_init( ) {					\
    if ( setjmp( cur_tcb->env ) == 0 ) {			\
      capture( );						\
      longjmp( main_env, 1 );					\
    }								\
    memcpy( cur_tcb->sp, cur_tcb->stack, cur_tcb->size );	\
  }

#define sthread_create( function, arguments ) { \
    if ( setjmp( main_env ) == 0 ) {		\
      func = &function;				\
      args = arguments;				\
      thread_created = true;			\
      cur_tcb = new TCB( );			\
      longjmp( scheduler_env, 1 );		\
    }						\
  }

#define sthread_exit( ) {			\
    if ( cur_tcb->stack != NULL )		\
      free( cur_tcb->stack );			\
    longjmp( scheduler_env, 1 );		\
  }

using namespace std;

static jmp_buf main_env;
static jmp_buf scheduler_env;

// Thread control block
class TCB {
public:
  TCB( ) : sp( NULL ), stack( NULL ), size( 0 ) { }
  jmp_buf env;  // the execution environment captured by set_jmp( )
  void* sp;     // the stack pointer (when retrieving thread, no need to use based pointer)
  void* stack;  // the temporary space to maintain the latest stack contents (heap)
  int size;     // the size of the stack contents
};
static TCB* cur_tcb;   // the TCB of the current thread in execution

// The queue of active threads
static queue<TCB*> thr_queue;

// Alarm caught to switch to the next thread
static bool alarmed = false;
static void sig_alarm( int signo ) {
  alarmed = true;
}

// A function to be executed by a thread
void (*func)( void * );
void *args = NULL;
static bool thread_created = false;

static void scheduler( ) {
  // invoke scheduler
  if ( setjmp( scheduler_env ) == 0 ) {
    cerr << "scheduler: initialized" << endl;
    if ( signal( SIGALRM, sig_alarm ) == SIG_ERR ) {
      perror( "signal function" );
      exit( -1 );
    }
    longjmp( main_env, 1 );
  }

  // check if it was called from sthread_create( )
  if ( thread_created == true ) {
    thread_created = false;
    ( *func )( args );
  }

  // restore the next thread's environment
  if ( ( cur_tcb = thr_queue.front( ) ) != NULL ) {
    thr_queue.pop( );

    // allocate a time quontum of 5 seconds
    alarm( 5 );

    // return to the next thread's execution
    longjmp( cur_tcb->env, 1 );
  }

  // no threads to schedule, simply return
  cerr << "scheduler: no more threads to schedule" << endl;
  longjmp( main_env, 2 );
}


