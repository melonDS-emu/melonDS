/*
 *                       s e m _ t i m e d w a i t
 *
 *  Function:
 *     Implements a version of sem_timedwait().
 *
 *  Description:
 *     Not all systems implement sem_timedwait(), which is a version of
 *     sem_wait() with a timeout. Mac OS X is one example, at least up to
 *     and including version 10.6 (Leopard). If such a function is needed,
 *     this code provides a reasonable implementation, which I think is
 *     compatible with the standard version, although possibly less
 *     efficient. It works by creating a thread that interrupts a normal
 *     sem_wait() call after the specified timeout.
 *
 *  Call:
 *
 *     The Linux man pages say:
 *
 *     #include <semaphore.h>
 *
 *     int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
 *
 *     sem_timedwait() is the same as sem_wait(), except that abs_timeout 
 *     specifies a limit on the amount of time that the call should block if 
 *     the decrement cannot be immediately performed. The abs_timeout argument
 *     points to a structure that specifies an absolute timeout in seconds and
 *     nanoseconds since the Epoch (00:00:00, 1 January 1970). This structure 
 *     is defined as follows:
 *
 *     struct timespec {
 *        time_t tv_sec;      Seconds
 *        long   tv_nsec;     Nanoseconds [0 .. 999999999]
 *     };
 *
 *     If the timeout has already expired by the time of the call, and the 
 *     semaphore could not be locked immediately, then sem_timedwait() fails 
 *     with a timeout error (errno set to ETIMEDOUT).
 *     If the operation can be performed immediately, then sem_timedwait() 
 *     never fails with a timeout error, regardless of the value of abs_timeout.
 *     Furthermore, the validity of abs_timeout is not checked in this case.
 *
 *  Limitations:
 *
 *     The mechanism used involves sending a SIGUSR2 signal to the thread
 *     calling sem_timedwait(). The handler for this signal is set to a null
 *     routine which does nothing, and with any flags for the signal 
 *     (eg SA_RESTART) cleared. Note that this effective disabling of the
 *     SIGUSR2 signal is a side-effect of using this routine, and means it
 *     may not be a completely transparent plug-in replacement for a
 *     'normal' sig_timedwait() call. Since OS X does not declare the
 *     sem_timedwait() call in its standard include files, the relevant 
 *     declaration (shown above in the man pages extract) will probably have
 *     to be added to any code that uses this.
 *
 *  Compiling:
 *     This compiles and runs cleanly on OS X (10.6) with gcc with the
 *     -Wall -ansi -pedantic flags. On Linux, using -ansi causes a sweep of
 *     compiler complaints about the timespec structure, but it compiles
 *     and works fine with just -Wall -pedantic. (Since Linux provides
 *     sem_timedwait() anyway, this really isn't needed on Linux.) However,
 *     since Linux provides sem_timedwait anyway, the sem_timedwait()
 *     code in this file is only compiled on OS X, and is a null on other
 *     systems.
 *
 *  Testing:
 *     This file contains a test program that exercises the sem_timedwait
 *     code. It is compiled if the pre-processor variable TEST is defined.
 *     For more details, see the comments for the test routine at the end
 *     of the file.
 *
 *  Author: Keith Shortridge, AAO.
 *
 *  History:
 *      8th Sep 2009. Original version. KS.
 *     24th Sep 2009. Added test that the calling thread still exists before
 *                    trying to set the timed-out flag. KS.
 *      2nd Oct 2009. No longer restores the original SIGUSR2 signal handler.
 *                    See comments in the body of the code for more details.
 *                    Prototypes for now discontinued internal routines removed.
 *     12th Aug 2010. Added the cleanup handler, so that this code no longer
 *                    leaks resources if the calling thread is cancelled. KS.
 *     21st Sep 2011. Added copyright notice below. Modified header comments
 *                    to describe the use of SIGUSR2 more accurately in the 
 *                    light of the 2/10/09 change above. Now undefs DEBUG
 *                    before defining it, to avoid any possible clash. KS.
 *     14th Feb 2012. Tidied out a number of TABs that had got into the
 *                    code. KS.
 *      6th May 2013. Copyright notice modified to one based on the MIT licence,
 *                    which is more permissive than the previous notice. KS.
 *
 *  Copyright (c) Australian Astronomical Observatory (AAO), (2013).
 *  Permission is hereby granted, free of charge, to any person obtaining a 
 *  copy of this software and associated documentation files (the "Software"), 
 *  to deal in the Software without restriction, including without limitation 
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 *  and/or sell copies of the Software, and to permit persons to whom the 
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in 
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

#ifdef __APPLE__

#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <setjmp.h>

#include "sem_timedwait.h"

/*  Some useful definitions - TRUE, FALSE, and DEBUG */

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0
#undef DEBUG
#define DEBUG printf

