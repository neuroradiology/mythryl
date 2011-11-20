// runtime-base.h


#ifndef RUNTIME_BASE_H
#define RUNTIME_BASE_H

// Macro concatenation (ANSI CPP)
//
#ifndef CONCAT /* assyntax.h also defines CONCAT */
    # define CONCAT(a,b)	a ## b
#endif
#define CONCAT3(a,b,c)	a ## b ## c

#define ONE_K_BINARY		1024
#define ONE_MEG_BINARY 	(ONE_K_BINARY*ONE_K_BINARY)

// The generated file
//
//     sizes-of-some-c-types--autogenerated.h
//
// defines various size-macros
// and the following types:
//
// Int16	-- 16-bit signed integer
// Int1	-- 32-bit signed integer
// Int2	-- 64-bit signed integer (64-bit machines only)
// Unt16	-- 16-bit unsigned integer
// Unt1	-- 32-bit unsigned integer
// Unt2	-- 64-bit unsigned integer (64-bit machines only)
// Unt8		-- Unsigned 8-bit integer.
// Val_Sized_Unt	-- Unsigned integer large enough for a Lib7 value.
// Val_Sized_Int	--   Signed integer large enough for a Lib7 value.
// Punt	-- Unsigned integer large enough for an address.
//
#include "sizes-of-some-c-types--autogenerated.h"


#define PAIR_BYTESIZE		(2*WORD_BYTESIZE)					// Size of a pair.
#define FLOAT64_SIZE_IN_WORDS		(FLOAT64_BYTESIZE / WORD_BYTESIZE)		// Val_Sized_Unt's per double.
#define PAIR_SIZE_IN_WORDS		2							// Val_Sized_Unt's per pair.
#define SPECIAL_CHUNK_SIZE_IN_WORDS	2							// Val_Sized_Unt's per special chunk.

// Convert a number of bytes
// to an even number of words:
//
#define BYTES_TO_WORDS(n)	(((n)+(WORD_BYTESIZE-1)) >> LOG2_BYTES_PER_WORD)

// Convert from double to word units:
//
#define DOUBLES_TO_WORDS(n)	((n) * FLOAT64_SIZE_IN_WORDS)

// On 32-bit machines it is useful to
// align doubles on 8-byte boundaries:
//
#ifndef SIZES_C_64_MYTHRYL_64
#  define ALIGN_FLOAT64S
#endif


#ifndef _ASM_

#include "../mythryl-config.h"

#include <stdlib.h>
#include <stdarg.h>

typedef  Int1  Bool;

#ifndef TRUE		// Some systems already define TRUE and FALSE.
    #define TRUE  1
    #define FALSE 0
#endif

typedef Int1 Status;

#define SUCCESS 1
#define FAILURE 0

// Assertions for debugging:
//
#ifdef ASSERT_ON
    extern void assert_fail (const char *a, const char *file, int line);
//  #define ASSERT(A)	((A) ? ((void)0) : assert_fail(#A, __FILE__, __LINE__))
    #define ASSERT(A)	{ if (!(A)) assert_fail(#A, __FILE__, __LINE__); }
#else
    #define ASSERT(A)	{ }
#endif

// Convert a bigendian 32-bit quantity
// into the host machine's representation:
//
#if defined(BYTE_ORDER_BIG)
    //
    #define BIGENDIAN_TO_HOST(x)	(x)
    //
#elif defined(BYTE_ORDER_LITTLE)
    //
    extern Unt1 swap_word_bytes (Unt1 x);
    #define BIGENDIAN_TO_HOST(x)	swap_word_bytes(x)
    //
#else
    #error must define endian
#endif

// Round i up to the nearest multiple of n,
// where n is a power of 2
//
#define ROUND_UP_TO_POWER_OF_TWO(i, n)		(((i)+((n)-1)) & ~((n)-1))


// Extract the bitfield of width WIDTH
// starting at position POS from I:
//
#define XBITFIELD(I,POS,WIDTH)		(((I) >> (POS)) & ((1<<(WIDTH))-1))

// Aliases for malloc/free, so 
// that we can easily replace them:
//
#define MALLOC(size)	malloc(size)
#define _FREE		free
#define FREE(p)		_FREE(p)

