/* solaris-mp.c
 *
 * MP support for Sparc multiprocessor machines running Solaris 2.5
 *
 * Solaris implementation of externals defined in $(INCLUDE)/runtime-mp.h
 */

#include "../config.h"

#include <stdio.h>
#include <sys/mman.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <thread.h>
#include <synch.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <sys/processor.h>
#include <sys/procset.h>
#include "runtime-limits.h"
#include "runtime-values.h"
#include "runtime-heap.h"
#include "tags.h"
#include "runtime-mp.h"
#include "runtime-state.h"
#include "runtime-globals.h"
#include "vproc-state.h"


#define INT_LIB7inc(n,i)  ((lib7_val_t)INT_CtoLib7(INT_LIB7toC(n) + (i)))
#define INT_LIB7dec(n,i)  (INT_LIB7inc(n,(-i)))

/* local functions */
static mp_lock_t AllocLock();
static mp_barrier_t *AllocBarrier();
static void *AllocArenaMem(int size);
static void FreeArenaMem(void *, int);
static void *ProcMain(void *lib7_state);
static void *ResumeProc(void *vlib7_state);
static void SuspendProc(lib7_state_t *lib7_state);
static lib7_state_t **InitProcStatesArray();
static void BindToRealProc(processorid_t *);

/* locals */
static caddr_t arena;   /* arena for shared sync chunks */
static mp_lock_t arenaLock;    /* must be held to alloc/free a lock */
static mp_lock_t MP_ProcLock;     /* must be used to acquire/release procs */
static lib7_state_t **procStates; /*[MAX_NUM_PROCS]*/ /* list of states of suspended
			      procs */
#if defined(MP_PROFILE)
static int *doProfile;
#endif

#define LEAST_PROCESSOR_ID       0
#define GREATEST_PROCESSOR_ID    3

#define NextProcessorId(id)  (((id) == GREATEST_PROCESSOR_ID) ? LEAST_PROCESSOR_ID : (id) + 1)

static processorid_t *processorId; /* processor id of the next processor a lwp 
				    will be bound to */
/* globals */
mp_lock_t	MP_GCLock;
mp_lock_t	MP_GCGenLock;
mp_barrier_t	*MP_GCBarrier;
mp_lock_t	MP_TimerLock;

#if defined(MP_PROFILE)
int mutex_trylock_calls;
int trylock_calls;
#endif

/* MP_Init:
 */
void MP_Init()
{
  int fd;

  if ((fd = open("/dev/zero",O_RDWR)) == -1)
      Die("MP_Init:Couldn't open /dev/zero");

  arena = mmap((caddr_t) 0, sysconf(_SC_PAGESIZE),PROT_READ | PROT_WRITE ,MAP_PRIVATE,fd,0);
  
    arenaLock           = AllocLock();
    MP_ProcLock	        = AllocLock();
    MP_GCLock		= AllocLock();
    MP_GCGenLock	= AllocLock();
    MP_TimerLock	= AllocLock();
    MP_GCBarrier	= AllocBarrier(); 
    procStates          = InitProcStatesArray();
    ASSIGN(ActiveProcs, INT_CtoLib7(1));
#ifdef MP_NONBLOCKING_IO
    MP_InitStdInReader  ();
#endif
    processorId         = (processorid_t *) AllocArenaMem(sizeof(processorid_t));
    *processorId        = -1;
    BindToRealProc(processorId);

#ifdef MP_PROFILE
    doProfile = (int *) AllocArenaMem(sizeof(int));
    *doProfile = 0;
#endif
    NextProcessorId(*processorId);

    /* thr_setconcurrency(MAX_NUM_PROCS); */  

} /* end of MP_Init */