/*  A structure of type timeoutDetails is passed to the thread used to 
 *  implement the timeout.
 */

typedef struct {
   struct timespec delay;            /* Specifies the delay, relative to now */
   pthread_t callingThread;          /* The thread doing the sem_wait call */
   volatile short *timedOutShort;    /* Address of a flag set to indicate that
                                      * the timeout was triggered. */
} timeoutDetails;

/*  A structure of type cleanupDetails is passed to the thread cleanup 
 *  routine which is called at the end of the routine or if the thread calling
 *  it is cancelled.
 */
 
typedef struct {
   pthread_t *threadIdAddr;          /* Address of the variable that holds 
                                      * the Id of the timeout thread. */
   struct sigaction *sigHandlerAddr; /* Address of the old signal action
                                      * handler. */
   volatile short *timedOutShort;    /* Address of a flag set to indicate that
                                      * the timeout was triggered. */
} cleanupDetails;

/*  Forward declarations of internal routines */

static void* timeoutThreadMain (void* passedPtr);
static int triggerSignal (int Signal, pthread_t Thread);
static void ignoreSignal (int Signal);
static void timeoutThreadCleanup (void* passedPtr);

/* -------------------------------------------------------------------------- */
/*
 *                      s e m _ t i m e d w a i t
 *
 *  This is the main code for the sem_timedwait() implementation.
 */