#define MALLOC_CHUNK(t)	((t*)MALLOC(sizeof(t)))		// Allocate a new C chunk of type t.
#define MALLOC_VEC(t,n)	((t*)MALLOC((n)*sizeof(t)))	// Allocate a new C array of type t chunks.

#define CLEAR_MEMORY(m, size)	(memset((m), 0, (size)))

// C types used in the run-time system:
//
#ifdef SIZES_C_64_MYTHRYL_32
    //
    typedef Unt1  Val;
#else
    //
    typedef   struct { Val_Sized_Unt v[1]; }   Valchunk;	// Just something for a Val to point to.
    //
    typedef   Valchunk*   Val;					// Only place Valchunk type is used.
#endif
//
typedef struct pthread_state_struct	Pthread;		// struct pthread_state_struct	def in   src/c/h/pthread-state.h
typedef struct task			Task;			// struct task			def in   src/c/h/task.h 
typedef struct heap			Heap;			// struct heap			def in   src/c/h/heap.h

#include <pthread.h>						// Posix threads:			https://computing.llnl.gov/tutorials/pthreads/

typedef pthread_mutex_t			Mutex;			// A mutual-exclusion lock:		https://computing.llnl.gov/tutorials/pthreads/#Mutexes
typedef pthread_barrier_t		Barrier;		// A barrier.
typedef pthread_cond_t			Condvar;		// Condition variable:			https://computing.llnl.gov/tutorials/pthreads/#ConditionVariables
typedef pthread_t 			Pid;			// A process id.
    //
    // NB; Pid MUST be pthread_t from <pthread.h> because in
    // pth__pthread_create from src/c/pthread/pthread-on-posix-threads.c
    // we pass a pointer to task->pthread->pid as pthread_t*.

// System_Constant
//
// In C, system constants are usually integers.
// We represent these in the Mythryl system as
// (Int, String) pairs, where the integer is the
// C constant and the string is a short version
// of the symbolic name used in C (e.g., the constant
// EINTR might be represented as (4, "INTR")).
//
typedef struct {
    //
    int	   id;
    char*  name;
    //
} System_Constant;

// System_Constants_Table
//
typedef struct {
    //
    int		       constants_count;
    System_Constant*   consts;
    //
} System_Constants_Table;


// Run-time system messages:
//
extern void say       (char* fmt, ...);											// say				def in    src/c/main/error-reporting.c
extern void debug_say (char* fmt, ...);											// debug_say			def in    src/c/main/error-reporting.c
extern void say_error (char*,     ...);											// say_error			def in    src/c/main/error-reporting.c
extern void die       (char*,     ...);											// die				def in    src/c/main/error-reporting.c

extern void print_stats_and_exit      (int code);									// print_stats_and_exit		def in    src/c/main/runtime-main.c

typedef   struct cleaner_args   Heapcleaner_Args;
    //
    // An abstract type whose representation depends
    // on the particular cleaner being used.

extern Heapcleaner_Args*   handle_cleaner_commandline_arguments   (char** argv);					// handle_cleaner_commandline_arguments		def in   src/c/heapcleaner/heapcleaner-initialization.c

extern void  load_compiled_files  (const char* compiled_files_to_load_filename, Heapcleaner_Args* params);		// load_compiled_files				def in   src/c/main/load-compiledfiles.c/load_compiled_files()
extern void  load_and_run_heap_image (const char* heap_image_to_run_filename,         Heapcleaner_Args* params);	// load_and_run_heap_image			def in   src/c/main/load-and-run-heap-image.c

extern Task* make_task               (Bool is_boot, Heapcleaner_Args* params);						// make_task					def in   src/c/main/runtime-state.c
extern void initialize_task (Task *task);										// initialize_task				def in   src/c/main/runtime-state.c
extern void save_c_state    (Task *task, ...);										// save_c_state					def in   src/c/main/runtime-state.c
extern void restore_c_state (Task *task, ...);										// restore_c_state				def in   src/c/main/runtime-state.c


extern void set_up_timers ();

extern Val    run_mythryl_function (Task *task, Val f, Val arg, Bool use_fate);						// run_mythryl_function				def in   src/c/main/run-mythryl-code-and-runtime-eventloop.c

