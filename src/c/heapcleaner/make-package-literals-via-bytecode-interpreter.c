// make-package-literals-via-bytecode-interpreter.c

// Problem
// =======
//
// When we generate a
//
//     foo.pkg.compiled
//
// file, we must somehow preserve foo.pkg's various
// literals and values -- which may include lists, records,
// tuples, trees etc -- on disk in a form allowing reconstitution
// when foo.pkg.compiled is later loaded into a running process.
//
// Solution
// ========
//
// We represent the literals as a bytecode program which
// when executed constructs the required values.  That is
// the job of the
//
//     Val   make_package_literals_via_bytecode_interpreter   (Task* task,   Unt8* bytecode_vector,   int bytecode_vector_length_in_bytes)
//
// function in this file.  Our
//
//    bytecode_vector
//    bytecode_vector_length_in_bytes
//
// arguments give the bytecode vector to execute,
// which was generated in
//
//     src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg

#include "../mythryl-config.h"

#include "runtime-base.h"
#include "make-strings-and-vectors-etc.h"
#include "heap.h"
#include <string.h>

// Codes for literal machine instructions (version 1):
//   INT(i)		0x01 <i>
//   RAW32[i]		0x02 <i>
//   RAW32[i1,..,in]	0x03 <n> <i1> ... <in>
//   RAW64[r]		0x04 <r>
//   RAW64[r1,..,rn]	0x05 <n> <r1> ... <rn>
//   STR[c1,..,cn]	0x06 <n> <c1> ... <cn>
//   LIT(k)		0x07 <k>			// Dup k-th element on stack.
//   VECTOR(n)		0x08 <n>			// Push vector containing n elements popped from stack.
//   RECORD(n)		0x09 <n>			// Push record containing n elements popped from stack.
//   RETURN		0xff				// Return top-of-stack value.
//
//
// These MUST be kept in sync with the bytecode generation logic in
//
//     src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
//
#define V1_MAGIC	0x19981022							// Generated by put_magic	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
#define I_INT		0x01								// Generated by put_int		in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
//
#define I_RAW32		0x02								// Generated by put_raw32	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
#define I_RAW32L	0x03	// "L" == "List"					// Generated by put_raw32	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
//
#define I_RAW64		0x04								// Generated by put_raw64	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
#define I_RAW64L	0x05	// "L" == "List"					// Generated by put_raw64	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
//
#define I_STR		0x06    // "STR" == "String"					// Generated by put_string	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
#define I_LIT		0x07	// "LIT" == "Literal"					// Generated by put_lit		in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
//
#define I_VECTOR	0x08								// Generated by put_vector	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
#define I_RECORD	0x09								// Generated by put_record	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg
//
#define I_RETURN	0xff								// Generated by put_return	in    src/lib/compiler/back/top/main/make-nextcode-literals-bytecode-vector.pkg


#define _B0(p)		((p)[pc])
#define _B1(p)		((p)[pc+1])
#define _B2(p)		((p)[pc+2])
#define _B3(p)		((p)[pc+3])

#define GET32(p)	\
    ((_B0(p) << 24) | (_B1(p) << 16) | (_B2(p) << 8) | _B3(p))

#define LIST_CONS_CELL_BYTESIZE	(WORD_BYTESIZE*3)		// Size of a list cons cell in bytes.



static double   get_double   (Unt8* p)   {
    //          =========
    //
    union {
	double	d;
	Unt8	b[ sizeof( double ) ];
    } u;

    #ifdef BYTE_ORDER_LITTLE
	//
	for (int i = sizeof(double);   i --> 0;  ) {
	    //
	    u.b[i] =  *p++;
	}
    #else
	for (int i = 0;   i < sizeof(double);   i++) {
	    //
	    u.b[i] = p[i];
	}
    #endif

    return u.d;
}