/*************************************************************************
 * Function: static mp_state_t **InitProcStatesArray()
 * Purpose: Initialize the array of pointers to ml states of suspended 
 *          processors
 * Return: The initialized array as a pointer to pointers.
 *************************************************************************/
 static lib7_state_t **InitProcStatesArray()
{
  lib7_state_t **array;
  lib7_state_t **ptr;
  int i;

  array = (lib7_state_t **) AllocArenaMem(sizeof(lib7_state_t *));

  for (i=1; i < MAX_NUM_PROCS; i++)
    {
      ptr =  (lib7_state_t **) AllocArenaMem(sizeof(lib7_state_t *));
      *ptr = (lib7_state_t *) NULL;
      ptr++;
    }
  
  return (array);

} /* end of InitProcStatesArray */
/*************************************************************************
 * Function: static mp_lock_t AllocLock()
 * Purpose: Allocate a portion of the arena of synch chunks for a spin 
            lock.
 * Returns: returns a pointer to the allocated region.
 * Created: 5-14-96 	   
*************************************************************************/
static mp_lock_t AllocLock()
{
  mp_lock_t lock;

  lock = (mp_lock_t) AllocArenaMem(MP_LOCK_SZ);
  
  lock->value = UNSET;

  if (mutex_init(&lock->mutex, USYNC_THREAD, NULL) == -1)
    Die("AllocLock: unable to initialize mutex");

  return lock;

} /* end of AllocLock */

/**************************************************************************
 * Function: FreeLock 
 * Purpose : Destroy the mutex. In addition, if the lock was the last chunk 
             allocated in the arena then recapture the space occupied by the 
	     lock. Otherwise, zero out the space occupied by the lock.
 * Created : 5-14-96
 **************************************************************************/
static void FreeLock(mp_lock_t lock)
{
#if defined(MP_LOCK_DEBUG)
  printf("arena = %ld\t lock = %ld\n",(int) arena, lock);
#endif
 
    mutex_destroy(&lock->mutex);

    FreeArenaMem(lock,MP_LOCK_SZ);

} /* end of FreeLock */

/*************************************************************************
 * Function: void BindToRealProc(processorid_t *)
 * Purpose: Bind the current lwp to a real processor. Attempt to bind the
            lwp to a processor different from the last processor a lwp was 
            bound to.
 * Created: 7-22-96 	
*************************************************************************/
void BindToRealProc(processorid_t *processorId)
{
  processorid_t procId = *processorId;
  processorid_t obind;
  int lwpBoundP = 0;

  while (!lwpBoundP) 
    {
      procId = NextProcessorId(procId);
      if (procId == *processorId)  /* attempts made to bind on all processors */
      {
	fprintf(stderr, "lwp was not bound to a processor.\n");
	lwpBoundP = 1;
      }
      else
	{
	  if (processor_bind(P_LWPID, P_MYID, procId, &obind) == -1)
	    {
	      fprintf(stderr, "error attempting to bind lwp to processor [%d]\n",(int) procId);
	      lwpBoundP = 1;
	    }
	  else
	    {
	      if (obind == PBIND_NONE) /* couldn't bind to lwp */
		  fprintf(stderr, "couldn't bind current lwp to processor [%d]\n", (int) procId);
	      else
		{
		  fprintf(stderr,"lwp bound to processor [%d]\n",procId);
		  lwpBoundP = 1;
		  *processorId = procId;
		}
	    }
	}
    }

} /* end of BindToRealProc */

/*************************************************************************
 * Function: bool_t MP_TryLock(mp_lock_t lock)
 * Purpose: Return FALSE if cannot set lock; otherwise set lock and return
            TRUE.
 * Created: 5-14-96 	
 * Invariant: If more than one processes calls MP_TryLock at the same time, 
              then only one of the processes will have TRUE returned.
*************************************************************************/

