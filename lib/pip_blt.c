/*
 * $RIKEN_copyright: 2018 Riken Center for Computational Sceience,
 * 	  System Software Devlopment Team. All rights researved$
 * $PIP_VERSION: Version 1.0$
 * $PIP_license: <Simplified BSD License>
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the PiP project.$
 */
/*
 * Written by Atsushi HORI <ahori@riken.jp>
 */

#include <pip_internal.h>
#include <pip_gdbif_func.h>

#ifdef DEBUG
#define QUEUE_DUMP(taski,queue)			\
  do {						\
    char msg[256];				\
    char *p = msg;				\
    int l = sizeof(msg), n;			\
    if( taski != NULL ) {			\
      n = pip_taski_str( p, l, taski );		\
      p += n; l -= n;				\
    }						\
    n = snprintf( p, l, ":" );			\
    p += n; l -= n;				\
    if( PIP_TASKQ_ISEMPTY( queue ) ) {		\
      snprintf( p, l, "  EMPTY" );		\
    } else {					\
      pip_task_t *task;				\
      int iii = 0;				\
      PIP_TASKQ_FOREACH( queue, task ) {	\
	n = snprintf( p, l, " [%d]:", iii++ );	\
	p += n; l -= n;				\
	n = pip_task_str( p, l, task );		\
	p += n; l -= n;				\
      }						\
    }						\
    DBGF( "QDUMP %s: %s", #queue, msg );	\
  } while( 0 )

#else  /* DEBUG */
#define QUEUE_DUMP( taski, queue )
#endif	/* DEBUG */

static int
pip_able_to_terminate_now( pip_task_internal_t *taski ) {
  DBGF( "[PIPID:%d] flag_exit:%d  refcount:%d",
	taski->pipid, taski->flag_exit, (int) taski->refcount );
  return taski->flag_exit && taski->refcount == 0;
}

void pip_terminate_task( pip_task_internal_t* )
 __attribute__((noreturn));
void pip_terminate_task( pip_task_internal_t *self ) {
  DBGF( "PIPID:%d  tid:%d", self->pipid, self->annex->tid );
  ASSERTD( (pid_t) self->annex->tid != pip_gettid() );

  /* call fflush() in the target context to flush out std* messages */
  if( self->annex->symbols.libc_fflush != NULL ) {
    self->annex->symbols.libc_fflush( NULL );
  }
  if( self->annex->symbols.named_export_fin != NULL ) {
    self->annex->symbols.named_export_fin( self );
  }
  pip_gdbif_exit( self, WEXITSTATUS(self->annex->status) );
  pip_gdbif_hook_after( self );

  DBGF( "PIPID:%d -- FORCE EXIT", self->pipid );
  if( pip_is_threaded_() ) {	/* thread mode */
    self->annex->flag_sigchld = 1;
    /* simulate process behavior */
    ASSERT( pip_raise_signal( pip_root->task_root, SIGCHLD ) );
    DBGF( "calling pthread_exit()" );
    pthread_exit( NULL );
  } else {			/* process mode */
    exit( WEXITSTATUS(self->annex->status) );
  }
  NEVER_REACH_HERE;
}

void pip_wakeup( pip_task_internal_t *taski ) {
  uint32_t 	opts = taski->annex->opts_sync;

  ENTERF( "-- WAKEUP PIPID:%d (PID:%d) vvvvvvvvvv",
	  taski->pipid, taski->annex->tid );
  if( taski->flag_wakeup ) RETURNV;

  DBGF( "schedq_len:%d  oodq_len:%d  RFC:%d  flag_exit:%d",
	taski->schedq_len, (int) taski->oodq_len,
	(int) taski->refcount, taski->flag_exit );

  pip_memory_barrier();
  taski->flag_wakeup = 1;
  DBGF( "sync_opts: 0x%x", opts );
  switch( opts ) {
  case PIP_SYNC_BUSYWAIT:
  case PIP_SYNC_YIELD:
    DBGF( "PIPID:%d -- WAKEUP (busywait or yield)", taski->pipid );
    break;
  case PIP_SYNC_BLOCKING:
  case PIP_SYNC_AUTO:
  default:
    pip_sem_post( &taski->annex->sleep );
    break;
  }
  RETURNV;
}

static void pip_change_sched( pip_task_internal_t *taski,
			      pip_task_internal_t *sched_new ) {
  pip_task_internal_t *sched_curr  __attribute__ ((unused));
  sched_curr = taski->task_sched;
  taski->task_sched = sched_new;
  DBGF( "sched %d --> %d", sched_curr->pipid, sched_new->pipid );
}

static int pip_sched_ood_task( pip_task_internal_t *schedi,
			       pip_task_internal_t *taski ) {
  pip_task_annex_t	*annex = schedi->annex;
  int			flag;

  ENTERF( "taski:PIPID:%d  sched:PIPID:%d", taski->pipid, schedi->pipid );

  pip_spin_lock( &annex->lock_oodq );
  PIP_TASKQ_ENQ_LAST( &annex->oodq, PIP_TASKQ(taski) );
  flag = ( schedi->oodq_len++ == 0 );
  pip_spin_unlock( &annex->lock_oodq );

  return flag;
}

static int pip_sched_ood_self( pip_task_internal_t *taski ) {
  return pip_sched_ood_task( taski, taski );
}

static int pip_takein_ood_task( pip_task_internal_t *schedi ) {
  pip_task_annex_t	*annex = schedi->annex;

  pip_spin_lock( &annex->lock_oodq );
  if( !PIP_TASKQ_ISEMPTY( &annex->oodq ) ) {
    QUEUE_DUMP( schedi, &annex->oodq );
    PIP_TASKQ_APPEND( &schedi->schedq, &annex->oodq );
    schedi->schedq_len += schedi->oodq_len;
    schedi->oodq_len    = 0;
    QUEUE_DUMP( schedi, &schedi->schedq );
  }
  pip_spin_unlock( &annex->lock_oodq );
  return !PIP_TASKQ_ISEMPTY( &schedi->schedq );
}

static void pip_do_sleep( pip_task_internal_t *taski ) {
  uint32_t 	opts = taski->annex->opts & PIP_SYNC_MASK;
  int		i, j;

  ENTERF( "sync_opts: 0x%x", opts );
  taski->annex->opts_sync = opts;
  pip_deadlock_inc();
  {
    switch( opts & PIP_SYNC_MASK ) {
    case PIP_SYNC_BUSYWAIT:
      DBGF( "PIPID:%d -- SLEEPING (busywait) zzzzzzzz", taski->pipid );
      while( !taski->flag_wakeup ) pip_pause();
      break;
    case PIP_SYNC_YIELD:
      DBGF( "PIPID:%d -- SLEEPING (yield) zzzzzzzz", taski->pipid );
      while( 1 ) {
	for( i=0; i<pip_root->yield_iters; i++ ) {
	  pip_pause();
	  if( taski->flag_wakeup ) goto done;
	}
	pip_system_yield();
      }
      break;
    case 0:
    case PIP_SYNC_AUTO:
    default:
      DBGF( "PIPID:%d -- SLEEPING (AUTO)", taski->pipid );
      for( j=0; j<100; j++ ) {
	pip_system_yield();
	for( i=0; i<pip_root->yield_iters; i++ ) {
	  if( taski->flag_wakeup ) goto done;
	}
      }
      /* fall through */
    case PIP_SYNC_BLOCKING:
      DBGF( "PIPID:%d -- SLEEPING (blocking) zzzzzzz", taski->pipid );
      while( !taski->flag_wakeup ) {
	pip_sem_wait( &taski->annex->sleep );
      }
      break;
    }
  }
 done:
  pip_deadlock_dec();
  taski->flag_wakeup = 0;
  DBGF( "PIPID:%d -- WOKEUP ^^^^^^^^", taski->pipid );
  RETURNV;
}

void pip_sleep( pip_task_internal_t* )
 __attribute__((noreturn));
void pip_sleep( pip_task_internal_t *schedi ) {
  pip_task_internal_t 	*nexti;
  pip_task_t 		*next;

  ENTERF( "PIPID:%d", schedi->pipid );
  while( 1 ) {
    pip_stack_unprotect( schedi );
    /* deffered wakeup */
    if( schedi->annex->task_wakeup ) {
      pip_wakeup( schedi->annex->task_wakeup );
      schedi->annex->task_wakeup = NULL;
    }
    while( 1 ) {
      if( pip_takein_ood_task( schedi ) ) break;
      if( pip_able_to_terminate_now( schedi ) ) {
	DBGF( "PIPID:%d -- WOKEUP to EXIT", schedi->pipid );
	pip_stack_wait( schedi );
	pip_terminate_task( schedi );
	NEVER_REACH_HERE;
      }
      pip_do_sleep( schedi );
    }
    next = PIP_TASKQ_NEXT( &schedi->schedq );
    PIP_TASKQ_DEQ( next );
    schedi->schedq_len --;

    nexti = PIP_TASKI( next );
    DBGF( "PIPID:%d ==>> PIPID:%d", schedi->pipid, nexti->pipid );
    SETCURR( schedi, nexti );
    pip_couple_context( schedi, nexti );
  }
  NEVER_REACH_HERE;
}

static pip_task_internal_t *pip_schedq_next( pip_task_internal_t *schedi ) {
  pip_task_internal_t 	*nexti;
  pip_task_t		*next;

  ASSERTD( schedi == NULL );
  DBGF( "sched-PIPID:%d", schedi->pipid );
  if( pip_takein_ood_task( schedi ) ) {
    next = PIP_TASKQ_NEXT( &schedi->schedq );
    PIP_TASKQ_DEQ( next );
    schedi->schedq_len --;
    ASSERTD( schedi->schedq_len < 0 );
    nexti = PIP_TASKI( next );
    DBGF( "next-PIPID:%d", nexti->pipid );
  } else {
    nexti = NULL;
    DBGF( "next:NULL" );
  }
  return nexti;
}

static void pip_enqueue_task( pip_task_internal_t *taski,
			      pip_task_queue_t *queue,
			      int flag_lock,
			      pip_enqueue_callback_t callback,
			      void *cbarg ) {
  pip_task_t	*task  = PIP_TASKQ( taski );

  if( flag_lock ) {
    pip_task_queue_lock( queue );
    pip_task_queue_enqueue( queue, task );
    pip_task_queue_unlock( queue );
  } else {
    pip_task_queue_enqueue( queue, task );
  }
  if( callback != NULL ) {
    if( callback == PIP_CB_UNLOCK_AFTER_ENQUEUE ) {
      pip_task_queue_unlock( queue );
    } else {
      callback( cbarg );
    }
  }
}

void pip_suspend_and_enqueue_generic( pip_task_internal_t *taski,
				      pip_task_queue_t *queue,
				      int flag_lock,
				      pip_enqueue_callback_t callback,
				      void *cbarg ) {
  pip_task_internal_t	*schedi = taski->task_sched;
  pip_task_internal_t	*nexti  = pip_schedq_next( schedi );

  PIP_SUSPEND( taski );
  ENTERF( "PIPID:%d sched-PIPID:%d", taski->pipid, schedi->pipid );

  pip_atomic_fetch_and_add( &schedi->refcount, 1 );
  DBGF( "PIPID:%d refc:%d", schedi->pipid, (int) schedi->refcount );
  if( nexti != NULL ) {
    /* context-switch to the next task */
    pip_stack_protect( taski, nexti );
    /* before enqueue, the stack must be protected */
    pip_enqueue_task( taski, queue, flag_lock, callback, cbarg );
    SETCURR( schedi, nexti );
    DBGF( "PIPID:%d -->> PIPID:%d", taski->pipid, nexti->pipid );
    pip_swap_context( taski, nexti );

  } else {
    /* if there is no task to schedule next, block scheduling task */
    pip_stack_protect( taski, schedi );
    /* before enqueue, the stack must be protected */
    pip_enqueue_task( taski, queue, flag_lock, callback, cbarg );
    pip_decouple_context( taski, schedi );
  }
  /* taski is resumed eventually */
  RETURNV;
}

static void pip_set_extval( pip_task_internal_t *taski, int extval ) {
  ENTER;
  if( !taski->flag_exit ) {
    taski->flag_exit = PIP_EXITED;
    /* mark myself as exited */
    DBGF( "PIPID:%d/%d extval:%d",
	  taski->pipid, taski->task_sched->pipid, extval );
    if( taski->annex->status == 0 ) {
      taski->annex->status = PIP_W_EXITCODE( extval, 0 );
    }
    pip_gdbif_exit( taski, extval );
    pip_memory_barrier();
    pip_gdbif_hook_after( taski );
  }
  DBGF( "extval: 0x%x(0x%x)", extval, taski->annex->status );
  RETURNV;
}

void pip_do_exit( pip_task_internal_t *taski, int extval ) {
  pip_task_internal_t	*schedi;
  pip_task_t		*queue, *next;
  pip_task_internal_t	*nexti;
  int			rfc __attribute__ ((unused));
  /* this is called when 1) return from main (or func) function */
  /*                     2) pip_exit() is explicitly called     */
  ENTERF( "PIPID:%d", taski->pipid );
  pip_set_extval( taski, extval );

 try_again:
  ASSERT( ( schedi = taski->task_sched ) == NULL );
  queue = &schedi->schedq;
  if( pip_takein_ood_task( schedi ) ) {
    /* taski cannot terminate itself        */
    /* since it has other tasks to schedule */
    next = PIP_TASKQ_NEXT( queue );
    PIP_TASKQ_DEQ( next );
    nexti = PIP_TASKI( next );
    DBGF( "PIPID:%d ==>> PIPID:%d", taski->pipid, nexti->pipid );
    //pip_stack_protect( taski, nexti );
    if( taski != schedi ) {
      /* if taski is NOT a scheduling task,  */
      /* then wakeup taski to terminate and  */
      ASSERT( --schedi->schedq_len < 0 );
      /* deffered wakeup */
      nexti->annex->task_wakeup = taski;
    } else {
      /* if this is a scheduling task, then */
      /* repeat this until the queue becomes emoty */
      /* or RFC becomes zero */
      PIP_TASKQ_ENQ_LAST( queue, PIP_TASKQ(taski) );
    }
    /* schedi schedules the rests */
    SETCURR( schedi, nexti );
    pip_swap_context( taski, nexti );
    DBGF( "TRY-AGAIN (1)" );
    goto try_again;

  } else {			/* queue is empty */
    DBGF( "task PIPID:%d   sched PIPID:%d", taski->pipid, schedi->pipid );
    if( taski != schedi ) {
      /* wakeup taski to terminate */
      /* deffered wakeup */
      schedi->annex->task_wakeup = taski;
      pip_decouple_context( taski, schedi );
      DBGF( "TRY-AGAIN (2)" );
      goto try_again;
    } else if( pip_able_to_terminate_now( taski ) ) {
      pip_terminate_task( taski );
    }
  }
  NEVER_REACH_HERE;
}

static int pip_do_resume( pip_task_internal_t *taski,
			  pip_task_internal_t *resume,
			  pip_task_internal_t *schedi ) {
#ifdef DEBUG
  if( schedi != NULL ) {
    ENTERF( "taski(PIPID:%d)  resume(PIPID:%d)  schedi(PIPID:%d)",
	    taski->pipid, resume->pipid, schedi->pipid );
  } else {
    ENTERF( "taski(PIPID:%d)  resume(PIPID:%d)  resume->schedi(PIPID:%d)",
	    taski->pipid, resume->pipid, resume->task_sched->pipid );
  }
#endif
  if( taski == resume ) RETURN( 0 );
  if( PIP_IS_RUNNING( resume ) ) RETURN( EPERM );
  if( schedi == NULL ) {
    /* resume in the previous scheduling domain */
    schedi = resume->task_sched;
    ASSERTD( schedi == NULL );
  } else {
    if( schedi->flag_exit ) RETURN( EBUSY );
  }

  PIP_RUN( resume );
  int rfc = pip_atomic_sub_and_fetch( &resume->task_sched->refcount, 1 );
  ASSERT( rfc < 0 );
  pip_change_sched( resume, schedi );
  if( taski->task_sched == schedi ) {
    /* same scheduling domain with the current task */
    DBGF( "RUN(self): %d/%d", resume->pipid, schedi->pipid );
    PIP_TASKQ_ENQ_LAST( &schedi->schedq, PIP_TASKQ(resume) );
    schedi->schedq_len ++;
  } else {
    /* schedule the task as an OOD (Out Of (scheduling) Domain) task */
    DBGF( "RUN(OOD): %d/%d", resume->pipid, schedi->pipid );
    if( pip_sched_ood_task( schedi, resume ) ) {
      pip_wakeup( schedi );
    }
  }
  RETURN( 0 );
}

/* API */

void pip_task_queue_brief( pip_task_t *task, char *msg, size_t len ) {
  pip_task_internal_t	*taski = PIP_TASKI(task);
  snprintf( msg, len, "%d/%d(%c)",
	    taski->pipid, taski->task_sched->pipid, taski->state );
}

int pip_get_task_pipid( pip_task_t *task, int *pipidp ) {
  pip_task_internal_t	*taski = pip_task;
  IF_UNLIKELY( taski  == NULL ) RETURN( EPERM  );
  IF_UNLIKELY( task   == NULL ) RETURN( EINVAL );
  IF_LIKELY(   pipidp != NULL ) *pipidp = PIP_TASKI(task)->pipid;
  RETURN( 0 );
}

int pip_get_task_by_pipid( int pipid, pip_task_t **taskp ) {
  int	err;

  IF_LIKELY( ( err = pip_check_pipid( &pipid ) ) == 0 ) {
    if( taskp != NULL ) {
      if( pipid == PIP_PIPID_ROOT ) {
	*taskp = PIP_TASKQ( pip_root->task_root );
      } else {
	*taskp = PIP_TASKQ( &pip_root->tasks[pipid] );
      }
    }
  }
  RETURN( err );
}

int pip_get_sched_domain( pip_task_t **domainp ) {
  pip_task_internal_t	*taski = pip_task;
  IF_UNLIKELY( taski   == NULL ) RETURN( EPERM  );
  IF_UNLIKELY( domainp != NULL ) {
    *domainp = PIP_TASKQ( taski->task_sched );
  }
  RETURN( 0 );
}

int pip_count_runnable_tasks( int *countp ) {
  pip_task_internal_t	*taski = pip_task;
  IF_UNLIKELY( taski  == NULL ) RETURN( EPERM  );
  IF_UNLIKELY( countp == NULL ) RETURN( EINVAL );
  *countp = taski->schedq_len;
  RETURN( 0 );
}

int pip_enqueue_runnable_N( pip_task_queue_t *queue, int *np ) {
  /* runnable tasks in the schedq of the current */
  /* task are enqueued into the queue            */
  pip_task_internal_t	*taski = pip_task;
  pip_task_internal_t 	*schedi;
  pip_task_t	 	*task, *next, *schedq;
  int			c, n;

  ENTER;
  IF_UNLIKELY( taski == NULL ) RETURN( EPERM  );
  IF_UNLIKELY( np    == NULL ) RETURN( EINVAL );

  schedi = taski->task_sched;
  ASSERTD( schedi == NULL );
  schedq = &schedi->schedq;
  n = *np;
  c = 0;
  if( n == PIP_TASK_ALL ) {
    pip_task_queue_lock( queue );
    PIP_TASKQ_FOREACH_SAFE( schedq, task, next ) {
      PIP_TASKQ_DEQ( task );
      PIP_SUSPEND( PIP_TASKI(task) );
      pip_task_queue_enqueue( queue, task );
      c ++;
    }
    pip_task_queue_unlock( queue );
  } else if( n > 0 ) {
    pip_task_queue_lock( queue );
    PIP_TASKQ_FOREACH_SAFE( schedq, task, next ) {
      PIP_TASKQ_DEQ( task );
      PIP_SUSPEND( PIP_TASKI(task) );
      pip_task_queue_enqueue( queue, task );
      if( ++c == n ) break;
    }
    pip_task_queue_unlock( queue );
  } else if( n < 0 ) {
    RETURN( EINVAL );
  }
  schedi->schedq_len -= c;
  ASSERTD( schedi->schedq_len < 0 );
  *np = c;
  return 0;
}

int pip_suspend_and_enqueue_( pip_task_queue_t *queue,
			      pip_enqueue_callback_t callback,
			      void *cbarg ) {
  pip_task_internal_t	*taski = pip_task;
  IF_UNLIKELY( taski == NULL ) RETURN( EPERM );
  IF_UNLIKELY( queue == NULL ) RETURN( EINVAL );
  IF_UNLIKELY( PIP_IS_SUSPENDED( taski ) ) RETURN( EPERM );
  pip_suspend_and_enqueue_generic( taski, queue, 1,
				   callback, cbarg );
  return 0;
}

int pip_suspend_and_enqueue_nolock_( pip_task_queue_t *queue,
				     pip_enqueue_callback_t callback,
				     void *cbarg ) {
  pip_task_internal_t	*taski = pip_task;
  IF_UNLIKELY( taski == NULL ) RETURN( EPERM );
  IF_UNLIKELY( queue == NULL ) RETURN( EINVAL );
  IF_UNLIKELY( PIP_IS_SUSPENDED( taski ) ) RETURN( EPERM );
  pip_suspend_and_enqueue_generic( taski, queue, 0,
				   callback, cbarg );
  return 0;
}

int pip_resume( pip_task_t *resume, pip_task_t *sched ) {
  pip_task_internal_t	*taski = pip_task;
  IF_UNLIKELY( taski == NULL ) RETURN( EPERM );
  return pip_do_resume( taski, PIP_TASKI(resume), PIP_TASKI(sched) );
}

int pip_dequeue_and_resume_( pip_task_queue_t *queue, pip_task_t *sched ) {
  pip_task_internal_t	*taski = pip_task;
  pip_task_t *resume;

  ENTER;
  IF_UNLIKELY( taski == NULL ) RETURN( EPERM );
  pip_task_queue_lock( queue );
  resume = pip_task_queue_dequeue( queue );
  pip_task_queue_unlock( queue );
  IF_UNLIKELY( resume == NULL ) RETURN( ENOENT );
  RETURN( pip_do_resume( taski, PIP_TASKI(resume), PIP_TASKI(sched) ) );
}

int pip_dequeue_and_resume_nolock_( pip_task_queue_t *queue,
				    pip_task_t *sched ) {
  ENTER;
  pip_task_t *resume;
  resume = pip_task_queue_dequeue( queue );
  if( resume == NULL ) RETURN( ENOENT );
  RETURN( pip_do_resume( pip_task, PIP_TASKI(resume), PIP_TASKI(sched) ) );
}

int pip_dequeue_and_resume_multiple( pip_task_internal_t *taski,
				     pip_task_queue_t *queue,
				     pip_task_internal_t *sched,
				     int *np ) {
  pip_task_t	 	tmpq, *resume, *next;
  int			n, c = 0, err = 0;

  ENTER;
  IF_UNLIKELY( taski == NULL ) RETURN( EPERM  );
  IF_UNLIKELY( queue == NULL ) RETURN( EINVAL );
  IF_UNLIKELY( np    == NULL ) RETURN( EINVAL );
  n = *np;

  PIP_TASKQ_INIT( &tmpq );
  if( n == PIP_TASK_ALL ) {
    DBGF( "dequeue_and_resume_N: ALL" );
    pip_task_queue_lock( queue );
    while( ( resume = pip_task_queue_dequeue( queue ) ) != NULL ) {
      PIP_TASKQ_ENQ_LAST( &tmpq, resume );
      c ++;
    }
    pip_task_queue_unlock( queue );

  } else if( n > 0 ) {
    DBGF( "N: %d", n );
    pip_task_queue_lock( queue );
    while( c < n ) {
      resume = pip_task_queue_dequeue( queue );
      if( resume == NULL ) break;
      PIP_TASKQ_ENQ_LAST( &tmpq, resume );
      c ++;
    }
    pip_task_queue_unlock( queue );

  } else if( n < 0 ) {
    RETURN( EINVAL );
  }
  PIP_TASKQ_FOREACH_SAFE( &tmpq, resume, next ) {
    PIP_TASKQ_DEQ( resume );
    err = pip_do_resume( taski, PIP_TASKI(resume), sched );
    if( err ) break;
  }
  if( !err ) *np = c;
  RETURN( err );
}

int pip_dequeue_and_resume_N_( pip_task_queue_t *queue,
			       pip_task_t *sched,
			       int *np ) {
  return pip_dequeue_and_resume_multiple( pip_task,
					  queue,
					  PIP_TASKI(sched),
					  np );
}

int pip_dequeue_and_resume_N_nolock_( pip_task_queue_t *queue,
				      pip_task_t *sched,
				      int *np ) {
  pip_task_internal_t	*taski = pip_task;
  pip_task_t 		tmpq, *resume, *next;
  int			n, c = 0, err = 0;

  ENTER;
  IF_UNLIKELY( taski == NULL ) RETURN( EPERM  );
  IF_UNLIKELY( queue == NULL ) RETURN( EINVAL );
  IF_UNLIKELY( np    == NULL ) RETURN( EINVAL );
  n = *np;

  PIP_TASKQ_INIT( &tmpq );
  if( n == PIP_TASK_ALL ) {
    DBGF( "dequeue_and_resume_N: ALL" );
    while( ( resume = pip_task_queue_dequeue( queue ) ) != NULL ) {
      PIP_TASKQ_ENQ_LAST( &tmpq, resume );
      c ++;
    }
  } else if( n > 0 ) {
    DBGF( "N: %d", n );
    while( c < n ) {
      resume = pip_task_queue_dequeue( queue );
      if( resume == NULL ) break;
      PIP_TASKQ_ENQ_LAST( &tmpq, resume );
      c ++;
    }
  } else if( n < 0 ) {
    RETURN( EINVAL );
  }
  PIP_TASKQ_FOREACH_SAFE( &tmpq, resume, next ) {
    PIP_TASKQ_DEQ( resume );
    err = pip_do_resume( taski, PIP_TASKI(resume), PIP_TASKI(sched) );
    if( err ) break;
  }
  if( !err ) *np = c;
  RETURN( err );
}

pip_task_t *pip_task_self( void ) { return PIP_TASKQ(pip_task); }

int pip_yield( int flag ) {
  pip_task_t		*queue, *next;
  pip_task_internal_t	*taski = pip_task;
  pip_task_internal_t	*nexti, *schedi = taski->task_sched;

  ENTER;
  IF_UNLIKELY( taski == NULL ) RETURN( EPERM );
  IF_UNLIKELY( flag &  PIP_YIELD_SYSTEM ) {
    pip_system_yield();
    RETURN( 0 );
  }
  IF_UNLIKELY( flag == PIP_YIELD_DEFAULT ) {
    pip_system_yield();
  }
  IF_UNLIKELY( schedi->oodq_len > 0 ) {	/* fast check */
    (void) pip_takein_ood_task( schedi );
  }
  queue = &schedi->schedq;
  IF_UNLIKELY( PIP_TASKQ_ISEMPTY( queue ) ) RETURN( 0 );

  PIP_TASKQ_ENQ_LAST( queue, PIP_TASKQ(taski) );
  next = PIP_TASKQ_NEXT( queue );
  PIP_TASKQ_DEQ( next );
  nexti = PIP_TASKI( next );
  ASSERTD( taski == nexti );
  SETCURR( schedi, nexti );
  DBGF( "next-PIPID: %d", nexti->pipid );
  pip_swap_context( taski, nexti );

  RETURN( EINTR );
}

int pip_yield_to( pip_task_t *target ) {
  pip_task_internal_t	*targeti = PIP_TASKI( target );
  pip_task_internal_t	*schedi, *taski = pip_task;
  pip_task_t		*queue;

  IF_UNLIKELY( targeti == NULL  ) {
    RETURN( pip_yield( PIP_YIELD_DEFAULT ) );
  }
  IF_UNLIKELY( taski   == NULL  ) RETURN( EPERM );
  IF_UNLIKELY( targeti == taski ) RETURN( 0 );
  schedi = taski->task_sched;
  /* the target task must be in the same scheduling domain */
  IF_UNLIKELY( targeti->task_sched != schedi ) RETURN( EPERM );

  IF_UNLIKELY( schedi->oodq_len > 0 ) {	/* fast check */
    (void) pip_takein_ood_task( schedi );
  }
  queue = &schedi->schedq;
  PIP_TASKQ_DEQ( target );
  PIP_TASKQ_ENQ_LAST( queue, PIP_TASKQ(taski) );
  SETCURR( schedi, targeti );
  pip_swap_context( taski, targeti );
  RETURN( EINTR );
}

int pip_set_aux( pip_task_t *task, void *aux ) {
  pip_task_internal_t 	*taski;

  IF_UNLIKELY( pip_task == NULL ) RETURN( EPERM  );
  if( task == NULL ) {
    taski = pip_task;
  } else {
    taski = PIP_TASKI( task );
  }
  taski->aux = aux;
  RETURN( 0 );
}

int pip_get_aux( pip_task_t *task, void **auxp ) {
  pip_task_internal_t 	*taski;

  IF_UNLIKELY( pip_task == NULL ) RETURN( EPERM  );
  IF_UNLIKELY( auxp     == NULL ) RETURN( EINVAL );
  if( task == NULL ) {
    taski = pip_task;
  } else {
    taski = PIP_TASKI( task );
  }
  *auxp = taski->aux;
  RETURN( 0 );
}

int pip_couple( void ) {
  pip_task_internal_t 	*taski = pip_task;
  pip_task_internal_t 	*schedi = taski->task_sched;
  pip_task_internal_t 	*nexti;
  int			err = 0;

  ENTER;
  IF_UNLIKELY( taski                == NULL  ) RETURN( EPERM );
  IF_UNLIKELY( schedi               == taski ) RETURN( EBUSY );
  IF_UNLIKELY( taski->coupled_sched != NULL  ) RETURN( EBUSY );

  taski->coupled_sched = schedi;
  pip_change_sched( taski, taski );
  nexti = pip_schedq_next( schedi );
  /* here, abobe pip_schedq_next() must  */
  /* be followed by pip_sched_ood_self() */
  /* otherwise, nexti would be myself !! */
  (void) pip_sched_ood_self( taski );
  if( nexti != NULL ) {
    //pip_stack_protect( taski, nexti );
    /* deffered wakeup */
    nexti->annex->task_wakeup = taski;
    SETCURR( schedi, nexti );
    pip_swap_context( taski, nexti );
  } else {
    //pip_stack_protect( taski, schedi );
    /* deffered wakeup */
    schedi->annex->task_wakeup = taski;
    pip_decouple_context( taski, schedi );
  }
  RETURN( err );
}

int pip_decouple( pip_task_t *sched ) {
  pip_task_internal_t 	*taski   = pip_task;
  pip_task_internal_t  	*schedi  = PIP_TASKI( sched );
  pip_task_internal_t  	*coupled = taski->coupled_sched;
  int			err = 0;

  ENTER;
  IF_UNLIKELY( taski                     == NULL  ) RETURN( EPERM );
  IF_UNLIKELY( taski->task_sched         != taski ) RETURN( EBUSY );
  IF_UNLIKELY( schedi == NULL && coupled == NULL  ) RETURN( EBUSY );

  taski->coupled_sched = NULL;
  if( schedi == NULL ) schedi = coupled;

  pip_change_sched( taski, schedi );
  //pip_stack_protect(  taski, taski);
  if( pip_sched_ood_task( schedi, taski ) ) {
    /* deffered wakeup */
    taski->annex->task_wakeup = schedi;
  }
  pip_decouple_context( taski, taski );
  RETURN( err );
}

int pip_set_syncflag( uint32_t flags ) {
  int err = 0;
  if( pip_check_sync_flag( &flags ) != 0 ) {
    err = EINVAL;
  } else {
    pip_task->annex->opts =
      ( pip_task->annex->opts & ~PIP_SYNC_MASK ) | flags;
  }
  return err;
}