Val   make_package_literals_via_bytecode_interpreter   (Task* task,   Unt8* bytecode_vector,   int bytecode_vector_length_in_bytes)   {
    //==============================================
    //
    // NOTE: We allocate all of the chunks in agegroup 1,
    // but allocate the vector of literals in agegroup0.
    //
    // This fn gets exported to the Mythryl level as
    //
    //     make_package_literals_via_bytecode_interpreter
    // in
    //     src/lib/compiler/execution/code-segments/code-segment.pkg
    // via
    //     src/c/lib/heap/make-package-literals-via-bytecode-interpreter.c
    //
    // Our ultimate invocation is in
    //
    //     src/lib/compiler/execution/main/execute.pkg

								check_agegroup0_overrun_tripwire_buffer( task, "make_package_literals_via_bytecode_interpreter/AAA" );

    int pc = 0;

    // Check that sufficient space is available for the
    // literal chunk that we are about to allocate.
    // Note that the cons cell has already been accounted
    // for in space_available (but not in space_needed).
    //
    #define GC_CHECK											\
	do {												\
	    if (space_needed > space_available								\
            &&  need_to_call_heapcleaner( task, space_needed + LIST_CONS_CELL_BYTESIZE)			\
            ){												\
		call_heapcleaner_with_extra_roots (task, 0, (Val *)&bytecode_vector, &stk, NULL);	\
		space_available = 0;									\
													\
	    } else {											\
													\
		space_available -= space_needed;							\
	    }												\
	} while (0)

    #ifdef DEBUG_LITERALS
	debug_say("make_package_literals_via_bytecode_interpreter: bytecode_vector = %#x, bytecode_vector_length_in_bytes = %d\n", bytecode_vector, bytecode_vector_length_in_bytes);
    #endif

    if (bytecode_vector_length_in_bytes <= 8)   return HEAP_NIL;

    Val_Sized_Unt  magic
	=
	GET32(bytecode_vector);   pc += 4;

    Val_Sized_Unt  max_depth							/* This variable is currently unused, so suppress 'unused var' compiler warning: */   __attribute__((unused))
	=
	GET32(bytecode_vector);   pc += 4;

    if (magic != V1_MAGIC) {
	die("bogus literal magic number %#x", magic);
    }

    Val	stk = HEAP_NIL;

    int space_available = 0;

    for (;;) {
	//
	ASSERT(pc < bytecode_vector_length_in_bytes);

	space_available -= LIST_CONS_CELL_BYTESIZE;	// Space for stack cons cell.

	if (space_available < ONE_K_BINARY) {
	    //
	    if (need_to_call_heapcleaner(task, 64*ONE_K_BINARY)) {
		//
		call_heapcleaner_with_extra_roots (task, 0, (Val *)&bytecode_vector, &stk, NULL);
            }
	    space_available = 64*ONE_K_BINARY;
	}


	switch (bytecode_vector[ pc++ ]) {
	    //
	case I_INT:
	    {	int i = GET32(bytecode_vector);	pc += 4;

		#ifdef DEBUG_LITERALS
		    debug_say("[%2d]: INT(%d)\n", pc-5, i);
		#endif

		LIST_CONS(task, stk, TAGGED_INT_FROM_C_INT(i), stk);
	    }
	    break;

	case I_RAW32:
	    {
		int i = GET32(bytecode_vector);	pc += 4;

		#ifdef DEBUG_LITERALS
		    debug_say("[%2d]: RAW32[%d]\n", pc-5, i);
		#endif

		Val               result;
		INT1_ALLOC(task, result, i);

		LIST_CONS(task, stk, result, stk);
		space_available -= 2*WORD_BYTESIZE;
	    }
	    break;

	case I_RAW32L:
	    {
		int n = GET32(bytecode_vector);	pc += 4;

		#ifdef DEBUG_LITERALS
		debug_say("[%2d]: RAW32L(%d) [...]\n", pc-5, n);
		#endif

		ASSERT(n > 0);

		int space_needed = 4*(n+1);
		GC_CHECK;

		LIB7_AllocWrite (task, 0, MAKE_TAGWORD(n, FOUR_BYTE_ALIGNED_NONPOINTER_DATA_BTAG));

		for (int j = 1;  j <= n;  j++) {
		    //
		    int i = GET32(bytecode_vector);	pc += 4;

		    LIB7_AllocWrite (task, j, (Val)i);
		}

		Val result =  LIB7_Alloc(task, n );

		LIST_CONS(task, stk, result, stk);
	    }
	    break;

	case I_RAW64:
	    {
		double d = get_double(&(bytecode_vector[pc]));	pc += 8;

		Val	           result;
		REAL64_ALLOC(task, result, d);

		#ifdef DEBUG_LITERALS
		    debug_say("[%2d]: RAW64[%f] @ %#x\n", pc-5, d, result);
		#endif

		LIST_CONS(task, stk, result, stk);

		space_available -= 4*WORD_BYTESIZE;		// Extra 4 bytes for alignment padding.
	    }
	    break;

	case I_RAW64L:
	    {
		int n = GET32(bytecode_vector);	pc += 4;

		#ifdef DEBUG_LITERALS
		    debug_say("[%2d]: RAW64L(%d) [...]\n", pc-5, n);
		#endif

		ASSERT(n > 0);

		int space_needed = 8*(n+1);
		GC_CHECK;

		#ifdef ALIGN_FLOAT64S
		    // Force FLOAT64_BYTESIZE alignment (descriptor is off by one word)
		    //
		    task->heap_allocation_pointer = (Val*)((Punt)(task->heap_allocation_pointer) | WORD_BYTESIZE);
		#endif

		int j = 2*n;							// Number of words.

		LIB7_AllocWrite (task, 0, MAKE_TAGWORD(j, EIGHT_BYTE_ALIGNED_NONPOINTER_DATA_BTAG));

		Val result =  LIB7_Alloc(task, j );

		for (int j = 0;  j < n;  j++) {
		    //
		    PTR_CAST(double*, result)[j] = get_double(&(bytecode_vector[pc]));	pc += 8;
		}
		LIST_CONS(task, stk, result, stk);
	    }
	    break;

	case I_STR:
	    {
		int n = GET32(bytecode_vector);		pc += 4;

		#ifdef DEBUG_LITERALS
		    debug_say("[%2d]: STR(%d) [...]", pc-5, n);
		#endif

		if (n == 0) {
		    #ifdef DEBUG_LITERALS
			debug_say("\n");
		    #endif

		    LIST_CONS(task, stk, ZERO_LENGTH_STRING__GLOBAL, stk);

		    break;
		}

		int j = BYTES_TO_WORDS(n+1);								// '+1' to include space for '\0'.

		// The space request includes space for the data-chunk header word and
		// the sequence header chunk.
		//
		int space_needed = WORD_BYTESIZE*(j+1+3);
		GC_CHECK;

		// Allocate the data chunk:
		//
		LIB7_AllocWrite(task, 0, MAKE_TAGWORD(j, FOUR_BYTE_ALIGNED_NONPOINTER_DATA_BTAG));
		LIB7_AllocWrite (task, j, 0);								// So word-by-word string equality works.

		Val result = LIB7_Alloc (task, j);

		#ifdef DEBUG_LITERALS
		    debug_say(" @ %#x (%d words)\n", result, j);
		#endif
		memcpy (PTR_CAST(void*, result), &bytecode_vector[pc], n);		pc += n;

		// Allocate the header chunk:
		//
		SEQHDR_ALLOC(task, result, STRING_TAGWORD, result, n);

		// Push on stack:
		//
		LIST_CONS(task, stk, result, stk);
	    }
	    break;

	case I_LIT:
	    {
		int n = GET32(bytecode_vector);	pc += 4;

		Val result = stk;

		for (int j = 0;  j < n;  j++) {
		    //
		    result = LIST_TAIL(result);
		}

		#ifdef DEBUG_LITERALS
		    debug_say("[%2d]: LIT(%d) = %#x\n", pc-5, n, LIST_HEAD(result));
		#endif

		LIST_CONS(task, stk, LIST_HEAD(result), stk);
	    }
	    break;

	  case I_VECTOR:
	    {
		int n = GET32(bytecode_vector);	pc += 4;

		#ifdef DEBUG_LITERALS
		    debug_say("[%2d]: VECTOR(%d) [", pc-5, n);
		#endif

		if (n == 0) {
		    #ifdef DEBUG_LITERALS
			debug_say("]\n");
		    #endif
		    LIST_CONS(task, stk, ZERO_LENGTH_VECTOR__GLOBAL, stk);
		    break;
		}

		// The space request includes space
		// for the data-chunk header word and
		// the sequence header chunk.
		//
		int space_needed = WORD_BYTESIZE*(n+1+3);
		GC_CHECK;

		// Allocate the data chunk:
		//
		LIB7_AllocWrite(task, 0, MAKE_TAGWORD(n, RO_VECTOR_DATA_BTAG));

		// Top of stack is last element in vector:
		//
		for (int j = n;  j > 0;  j--) {
		    //
		    LIB7_AllocWrite(task, j, LIST_HEAD(stk));

		    stk = LIST_TAIL(stk);
		}

		Val result =  LIB7_Alloc(task, n );

		// Allocate the header chunk:
		//
		SEQHDR_ALLOC(task, result, TYPEAGNOSTIC_RO_VECTOR_TAGWORD, result, n);

		#ifdef DEBUG_LITERALS
		    debug_say("...] @ %#x\n", result);
		#endif

		LIST_CONS(task, stk, result, stk);
	    }
	    break;

	case I_RECORD:
	    {
		int n = GET32(bytecode_vector);	pc += 4;

		#ifdef DEBUG_LITERALS
		    debug_say("[%2d]: RECORD(%d) [", pc-5, n);
		#endif

		if (n == 0) {
		    #ifdef DEBUG_LITERALS
			debug_say("]\n");
		    #endif

		    LIST_CONS(task, stk, HEAP_VOID, stk);
		    break;

		} else {

		    int space_needed = 4*(n+1);
		    GC_CHECK;

		    LIB7_AllocWrite(task, 0, MAKE_TAGWORD(n, PAIRS_AND_RECORDS_BTAG));
		}

		// Top of stack is last element in record:
		//
		for (int j = n;  j > 0;  j--) {
		    //
		    LIB7_AllocWrite(task, j, LIST_HEAD(stk));

		    stk = LIST_TAIL(stk);
		}

		Val result = LIB7_Alloc(task, n );

		#ifdef DEBUG_LITERALS
		    debug_say("...] @ %#x\n", result);
		#endif

		LIST_CONS(task, stk, result, stk);
	    }
	    break;

	case I_RETURN:
	    ASSERT(pc == bytecode_vector_length_in_bytes);

	    #ifdef DEBUG_LITERALS
	        debug_say("[%2d]: RETURN(%#x)\n", pc-5, LIST_HEAD(stk));
	    #endif

								check_agegroup0_overrun_tripwire_buffer( task, "make_package_literals_via_bytecode_interpreter/ZZZ" );
	    return  LIST_HEAD( stk );
	    break;

	default:
	    die ("bogus literal opcode #%x @ %d", bytecode_vector[pc-1], pc-1);
	}								// switch
    }									// while
}									// fun make_package_literals_via_bytecode_interpreter


// COPYRIGHT (c) 1997 Bell Labs, Lucent Technologies.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2011,
// released under Gnu Public Licence version 3.