bool_t MP_TryLock(mp_lock_t lock)
{
#if defined(MP_PROFILE)
  long cpuTime;
#endif

#if defined(MP_LOCK_DEBUG)
  printf("MP_TryLock: lock value is %d\n",lock->value);
#endif

#if defined(MP_PROFILE)
  if (*doProfile)
    {
      cpuTime = (long) clock();
      printf("trylock_calls = %d\n",++trylock_calls);
    }
#endif

  /* We test to see if the lock is set here so that we can reduce the number
     of calls to mutex_trylock when we are waiting for the lock to be 
     released. Apparently repeated calls to mutex_trylock floods the bus.
     I don't know why. I found this out from the Threads Primer book.
   */ 
  if (lock->value == SET)
#if defined(MP_PROFILE)
    {
      if (*doProfile)  
	fprintf(stderr,"MP_Trylock:cpu time %ld\n",(long) clock() - cpuTime);
      return(FALSE);
    }
#else
    return(FALSE);
#endif
  else
    {
    #if defined(MP_LOCK_DEBUG)
      printf("MP_TryLock: calling mutex_trylock\n");
    #endif

    #if defined(MP_PROFILE)
      if (*doProfile)
	printf("mutex_trylock_calls = %d\n",++mutex_trylock_calls);
    #endif

      if (mutex_trylock(&lock->mutex) == EBUSY)
#if defined(MP_PROFILE)
	if (*doProfile)
	  fprintf(stderr,"MP_Trylock:cpu time %ld\n",(long) clock() - cpuTime);
#else
	return(FALSE);
#endif
      else
	{
	  if (lock->value == SET)
	    {
	      mutex_unlock(&lock->mutex);
#if defined(MP_PROFILE)
	      if (*doProfile)
		fprintf(stderr,"MP_Trylock:cpu time %ld\n",(long) clock() - cpuTime);
#endif
	      return(FALSE);
	    }

	  lock->value = SET;
	  mutex_unlock(&lock->mutex);
#if defined(MP_PROFILE)
	if (*doProfile)
	  fprintf(stderr,"MP_Trylock:cpu time %ld\n",(long) clock() - cpuTime);
#endif
	  return(TRUE);
	}
    }
} /* end of MP_TryLock */


/*************************************************************************
 * Function: void MP_UnsetLock(mp_lock_t lock)
 * Purpose: Assign lock->value the value of 0.
 * Created: 5-14-96 	   
*************************************************************************/

void MP_UnsetLock(mp_lock_t lock)
{
  lock->value = UNSET;
} 

/*************************************************************************
 * Function: void MP_SetLock(mp_lock_t lock)
 * Purpose: Busy wait until able set the lock.
 * Created: 5-14-96 	   
*************************************************************************/
void MP_SetLock(mp_lock_t lock)
{
  while (MP_TryLock(lock) == FALSE)  ;
} 


/* MP_AllocLock:
 */
mp_lock_t MP_AllocLock()
{
  mp_lock_t lock;

  MP_SetLock(arenaLock);
     lock = AllocLock();
  MP_UnsetLock(arenaLock);

  return lock;
}  /* end of MP_AllocLock */

/*************************************************************************
 * Function: void MP_FreeLock (mp_lock_t lock)
 * Purpose: Destroy mutex of lock and free memory occupied by lock.
 * Returns: returns non-negative int if OK, -1 on error
 * Created: 5-13-96 	   
*************************************************************************/

void MP_FreeLock (mp_lock_t lock)
{
  MP_SetLock(arenaLock);
     FreeLock(lock);
  MP_UnsetLock(arenaLock);
} 

/*************************************************************************
 * Function: AllocBarrier
 * Purpose: Get a chunk of memory from the arena for a barrier and 
            initialize it.
 * Returns: Return a pointer to the barrier.
 * Created: 5-15-96 	   
*************************************************************************/

static mp_barrier_t *AllocBarrier ()
{
  mp_barrier_t *barrierp;

  barrierp = (mp_barrier_t *) arena;
  arena += MP_BARRIER_SZ;

  barrierp->n_waiting = 0; 
  barrierp->phase = 0; 
  
  if (mutex_init(&barrierp->lock, USYNC_THREAD, NULL) == -1)
    Die("MP_Barrier: could not init barrier mutex lock");

  if (cond_init(&barrierp->wait_cv, USYNC_THREAD, NULL) == -1)
    Die("MP_Barrier: could not init conditional var of barrier");


  return barrierp;
  
} /* end of AllocBarrier */


/*************************************************************************
 * Function: MP_AllocBarrier
 * Purpose: Allocate a barrier from the synch chunk arena. Allocation is
            mutually exclusive. Note the barrier is not initialized.
 * Returns: Return a pointer to the barrier.
 * Created: 5-15-96 	   
*************************************************************************/
mp_barrier_t *MP_AllocBarrier ()
{
  mp_barrier_t *barrierp;

  MP_SetLock(arenaLock);
      barrierp = AllocBarrier ();
  MP_UnsetLock(arenaLock);

  return barrierp;

} /* end of MP_AllocBarrier */