int sem_timedwait (
   sem_t *sem,
   const struct timespec *abs_timeout)
{
   int result = 0;                   /* Code returned by this routine 0 or -1 */
   
   /*  "Under no circumstances shall the function fail if the semaphore
    *  can be locked immediately". So we try to get it quickly to see if we
    *  can avoid all the timeout overheads.
    */
   
   if (sem_trywait(sem) == 0) {
      
      /*  Yes, got it immediately. */
      
      result = 0;
      
   } else {
      
      /*  No, we've got to do it with a sem_wait() call and a thread to run
       *  the timeout. First, work out the time from now to the specified
       *  timeout, which we will pass to the timeout thread in a way that can
       *  be used to pass to nanosleep(). So we need this in seconds and
       *  nanoseconds. Along the way, we check for an invalid passed time,
       *  and for one that's already expired.
       */
      
      if ((abs_timeout->tv_nsec < 0) || (abs_timeout->tv_nsec > 1000000000)) {
         
         /* Passed time is invalid */
         
         result = -1;
         errno = EINVAL;
         
      } else {
         
         struct timeval currentTime;                              /* Time now */
         long secsToWait,nsecsToWait;            /* Seconds and nsec to delay */
         gettimeofday (&currentTime,NULL);
         secsToWait = abs_timeout->tv_sec - currentTime.tv_sec;
         nsecsToWait = (abs_timeout->tv_nsec - (currentTime.tv_usec * 1000));
         while (nsecsToWait < 0) {
            nsecsToWait += 1000000000;
            secsToWait--;
         }
         if ((secsToWait < 0) || ((secsToWait == 0) && (nsecsToWait < 0))) {
            
            /*  Time has passed. Report an immediate timeout. */
            
            result = -1;
            errno = ETIMEDOUT;
            
         } else {
            
            /*  We're going to have to do a sem_wait() with a timeout thread.
             *  The thread will wait the specified time, then will issue a
             *  SIGUSR2 signal that will interrupt the sem_wait() call. 
             *  We pass the thread the id of the current thread, the delay,
             *  and the address of a flag to set on a timeout, so we can 
             *  distinguish an interrupt caused by the timeout thread from
             *  one caused by some other signal.
             */
         
            volatile short timedOut;                /* Flag to set on timeout */
            timeoutDetails details;     /* All the stuff the thread must know */
            struct sigaction oldSignalAction;       /* Current signal setting */
            pthread_t timeoutThread;                  /* Id of timeout thread */
            cleanupDetails cleaningDetails; /* What the cleanup routine needs */
            int oldCancelState;                /* Previous cancellation state */
            int ignoreCancelState;               /* Used in call, but ignored */
            int createStatus;              /* Status of pthread_create() call */
            
            /*  If the current thread is cancelled (and CML does do this)
             *  we don't want to leave our timer thread running - if we've
             *  started the thread we want to make sure we join it in order
             *  to release its resources. So we set a cleanup handler to
             *  do this. We pass it the address of the structure that will
             *  hold all it needs to know. While we set all this up,
             *  we prevent ourselves being cancelled, so all this data is
             *  coherent.
             */
            
            pthread_setcancelstate (PTHREAD_CANCEL_DISABLE,&oldCancelState);
            timeoutThread = (pthread_t) 0;
            cleaningDetails.timedOutShort = &timedOut;
            cleaningDetails.threadIdAddr = &timeoutThread;
            cleaningDetails.sigHandlerAddr = &oldSignalAction;
            pthread_cleanup_push (timeoutThreadCleanup,&cleaningDetails);
            
            /*  Set up the details for the thread. Clear the timeout flag,
             *  record the current SIGUSR2 action settings so we can restore
             *  them later.
             */
            
            details.delay.tv_sec = secsToWait;
            details.delay.tv_nsec = nsecsToWait;
            details.callingThread = pthread_self();
            details.timedOutShort = &timedOut;
            timedOut = FALSE;
            sigaction (SIGUSR2,NULL,&oldSignalAction);
            
            /*  Start up the timeout thread. Once we've done that, we can
             *  restore the previous cancellation state.
             */
            
            createStatus = pthread_create(&timeoutThread,NULL,
                                         timeoutThreadMain, (void*)&details);
            pthread_setcancelstate (oldCancelState,&ignoreCancelState);
            
            if (createStatus < 0) {
               
               /* Failed to create thread. errno will already be set properly */
               
               result = -1;
               
            } else {
               
               /*  Thread created OK. This is where we wait for the semaphore.
                */
               
               if (sem_wait(sem) == 0) {
                  
                  /*  Got the semaphore OK. We return zero, and all's well. */
                  
                  result = 0;
                  
               } else {
                  
                  /*  If we got a -1 error from sem_wait(), it may be because
                   *  it was interrupted by a timeout, or failed for some
                   *  other reason. We check for the expected timeout 
                   *  condition, which is an 'interrupted' status and the
                   *  timeout flag set by the timeout thread. We report that as
                   *  a timeout error. Anything else is some other error and
                   *  errno is already set properly.
                   */
                  
                  result = -1;
                  if (errno == EINTR) {
                     if (timedOut) errno = ETIMEDOUT;
                  }
               }
               
            }
            
            /*  The cleanup routine - timeoutThreadCleanup() - packages up
             *  any tidying up that is needed, including joining with the
             *  timer thread. This will be called if the current thread is
             *  cancelled, but we need it to happen anyway, so we set the
             *  execute flag true here as we remove it from the list of
             *  cleanup routines to be called. So normally, this line amounts
             *  to calling timeoutThreadCleanup().
             */
             
            pthread_cleanup_pop (TRUE);
         }
      }
   }
   return (result);
}

/* -------------------------------------------------------------------------- */
/*
 *                  t i m e o u t  T h r e a d  C l e a n u p
 *
 *  This internal routine tidies up at the end of a sem_timedwait() call.
 *  It is set as a cleanup routine for the current thread (not the timer
 *  thread) so it is executed even if the thread is cancelled. This is
 *  important, as we need to tidy up the timeout thread. If we took the
 *  semaphore (in other words, if we didn't timeout) then the timer thread
 *  will still be running, sitting in its nanosleep() call, and we need
 *  to cancel it. If the timer thread did signal a timeout then it will
 *  now be closing down. In either case, we need to join it (using a call
 *  to pthread_join()) or its resources will never be released.
 *  The single argument is a pointer to a cleanupDetails structure that has
 *  all the routine needs to know.
 */
   
