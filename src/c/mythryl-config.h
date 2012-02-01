// mythryl-config.h
//
// The architectural idea here is to have
//
//     config.h
//
// (which is autogenerated by the GNU autoconfig toolset)
// record which resources are available on the build machine,
// and then record in this file manual policy decisions such
// as whether to take advantage of a given resource.
//
// Thus, we #include config.h here, and then #include (only)
//
//     mythryl-config.h
//
// in all our C source files.
//
// By convention symbols defined in config.h start with the prefix
//
//     HAVE_
//
// whereas symbols defined in mythryl-config.h start with the prefix
//
//     NEED_
//
//                -- 2011-10-28 CrT

#ifndef MYTHRYL_CONFIG_H
#define MYTHRYL_CONFIG_H 1

#include "config.h"



#define NEED_SOFTWARE_GENERATED_PERIODIC_EVENTS 1
    //
    // The Mythryl heapcleaner ("garbage collector") runs as a cooperative
    // thread with user code -- the Mythryl compiler ensures that every closed
    // loop through the code calls the garbage collector probe at least once.
    //
    // This facility takes advantage of that to allow invocation of arbitrary
    // user code on a regular basis.  This provides an alternative to (say)
    // using kernel-generated SIGVTALRM calls, which have the disadvantages
    // of higher overhead, and of interrupting "slow" system calls, which must
    // then be explicitly restarted -- something which many C library functions
    // probably do not do correctly.  



#define NEED_PTHREAD_SUPPORT 1
    //
    // Define this as 1 (i.e. TRUE) to compile in support
    // for multiple posix threads running Mythryl code in
    // parallel in the same address space.  For background
    // see    src/A.PTHREAD-SUPPORT.OVERVIEW



#define NEED_PTHREAD_SUPPORT_FOR_SOFTWARE_GENERATED_PERIODIC_EVENTS 1
    //
    // Define this as 1 (i.e. TRUE) to compile in support.
    //
    // Currently this must be TRUE whenever NEED_PTHREAD_SUPPORT
    // is TRUE.  (I have a feeling this would be a good switch
    // to get rid of.  -- 2011-01-02 CrT) 
    //
    // This switch affects the files:
    // 
    //     src/c/h/runtime-base.h				// pthread section.
    //     src/c/heapcleaner/pthread-heapcleaner-stuff.c
    //     src/c/heapcleaner/call-heapcleaner.c
    //     src/c/main/run-mythryl-code-and-runtime-eventloop.c


#if NEED_PTHREAD_SUPPORT
    //
    #define MAX_PTHREADS	8
	//
	// Max number of posix threads running Mythryl.
	// Don't be profligatehere :  We statically
	// dedicate about 256K of memory to each one --
	// see DEFAULT_AGEGROUP0_BUFFER_BYTESIZE in
	//
	//     src/c/h/runtime-configuration.h
#else
    #define MAX_PTHREADS	1
#endif

#define NEED_PTHREAD_DEBUG_SUPPORT 0
    //
    // Set this to TRUE to Log pthread-related stuff
    // via the log_if fn from   src/c/main/error-reporting.c
    // NB: Doing this during a full build of the compiler
    //     will produce a logfile gigabytes long.

#define PTHREAD_LOG_IF   if (NEED_PTHREAD_DEBUG_SUPPORT) log_if
    //
    // The idea here is that 
    //
    //     PTHREAD_LOG_IF ("Starting to foo the %s\n", bar);
    //
    // is a lot less clutter than
    //
    //     #if NEED_PTHREAD_DEBUG_SUPPORT
    //         log_if ("Starting to foo the %s\n", bar);
    //     #endif
    //
    // Also, the former provides typechecking even when
    // PTHREAD_LOG_IF == 0   -- much more bitrot-resistant.


#define NEED_HEAPCLEANER_PAUSE_STATISTICS 0
    //
    // Define this at 1 (TRUE) to compile in code tracking pause times
    // for the heapcleaner ("garbage collector") code.  This is supported
    // only on posix operating systems.  This affects the files
    //
    //    src/c/heapcleaner/heapcleaner-statistics.h
    //
    //    src/c/heapcleaner/heapcleaner-initialization.c
    //    src/c/heapcleaner/heapclean-n-agegroups.c
    //    src/c/heapcleaner/call-heapcleaner.c
    //    src/c/heapcleaner/call-heapcleaner.c



#define NEED_HUGECHUNK_REFERENCE_STATISTICS 0
    //
    // Define this at 1 (TRUE) to compile in code tracking
    // hugechunk references.  This affects only the file
    //
    //     src/c/heapcleaner/heapclean-n-agegroups.c



// #define MAX_POSIX_SIGNALS      60			// Redundant with def in src/c/o/system-signals.h
    //
    // This needs to be at least one more than the
    // highest valid signal number on the host system.
    //
    // This value gets used in    src/c/h/runtime-base.h
    // to size the                posix_signal_counts []
    // vector and in              src/c/main/runtime-state.c
    // and                        src/c/machine-dependent/signal-stuff.c
    // to iterate over that vector.
    // 
    // The proper value for this symbol is typically about 30
    // and doesn't change much, so just hardwiring it to 60 should be
    // reasonably safe, if not particularly elegant.
    //
    // (I'm reluctant to autoconfig it at compile time based on (say)
    // `kill -l` because we might well (say) upgrade to a kernel supporting
    // more signals and wind up getting weird memory corruption bugs
    // as a result.)
    //                                         -- 2011-10-30 CrT


#define CACHE_LINE_BYTESIZE 64
    //
    // Size-in-bytes of a processor cache line.
    // For Intel this is documented in as
    //
    //     "64 bytes for Intel Core 2 Duo, Intel Core, Pentium M, and Pentium 4 processors;
    //      32 bytes P6 family and Pentium processors"
    //
    //            -- http://www.intel.com/content/dam/doc/manual/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf
    //
    // You can check this on Linux by doing
    //
    //     grep . /sys/devices/system/cpu/cpu0/cache/index*/* | more
    //
    // and looking at 'coherency_line_size'.
    //
    // This number is non-critical and does not change much;
    // it is safe and sensible to leave it unchanged.
    // We use it in
    //
    //     src/c/pthread/pthread-on-posix-threads.c
    //
    // to try to put each mutex in its own cache line.  This can
    // improve performance because cores typically lock a complete
    // cache line when doing mutex operations, so putting two
    // unrelated mutexes in the same cache line can introduce
    // needless contention between cores.


#endif // MYTHRYL_CONFIG_H