extern void   reset_timers (Pthread* pthread);
extern void   run_mythryl_task_and_runtime_eventloop (Task* task);							// run_mythryl_task_and_runtime_eventloop	def in   src/c/main/run-mythryl-code-and-runtime-eventloop.c
extern void   raise_mythryl_exception (Task* task, Val exn);								// raise_mythryl_exception			def in   src/c/main/run-mythryl-code-and-runtime-eventloop.c
extern void   handle_uncaught_exception   (Val e);									// handle_uncaught_exception			def in   src/c/main/runtime-exception-stuff.c

extern void   set_up_fault_handlers ();											// set_up_fault_handlers			def in   src/c/machine-dependent/posix-arithmetic-trap-handlers.c
															// set_up_fault_handlers			def in   src/c/machine-dependent/cygwin-fault.c
															// set_up_fault_handlers			def in   src/c/machine-dependent/win32-fault.c
#if NEED_SOFTWARE_GENERATED_PERIODIC_EVENTS
    //
    extern void reset_heap_allocation_limit_for_software_generated_periodic_events (Task *task);
#endif


///////////////////////////////////////////////////////////////////////////
// Support for CEASE_USING_MYTHRYL_HEAP.
//
// The problem to be solved by CEASE_USING_MYTHRYL_HEAP
// is that while we are doing a slow syscall (or just a
// slow C op, like compressing a largish string) we cannot
// respond to a request to enter heapcleaner mode,
// and consequently all other pthreads coult wind up blocked
// waiting for us to join them in heapcleaner mode -- thus
// defeating much of the point of having multiple kernel threads
// running. (Minor heapcleanings happen about 200 times per second.)
//
// Our basic solution is that before doing such an op we
// reliquish heap access rights by changing our pthread
// status from PTHREAD_IS_RUNNING_MYTHRYL to PTHREAD_IS_RUNNING_C;
// the other pthreads then know we're out of the loop and can go
// ahead and do a heapcleaning without us.
//
// Our solution creates the problem that any Mythryl heap values
// used by the slow system call or C function must therefor be
// copied our of the Mythryl heap, since heapcleaning may move
// them around arbitrarily without warning so long as we have
// PTHREAD_IS_RUNNING_C set.
//
// So here we implement functionality to copy values out of the
// Mythryl heap.  Obviously, we cannot use static buffers, since
// they would be shared between all pthreads;  we have to use
// either stack storage or malloc()ed storage.  Malloc()ing is
// slow and stack storage is fixed-size (unless we use nonstandard
// gcc features) so we use stack storage for small stuff and
// malloc()ed space for large stuff:
//
#define MAX_STACK_BUFFERED_MYTHRYL_HEAP_VALUE (4*1024)		// Any number large enough so copying it takes longer than malloc()ing it.
//
typedef  struct  {
    //
    void* heap_space;						// NULL if stack_space was big enough, otherwise a malloc()d buffer that needs to be free()d later.
    //
    char stack_space[ MAX_STACK_BUFFERED_MYTHRYL_HEAP_VALUE ];	// Small stuff gets buffered in here.
    //
} Mythryl_Heap_Value_Buffer;
//
void*   buffer_mythryl_heap_value	( Mythryl_Heap_Value_Buffer*, void* heapval, int heapval_bytesize );		//   buffer_mythryl_heap_value			def in   src/c/main/runtime-state.c
void  unbuffer_mythryl_heap_value	( Mythryl_Heap_Value_Buffer* );							// unbuffer_mythryl_heap_value			def in   src/c/main/runtime-state.c





/////////////////////////////////////////////////////////////////////////
// These are two views of the command line arguments.
// raw_args is essentially argv[].
// commandline_arguments is argv[] with runtime system arguments stripped
// out (e.g., those of the form --runtime-xxx[=yyy]).
// commandline_arguments is argv[] with runtime system arguments stripped
// out (e.g., those of the form --runtime-xxx[=yyy]).
//
extern char** raw_args;
extern char** commandline_arguments;				// Does not include the command name (argv[0]).
extern char*  mythryl_program_name__global;			// Command name used to invoke the runtime.
extern int    verbosity;
extern Bool   codechunk_comment_display_is_enabled__global;	// Set per   --show-code-chunk-comments	  commandline switch in   src/c/main/runtime-main.c
extern Bool   cleaner_messages_are_enabled__global;		// Set                                                       in   src/c/lib/heap/heapcleaner-control.c
extern Bool   unlimited_heap_is_enabled__global;		// Set per   --unlimited-heap             commandline switch in   src/c/heapcleaner/heapcleaner-initialization.c