static void timeoutThreadCleanup (void* passedPtr)
{
   /*  Get what we need from the structure we've been passed. */
   
   cleanupDetails *detailsPtr = (cleanupDetails*) passedPtr;
   short timedOut = *(detailsPtr->timedOutShort);
   pthread_t timeoutThread = *(detailsPtr->threadIdAddr);
   
   /*  If we created the thread, stop it - doesn't matter if it's no longer
    *  running, pthread_cancel can handle that. We make sure we wait for it 
    *  to complete, because it is this pthread_join() call that releases any 
    *  memory the thread may have allocated. Note that cancelling a thread is 
    *  generally not a good idea, because of the difficulty of cleaning up 
    *  after it, but this is a very simple thread that does nothing but call 
    *  nanosleep(), and that we can cancel quite happily.
    */
      
   if (!timedOut) pthread_cancel(timeoutThread);
   pthread_join(timeoutThread,NULL);
   
   /*  The code originally restored the old action handler, which generally 
    *  was the default handler that caused the task to exit. Just occasionally,
    *  there seem to be cases where the signal is still queued and ready to 
    *  trigger even though the thread that presumably sent it off just before
    *  it was cancelled has finished. I had thought that once we'd joined
    *  that thread, we could be sure of not seeing the signal, but that seems 
    *  not to be the case, and so restoring a handler that will allow the task
    *  to crash is not a good idea, and so the line below has been commented
    *  out.
    *
    *  sigaction (SIGUSR2,detailsPtr->sigHandlerAddr,NULL);
    */
}

/* -------------------------------------------------------------------------- */
/*
 *                  t i m e o u t  T h r e a d  M a i n
 *
 *  This internal routine is the main code for the timeout thread.
 *  The single argument is a pointer to a timeoutDetails structure that has
 *  all the thread needs to know - thread to signal, delay time, and the
 *  address of a flag to set if it triggers a timeout.
 */
   
static void* timeoutThreadMain (void* passedPtr)
{
   void* Return = (void*) 0;
   
   /*  We grab all the data held in the calling thread right now. In some
    *  cases, we find that the calling thread has vanished and released
    *  its memory, including the details structure, by the time the timeout
    *  expires, and then we get an access violation when we try to set the
    *  'timed out' flag.
    */
   
   timeoutDetails details = *((timeoutDetails*) passedPtr);
   struct timespec requestedDelay = details.delay;
   
   /*  We do a nanosleep() for the specified delay, and then trigger a
    *  timeout. Note that we allow for the case where the nanosleep() is
    *  interrupted, and restart it for the remaining time. If the 
    *  thread that is doing the sem_wait() call gets the semaphore, it
    *  will cancel this thread, which is fine as we aren't doing anything
    *  other than a sleep and a signal.
    */
   
   for (;;) {
      struct timespec remainingDelay;
      if (nanosleep (&requestedDelay,&remainingDelay) == 0) {
         break;
      } else if (errno == EINTR) {
         requestedDelay = remainingDelay;
      } else {
         Return = (void*) errno;
         break;
      }
   }
   
   /*  We've completed the delay without being cancelled, so we now trigger
    *  the timeout by sending a signal to the calling thread. And that's it,
    *  although we set the timeout flag first to indicate that it was us
    *  that interrupted the sem_wait() call. One precaution: before we
    *  try to set the timed-out flag, make sure the calling thread still
    *  exists - this may not be the case if things are closing down a bit
    *  messily. We check this quickly using a zero test signal.
    */
   
   if (pthread_kill(details.callingThread,0) == 0) {
      *(details.timedOutShort) = TRUE;
      if (triggerSignal (SIGUSR2,details.callingThread) < 0) {
         Return = (void*) errno;
      }
   }
   
   return Return;
}
   
/* -------------------------------------------------------------------------- */
/*
 *                    t r i g g e r  S i g n a l
 *
 *  This is a general purpose routine that sends a specified signal to
 *  a specified thread, setting up a signal handler that does nothing,
 *  and then giving the signal. The only effect will be to interrupt any
 *  operation that is currently blocking - in this case, we expect this to
 *  be a sem_wait() call.
 */
   
static int triggerSignal (int Signal, pthread_t Thread)
{
   int Result = 0;
   struct sigaction SignalDetails;
   SignalDetails.sa_handler = ignoreSignal;
   SignalDetails.sa_flags = 0;
   (void) sigemptyset(&SignalDetails.sa_mask);
   if ((Result = sigaction(Signal,&SignalDetails,NULL)) == 0) {
      Result = pthread_kill(Thread,Signal);
   }
   return Result;
}
   
/* -------------------------------------------------------------------------- */
/*
 *                     i g n o r e  S i g n a l
 *
 *  And this is the signal handler that does nothing. (It clears its argument,
 *  but this has no effect and prevents a compiler warning about an unused
 *  argument.)
 */
    
static void ignoreSignal (int Signal) {
   Signal = 0;
} 

#endif