/*************************************************************************
 * Function: MP_AllocBarrier
 * Purpose: destroy mutex and conditional variables of the barrier.
            Regain memory if barrier was last chunk allocated in arena;
            otherwise zero out the memory occupied by the barrier.
 * Returns: Nothing.
 * Created: 5-15-96 	   
*************************************************************************/
void FreeBarrier(mp_barrier_t *barrierp)
{
  mutex_destroy(&barrierp->lock);
  cond_destroy(&barrierp->wait_cv);

  FreeArenaMem(barrierp, MP_BARRIER_SZ);
} /* end of FreeBarrier */

void MP_FreeBarrier(mp_barrier_t *barrierp)
{
  MP_SetLock(arenaLock);
     FreeBarrier(barrierp);
  MP_UnsetLock(arenaLock);
} /* end of MP_FreeBarrier */


/*************************************************************************
 * Function: MP_Barrier
 * Purpose: Wait until the required number of threads enter the barrier.
 * Returns: Nothing.
 * Created: 5-15-96 
 * Invariant: barrierp->n_waiting <= n_clients	   
*************************************************************************/

void MP_Barrier(mp_barrier_t *barrierp, unsigned n_clients)
{
  int my_phase;

  mutex_lock(&barrierp->lock);

  my_phase = barrierp->phase;
  barrierp->n_waiting++;

  if (barrierp->n_waiting == n_clients) 
    {
      barrierp->n_waiting = 0;
      barrierp->phase = 1 - my_phase;
      cond_broadcast(&barrierp->wait_cv);
    }
	
  /* Wait for the end of this synchronization phase */
  while (barrierp->phase == my_phase) 
    {
      cond_wait(&barrierp->wait_cv, &barrierp->lock);
    }

  mutex_unlock(&barrierp->lock);

} /* end of MP_Barrier */

/*************************************************************************
 * Function: MP_ResetBarrier
 * Purpose: Set the various values of the barrier to zero.
 * Returns: Nothing.
 * Created: 5-15-96 	   
*************************************************************************/
void MP_ResetBarrier(mp_barrier_t *barrierp)
{
  barrierp->n_waiting = 0; 
  barrierp->phase = 0; 
   
} /* end of MP_ResetBarrier */



/*************************************************************************
 * Function: AllocArenaMem
 ************************************************************************/

static void *AllocArenaMem(int size)
{
  void *obj;

  obj = arena;
  arena += size;

  return obj;
}

/*************************************************************************
 * Function: FreeArenaMem
 ************************************************************************/
static void FreeArenaMem(void *p, int size)
{
  if (arena == (caddr_t) p + size)
    arena -= size;
  else
    memset(p,0,size);
}

/*************************************************************************
 * Function: ResumeProc(lib7_state_t *lib7_state)
 * Purpose:  Resumes a proc to either perform garbage collection or to 
 *           run ml with the given ml state.
 * Return:   Nothing
 ************************************************************************/
static void *ResumeProc(void *vlib7_state)
{
  lib7_state_t *lib7_state = (lib7_state_t *) vlib7_state;

  MP_SetLock(MP_ProcLock);
  if (lib7_state->lib7_vproc->vp_mpState == MP_PROC_SUSPENDED) 
    {
      /* proc only resumed to do a gc */
#ifdef MP_DEBUG
      SayDebug("resuming %d to perform a gc\n",lib7_state->lib7_vproc->vp_mpSelf);
#endif      
      lib7_state->lib7_vproc->vp_mpState == MP_PROC_GC;
      MP_UnsetLock(MP_ProcLock);
      
      /* the GC will be performed when we call MP_ReleaseProc */
      
      MP_ReleaseProc(lib7_state);
    }
  else
    {
#ifdef MP_DEBUG
      SayDebug("[release_proc: resuming proc %d]\n",lib7_state->lib7_vproc->vp_mpSelf);
#endif
      MP_UnsetLock(MP_ProcLock);
      RunLib7(lib7_state);
      Die ("return after RunLib7(lib7_state) in mp_release_proc\n");
    }
} /* end of ResumeProc */

/*************************************************************************
 * Function: MP_ResumeVProcs(int n_procs)
 * Purpose: Remove n_procs states from the list of states and spawn threads 
 *          to execute them. 
 * Note: We assume that calls to this function are mutually exclusive.
 * Return: Return a pointer to the last state resumed.
 ************************************************************************/