extern Pthread*	pthread_table__global [];			// pthread_table__global	def in   src/c/main/runtime-state.c
    //
    // Table of all active posix threads in process.
    // (Or at least, all posix threads running Mythryl
    // code or accessing the Mythryl heap.)
    //
    // In multithreaded operation this table is modified
    // only by code in   src/c/pthread/pthread-on-posix-threads.c
    // serialized by the pthread_table_mutex__local
    // in that file.     

extern int   pth__done_pthread_create__global;
    //
    // This boolean flag starts out FALSE and is set TRUE
    // the first time   pth__pthread_create   is called.
    //
    // We can use simple mutex-free monothread logic in
    // the heapcleaner (etc) so long as this is FALSE.


// log_if declaration.
//
// Conditional tracing to a logfile
// designed to work in concert with
//
//     src/lib/src/lib/thread-kit/src/lib/logger.pkg
//
// At the Mythryl level one calls
//
//     internet_socket::set_printif_fd
//
// from
//
//     src/lib/std/src/socket/internet-socket.pkg
//
// to enable this tracing by setting
//
//     log_if_fd
//
// after which desired C modules can call log_if
// to write lines into the tracelog file.


extern void   log_if   (const char * fmt, ...);
extern int    log_if_fd;



// Some convenience macros -- is there a better place for them?

#ifndef MIN
#define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif


#define CEASE_USING_MYTHRYL_HEAP( pthread, fn_name, arg )   { if (0) printf("%s: Cease using Mythryl heap.\n",fn_name); }
#define BEGIN_USING_MYTHRYL_HEAP( pthread, fn_name      )   { if (0) printf("%s: Begin using Mythryl heap.\n",fn_name); }
    //
    // NB: The production definitions of the above need to
    // increment/decrement ACTIVE_PTHREADS_COUNT_REFCELL__GLOBAL.
    //
    // The problem to be solved here is that when
    // multiple pthreads (kernel threads) share the
    // Mythryl heap, all threads must enter heapcleaning
    // mode before heapcleaning can begin, which happens
    // about 200 times per second:  If one pthread is
    // blocked in a sleep() or select() or whatever for
    // a long time (on the millisecond scale), all other
    // pthreads will wind up dead in the water until the
    // offending pthread finally wakes up.
    //
    // Our solution is that any pthread starting a potentially
    // lengthy C operation (which does not involve the Mythryl heap!)
    // should do
    //
    //     CEASE_USING_MYTHRYL_HEAP( task->pthread, "foo", arg );
    //         //
    //         slow_c_operation_not_using_mythryl_heap();
    //         //
    //     BEGIN_USING_MYTHRYL_HEAP( task->pthread, "foo" );
    //
    //  (These are expected to be used in one of the 
    //  Mythryl/C interface fns taking (Task* task, Val arg)
    //  as arguments. Think very carefully before using them
    //  elsewhere -- there may be Val args in the caller which
    //  are unprotected from the heapcleaner!)
    //
    // Those macros can then explicitly remove the pthread from
    // the 'active' set (by changing pthread->status from
    // PTHREAD_IS_RUNNING_MYTHRYL to PTHREAD_IS_RUNNING_C) before
    // the slow operation and then changing pthread->status back to
    // PTHREAD_IS_RUNNING_MYTHRYL afterward, with of course proper
    // mutex protection on the latter to assure that the pthread
    // does not attempt to resume using the heap during heapcleaning.
    //
    // NB: Because the heapcleaner (garbage collector)
    //     may move things around, you must:
    //
    //     o  Use the third arg to CEASE_USING_MYTHRYL_HEAP
    //        to protect the main Val arg to the fn.  (I assume
    //        here that CEASE/BEGIN are being used in one of the
    //
    //     o  You must not reference the Mythryl heap in any
    //        way between CEASE_USING_MYTHRYL_HEAP and
    //        BEGIN_USING_MYTHRYL_HEAP.
    //
    //     o  You must treat all Mythryl-heap references -except-
    //        'arg' as garbage after BEGIN_USING_MYTHRYL_HEAP,
    //        re-fetching them as necessary. 



// The following stuff used to be in runtime-pthread.h but I merged
// it into this file as part of making pthread support part of thed
// base functionality of Mythryl -- 2011-11-18 CrT