vproc_state_t *MP_ResumeVProcs(int n_procs)
{
  lib7_state_t *statep;
  int i = 0;

  while(i < MAX_NUM_PROCS && n_procs > 0) {

    if ((statep = procStates[i]) != (lib7_state_t *) NULL)    /* get a state */
      {
	/* spawn a thread to execute the state */
#ifdef MP_DEBUG
	SayDebug("Resuming proc %d\n",statep->lib7_vproc->vp_mpSelf);
#endif	
	if(thr_create(NULL,0,ResumeProc,(void *)statep,NULL,NULL) != 0)
	  Die("Could create a thread to resume processors");
	
	procStates[i] = NULL;
	i++;
	n_procs--;
      }
    else
      i++;
  }

  if (statep == (lib7_state_t *) NULL)
    return (vproc_state_t *) NULL;

  return statep->lib7_vproc;

} /* end of MP_ResumeVProcs */

/*************************************************************************
 * Function: SuspendProc(lib7_state_t *lib7_state)
 * Purpose: Suspend the calling proc, add its state, lib7_state, to the suspended
 *          proc state list, and kill the thread the proc is running on.
 * Return: Nothing.
 ************************************************************************/
static void SuspendProc(lib7_state_t *lib7_state)
{
  int i=0;  

  MP_SetLock(MP_ProcLock);

  /* check if proc has actually been suspended */

  if (lib7_state->lib7_vproc->vp_mpState != MP_PROC_SUSPENDED) 
    {
#ifdef MP_DEBUG
      SayDebug("proc state is not PROC_SUSPENDED; not suspended");
#endif      
      MP_UnsetLock(MP_ProcLock);
      return;
    }
    

  while (i < MAX_NUM_PROCS) { 
    if (procStates[i] == NULL) 
      {
	procStates[i] = lib7_state; 
	i = MAX_NUM_PROCS;
      }
    else
      i++;
  }

  MP_UnsetLock(MP_ProcLock);

  /* exit the thread */
  thr_exit(NULL); 

} /* end of SuspendProc */

/*************************************************************************
 * Function: MP_ReleaseProc(lib7_state_t *lib7_state)
 ************************************************************************/
void MP_ReleaseProc(lib7_state_t *lib7_state)
{

  
  collect_garbage( lib7_state, 1 );
  
  MP_SetLock(MP_ProcLock);
     lib7_state->lib7_vproc->vp_mpState = MP_PROC_SUSPENDED;
  MP_UnsetLock(MP_ProcLock);

  /* suspend the proc */
#ifdef MP_DEBUG
  SayDebug("suspending proc %d\n",lib7_state->lib7_vproc->vp_mpSelf);
#endif
  SuspendProc(lib7_state);

} /* end of MP_ReleaseProc */

/*************************************************************************
 * Function: ProcMain(lib7_state_t *lib7_state)
 * Purpose: Invoke RunLib7 on lib7_state; die if RunLib7 returns
 ************************************************************************/
static void *ProcMain(void *vlib7_state)
{
  lib7_state_t *lib7_state = (lib7_state_t *) vlib7_state;

  /* spin until we get our id (from return of call to thr_create) */
  while  (lib7_state->lib7_vproc->vp_mpSelf == NULL) {
#ifdef MP_DEBUG
    SayDebug("[waiting for self]\n");
#endif
    continue;
  }
#ifdef MP_DEBUG
  SayDebug ("[new proc main: releasing lock]\n");
#endif
  
  BindToRealProc(processorId);

  MP_UnsetLock(MP_ProcLock); /* implicitly handed to us by the parent */
  RunLib7(lib7_state);                /* should never return */
  Die("proc returned after run_lib7() in ProcMain().\n");

} /* end of ProcMain */
/*************************************************************************
 * Function: MP_AcquireProc(lib7_state_t *lib7_state, lib7_val_t arg)
 ************************************************************************/
lib7_val_t MP_AcquireProc(lib7_state_t *lib7_state, lib7_val_t arg)
{
  lib7_state_t *p;
  vproc_state_t *vsp;
  lib7_val_t v = REC_SEL(arg, 0);
  lib7_val_t f = REC_SEL(arg, 1);
  int i;

#ifdef MP_DEBUG
    SayDebug("[acquiring proc]\n");
#endif

  MP_SetLock(MP_ProcLock);

  /* search for a suspended proc to reuse */
  for (i = 0;
       (i < NumVProcs) && (VProc[i]->vp_mpState != MP_PROC_SUSPENDED);
       i++)
    continue;

#ifdef MP_DEBUG
    SayDebug("[checking for suspended processor]\n");
#endif
  if (i == NumVProcs) 
    {
      if (DEREF(ActiveProcs) == INT_CtoLib7(MAX_NUM_PROCS)) 
	{
	  MP_UnsetLock(MP_ProcLock);
	  Error("[processors maxed]\n");
	  return LIB7_false;
	}
#ifdef MP_DEBUG
    SayDebug("[checking for NO_PROC]\n");
#endif

    /* search for a slot in which to put a new proc */
    for (i = 0;
	 (i < NumVProcs) && (VProc[i]->vp_mpState != MP_PROC_NO_PROC);
	 i++)
      continue;

    if (i == NumVProcs) 
      {
	MP_UnsetLock(MP_ProcLock);
	Error("[no processor to allocate]\n");
	return LIB7_false;
      }

    /* use processor at index i */
    vsp = VProc[i];

    } /* end of then */

  else   /* using a suspended processor */
    {
#ifdef MP_DEBUG
      SayDebug("[using a suspended processor]\n");
#endif     
      vsp = MP_ResumeVProcs(1);
    }

  p = vsp->vp_state;

  p->lib7_exception_fate	= PTR_CtoLib7(handle_v+1);
  p->lib7_argument		= LIB7_void;
  p->lib7_fate			= PTR_CtoLib7(return_c);
  p->lib7_closure		= f;
  p->lib7_program_counter	= 
  p->lib7_link_register		= GET_CODE_ADDR(f);
  p->lib7_current_thread	= v;
     
  if (vsp->vp_mpState == MP_PROC_NO_PROC) 
    {
      mp_pid_t procId;

      /* assume we get one */
      ASSIGN(ActiveProcs, INT_LIB7inc(DEREF(ActiveProcs), 1));
      if (thr_create(NULL,0,ProcMain,(void *)p,THR_NEW_LWP,&((thread_t) procId)) == 0)
	{
#ifdef MP_DEBUG
	  SayDebug ("[got a processor: %d,]\n",procId);
#endif
	vsp->vp_mpState = MP_PROC_RUNNING;
	vsp->vp_mpSelf = procId;
	/* NewProc will release MP_ProcLock */
	return LIB7_true;
	}
      else
	{
	  ASSIGN(ActiveProcs, INT_LIB7dec(DEREF(ActiveProcs), 1));
	  MP_UnsetLock(MP_ProcLock);
	  return LIB7_false;
	}
    }
  else
     {
       /* the thread executing the processor has already been invoked */
      vsp->vp_mpState = MP_PROC_RUNNING;
#ifdef MP_DEBUG
      SayDebug ("[reusing a processor %d]\n",vsp->vp_mpSelf);
#endif
      MP_UnsetLock(MP_ProcLock);
      return LIB7_true;
    }
 
} /* end of MP_AcquireProc */


/*************************************************************************
 * Function: MP_Shutdown
 ************************************************************************/
void MP_Shutdown ()
{
  munmap(arena,sysconf(_SC_PAGESIZE));
} /* end of MP_Shutdown */


/*************************************************************************
 * Function: MP_MaxProcs
 ************************************************************************/
int MP_MaxProcs ()
{
    return MAX_NUM_PROCS;

} /* end of MP_MaxProcs */

/*************************************************************************
 * Function: MP_ProcId
 ************************************************************************/
mp_pid_t MP_ProcId ()
{

  return (thr_self());

} /* end of MP_ProcId */

/*************************************************************************
 * Function: MP_ActiveProcs
 ************************************************************************/
int MP_ActiveProcs ()
{
    int ap;

    MP_SetLock(MP_ProcLock);
    ap = INT_LIB7toC(DEREF(ActiveProcs));
    MP_UnsetLock(MP_ProcLock);

    return ap;

} /* end of MP_ActiveProcs */


/* EndSourceFile */