typedef enum {
    //
    PTHREAD_IS_RUNNING_MYTHRYL,		// Normal state of a running Mythryl pthread.
    PTHREAD_IS_RUNNING_C,		// For when a pthread is I/O blocked at the C level on a sleep(), select(), read() or such.  MUST NOT TOUCH MYTHRYL HEAP IN ANY WAY WHEN IN THIS STATE because heapcleaner may be running!
    PTHREAD_IS_VOID			// No kernel thread allocated -- unused slot in pthread table.
    //
} Pthread_Status;
    //
    // Status of a Pthread: value of pthread->status.	// pthread_state_struct is defined in   src/c/h/pthread-state.h
    //
    // To switch a pthread between the two
    // RUNNING modes, use the
    //
    //     CEASE_USING_MYTHRYL_HEAP		// PTHREAD_IS_RUNNING_MYTHRYL  ->  PTHREAD_IS_RUNNING_C        state transition.
    //     BEGIN_USING_MYTHRYL_HEAP		// PTHREAD_IS_RUNNING_C        ->  PTHREAD_IS_RUNNING_MYTHRYL  state transition.
    //
    // macros.



    #if !NEED_SOFTWARE_GENERATED_PERIODIC_EVENTS \
     || !NEED_PTHREAD_SUPPORT_FOR_SOFTWARE_GENERATED_PERIODIC_EVENTS
	//
	#error Multicore runtime currently requires polling support.
    #endif

    #if HAVE_SYS_TYPES_H
	#include <sys/types.h>
    #endif

    #include <sys/prctl.h>

    #if HAVE_UNISTD_H
	#include <unistd.h>
    #endif





    ////////////////////////////////////////////////////////////////////////////
    // Statically pre-allocated mutexs, barriers and condition variables:
    //
    extern Mutex	    pth__heapcleaner_mutex__global;
    extern Mutex	    pth__heapcleaner_gen_mutex__global;
    extern Mutex	    pth__timer_mutex__global;
    //
    extern Barrier	    pth__heapcleaner_barrier__global;
    //
    extern Condvar	    pth__unused_condvar__global;
    //


    ////////////////////////////////////////////////////////////////////////////
    // PACKAGE STARTUP AND SHUTDOWN
    //
    extern void     pth__start_up		(void);					// Called once near the top of main() to initialize the package.  Allocates our static locks, may also mmap() memory for arena or whatever.
    extern void     pth__shut_down		(void);					// Called once just before calling exit(), to release any OS resources.



    ////////////////////////////////////////////////////////////////////////////
    // PTHREAD START/STOP/ETC SUPPORT
    //
    extern char*    pth__pthread_create		( int* pthread_table_slot,
						  Val thread,
						  Val closure
						);					// Called with (thread, closure) and if a pthread is available starts closure running on a new pthread and returns TRUE.
    //											// Returns FALSE if we're already maxed out on allowed number of pthreads.
    //											// This gets exported to the Mythryl level as  "pthread", "make_pthread"  via   src/c/lib/pthread/libmythryl-pthread.c
    //											// and instantiated   at the Mythryl leval as  "make_pthread"             in    src/lib/std/src/pthread.pkg
    //
    extern void     pth__pthread_exit		(Task* task);				// Reverse of above, more or less.
    //											// On Solaris this appears to actually stop and kill the thread.
    //											// On SGI this appears to just suspend the thread pending another request to run something on it.
    //											// Presumably the difference is that thread de/allocation is cheaper on Solaris than on SGI...?
    // 
    //
    extern char*    pth__pthread_join		(Task* task, int pthread_table_slot);	// Wait until subthread exits.
    // 
    extern Pthread* pth__get_pthread		(void);					// Needed to find record for current pthread in contexts like signal handlers where it is not (otherwise) available.
    //											// Pthread is typedef'ed in src/c/h/runtime-base.h
    //
    extern Pid      pth__get_pthread_id		(void);					// Used to initialize pthread_table__global[0]->pid in   src/c/main/runtime-state.c
    //											// This just calls getpid()  in                         src/c/pthread/pthread-on-sgi.c
    //											// This returns thr_self() (I don't wanna know) in      src/c/pthread/pthread-on-solaris.c
    //
    extern int      pth__get_active_pthread_count();					// Just returns (as a C int) the value of   ACTIVE_PTHREADS_COUNT_REFCELL__GLOBAL, which is defined in   src/c/h/runtime-globals.h
											// Used only to set barrier for right number of pthreads in   src/c/heapcleaner/pthread-heapcleaner-stuff.c


    ////////////////////////////////////////////////////////////////////////////
    // PTHREAD GARBAGE COLLECTION SUPPORT
    //
    extern void   partition_agegroup0_buffer_between_pthreads   (Pthread *pthread_table[]);
    extern int   pth__start_heapcleaning    (Task*);
    extern void  pth__finish_heapcleaning   (Task*);
    extern int   pth__call_heapcleaner_with_extra_roots   (Task *task, va_list ap);
    //
    extern Val*  pth__extra_heapcleaner_roots__global [];



    ////////////////////////////////////////////////////////////////////////////
    //                   MUTEX LOCKS
    //
    // We use our "locks" to perform mutual exclusion,
    // ensuring consistency of shared mutable datastructures
    // by ensuring that at most one pthread at a time is
    // updating that datastructure.  Typically we allocate
    // one such lock for each major shared mutable datastructure,
    // which persists for as long as that datastructure.
    //
    // Tutorial:   https://computing.llnl.gov/tutorials/pthreads/#Mutexes
    //
    extern char* pth__mutex_init	(Mutex* mutex);				// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_mutex_init.html
    extern char* pth__mutex_destroy	(Mutex* mutex);				// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_mutex_init.html
    //
    extern char* pth__mutex_lock	(Mutex* mutex);				// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_mutex_lock.html
    extern char* pth__mutex_unlock	(Mutex* mutex);				// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_mutex_lock.html
    extern char* pth__mutex_trylock	(Mutex* mutex, Bool* result);		// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_mutex_lock.html
    //										// pth__mutex_trylock returns FALSE if lock was acquired, TRUE if it was busy.
    //										// This bool value is confusing -- the Mythryl-level binding should return (say) ACQUIRED vs BUSY.
    // The idea here is to not waste time on
    // mutex ops so long as we know there is
    // only one Mythryl pthread running:
    // 
    #define PTH__MUTEX_LOCK(mutex)    { if (pth__done_pthread_create__global) pth__mutex_lock(  mutex); }
    #define PTH__MUTEX_UNLOCK(mutex)  { if (pth__done_pthread_create__global) pth__mutex_unlock(mutex); }

    ////////////////////////////////////////////////////////////////////////////
    //                   CONDITIONAL VARIABLES
    //
    // Condition variables (in conjunction with mutexes)
    // provide a way for a pthread to wait for (typically)
    // a particular variable to assume a particular value,
    // without having to poll.
    //
    // Tutorial:   https://computing.llnl.gov/tutorials/pthreads/#ConditionVariables
    //
    extern char*   pth__condvar_init		(Condvar* condvar);			// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_cond_init.html
	//
	// Prepare the condition variable for use.
	// This may allocate resources or such internally.
	// Caveats:
	//
	//  o Behavior is undefined if pth__condvar_init()
	//   is called on an already-initialized condition variable.
	//   (Call pth__condvar_destroy first.)

    extern char*   pth__condvar_destroy		(Condvar* condvar);			// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_cond_init.html
        //
        // Undo the effects of   pth__condvar_init ()   on the condition variable.
	// ("Destroy" is poor nomenclature; "reset" would be better.)
        //
        //  o After calling pth__condvar_destroy on a condition variable
	//    one may call  pth__condvar_init on it; all other operations are undefined.
	//
	//  o Behavior is undefined if pth__condvar_destroy()
	//    is called when a pthread is blocked on the condition variable.

    extern char*   pth__condvar_wait   (Condvar* condvar, Mutex* mutex);		// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_cond_wait.html
	//
	// Atomically release mutex and block on the condition variable.
	// Upon return we will again hold the mutex.  (Return is triggered
	// by a call to   pth__condvar_signal or pth__condvar_broadcast.)

    extern char*   pth__condvar_signal   (Condvar* condvar);				// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_cond_signal.html
	//
	// Unblock at least one pthread waiting on condvar,
	// except no effect if no pthreads are blocked on condvar,
	//
	// If more than one pthread is blocked on condvar the scheduling
	// policy determines the order in which threads are unblocked.
	//
	// If multiple pthreads are unblocked, they compete for the
	// associated mutex as though they had call called pth__mutex_lock().

    extern char*   pth__condvar_broadcast   (Condvar* condvar);				// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_cond_signal.html
	//
	// Unblock all pthreads waiting on condvar, which might be none.
	//
	// If multiple pthreads are unblocked, they compete for the
	// associated mutex as though they had all called pth__mutex_lock().



    ////////////////////////////////////////////////////////////////////////////
    //                   BARRIERS
    //
    // We use our "barriers" to perform essentially the
    // opposite of mutual exclusion, ensuring that all
    // pthreads in a set have completed their part of
    // a shared task before any of them are allowed to
    // proceed past the "barrier".
    //
    // Our only current use of this facility is in
    //
    //     src/c/heapcleaner/pthread-heapcleaner-stuff.c
    //
    // where it serves to ensure that garbage collection
    // does not start until all pthreads have ceased normal
    // processing, and that no pthread resumes normal processing
    // until the garbage collection is complete.
    //
    // The basic use protocol is:
    //
    //  o Call pth__barrier_init() before doing anything else.
    //
    //  o Call pth__barrier_wait() to synchronize multiple pthreads.
    //
    //  o Call pth__barrier_detroy() before calling pth__barrier_init() again.
    //
    //  o Never call  pth__barrier_init() or pth__barrier_detroy()
    //    while pthreads are blocked on the barrier.
    //
    extern char*    pth__barrier_init 	(Barrier* barrier, int threads);	// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_barrier_init.html
	//
	// Tell the barrier how many threads must be
	// present at it before they can pass. This
	// may allocate resources or such internally.
	// Caveats:
	//
	//  o Behavior is undefined if pth__barrier_init()
	//   is called on an already-initialized barrier.
	//   (Call pth__barrier_destroy first.)
	//
	//  o Behavior is undefined if pth__barrier_init()
	//    is called when a pthread is blocked on the barrier.
	//    (That is, if some pthread has not returned from
	//    pth__barrier_wait)

    extern char*    pth__barrier_wait (Barrier* barrierp, Bool* result);	// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_barrier_wait.html
	//
	// Block currently executing pthread until the proper
	// number of pthreads are waiting at the barrier.
	// This number is specified via pth__barrier_init().
	//
	// When released, one pthread at barrier gets a TRUE
	// back pth__barrier_wait(), the others  get a FALSE;
	// this lets them easily "elect a leader" if desired.
	// (This is particularly useful for ensuring that
	// pth__barrier_destroy() gets called exactly once
	// after use of a barrier.)
	//
	//  o Behavior is undefined if calling pth__barrier_wait
	//    wait on an uninitialized barrier.
	//    A barrier is "uninitialized" if
	//      * pth__barrier_init() has never been called on it, or if
	//      * pth__barrier_init() has not been called on it since the last
	//        pth__barrier_destroy() call on it.

    extern char*    pth__barrier_destroy(Barrier* barrierp);			// http://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_barrier_init.html
        //
        // Undo the effects of   pth__barrier_init ()   on the barrier.
	// ("Destroy" is poor nomenclature; "reset" would be better.)
        //
        //  o Calling pth__barrier_destroy() immediately after a
	//    pth__barrier_wait() call is safe and typical.
	//    To ensure it is done exactly once, it is convenient
        //    to call pth__barrier_destroy() iff pth__barrier_wait()
	//    returns TRUE.
        //
	//  o Behavior is undefined if pth__barrier_destroy()
	//    is called on an uninitialized barrier.
        //    (In particular, behavior is undefined if
        //    pth__barrier_destroy() is called twice in a
	//    row on a barrier.)
	//
	//  o Behavior is undefined if pth__barrier_destroy()
	//    is called when a pthread is blocked on the barrier.





#endif // _ASM_ 



#ifndef HEAP_IMAGE_SYMBOL
#define HEAP_IMAGE_SYMBOL       "lib7_heap_image"
#define HEAP_IMAGE_LEN_SYMBOL   "lib7_heap_image_len"

#endif



#endif // RUNTIME_BASE_H


// COPYRIGHT (c) 1992 AT&T Bell Laboratories
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2011,
// released under Gnu Public Licence version 3.


