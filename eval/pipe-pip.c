/*
 * $RIKEN_copyright: Riken Center for Computational Sceience,
 * System Software Development Team, 2016, 2017, 2018, 2019$
 * $PIP_VERSION: Version 1.0.0$
 * $PIP_license$
 */

//#define DEBUG

#include <eval.h>
#include <test.h>

#define NSAMPLES	(10)
#define WITERS		(100)
#define NITERS		(10*1000)

typedef struct exp {
  pip_barrier_t		barrier;
  pip_task_queue_t	queue;
  int			fd;
  volatile int		done;
} exp_t;

#ifdef NTASKS
#undef NTASKS
#endif

static int opts[] = { PIP_SYNC_BUSYWAIT,
		      PIP_SYNC_BLOCKING,
		      0 };

static char *optstr[] = { "PIP_SYNC_BUSYWAIT",
			  "PIP_SYNC_BLOCKING",
			  "" };

#define NTASKS		(sizeof(opts)/sizeof(int))

#define BUFSZ	(1024*1024)
char buffer[BUFSZ];

static int feed_pipe( int fd, size_t sz ) {
  return write( fd, buffer, sz );
}

static void drain_pipe( int fd ) {
  bind_core( NTASKS );
  while( read( fd, buffer, BUFSZ ) > 0 );
}

int main( int argc, char **argv ) {
  pip_spawn_program_t	prog;
  exp_t	exprt, *expp;
  int 	ntasks, pipid;
  int	witers = WITERS, niters = NITERS;
  int	i, j, status, extval = 0;
  int	pp[2];
  double nd = (double) niters;
  pid_t	pid;

  ntasks = NTASKS;

  pip_spawn_from_main( &prog, argv[0], argv, NULL );
  expp = &exprt;

  CHECK( pip_init(&pipid,&ntasks,(void*)&expp,PIP_MODE_PROCESS), RV,
	 return(EXIT_FAIL) );
  if( pipid == PIP_PIPID_ROOT ) {
    CHECK( pipe(pp),         RV,   return(EXIT_FAIL) );
    CHECK( ( pid = fork() ), RV<0, return(EXIT_FAIL) );
    if( pid == 0 ) {
      close( pp[1] );
      drain_pipe( pp[0] );
      exit( 0 );
    }
    close( pp[0] );
    expp->fd = pp[1];

    CHECK( pip_barrier_init(&expp->barrier ,ntasks), RV, return(EXIT_FAIL) );
    CHECK( pip_task_queue_init(&expp->queue ,NULL),  RV, return(EXIT_FAIL) );
    expp->done = 0;

    for( i=0; i<ntasks; i++ ) {
      pipid = i;
      CHECK( pip_blt_spawn( &prog, i, 0, &pipid, NULL, NULL, NULL ),
	     RV,
	     abend(EXIT_UNTESTED) );
    }
    for( i=0; i<ntasks; i++ ) {
      CHECK( pip_wait(i,&status), RV, return(EXIT_FAIL) );
      if( WIFEXITED( status ) ) {
	CHECK( ( extval = WEXITSTATUS( status ) ),
	       RV,
	       return(EXIT_FAIL) );
      } else {
	CHECK( "Task is signaled", RV, return(EXIT_UNRESOLVED) );
      }
    }
    close( pp[1] );
    wait( NULL );

  } else {
    double 	t, t0[NSAMPLES];
    double	min0 = t0[0];
    int 	idx0 = 0;
    size_t	sz;

    CHECK( pip_barrier_wait(&expp->barrier),                RV, return(EXIT_FAIL) );

    if( pipid < ntasks - 1 ) {
      CHECK( pip_suspend_and_enqueue(&expp->queue,NULL,NULL), RV, return(EXIT_FAIL) );
      CHECK( pip_set_syncflag( opts[pipid] ),                 RV, return(EXIT_FAIL) );

      for( sz=4096; sz<BUFSZ; sz*=2 ) {
	printf( "SYNC_OPT: %s  SZ:%lu\n", optstr[pipid], sz );
	fflush( NULL );

	for( j=0; j<NSAMPLES; j++ ) {
	  for( i=0; i<witers; i++ ) {
	    system ("mount -t tmpfs -o size=64m /dev/shm /tmpfs" );
	    t0[j]  = 0.0;
	    pip_gettime();
	    memset( buffer, 123, sz );
	    CHECK( pip_couple(),                   RV,   return(EXIT_FAIL) );
	    feed_pipe( expp->fd, sz );
	    CHECK( pip_decouple(NULL),             RV,   return(EXIT_FAIL) );
	    system ("umount /tmpfs" );
	  }
	  for( i=0; i<niters; i++ ) {
	    system ("mount -t tmpfs -o size=64m /dev/shm /tmpfs" );
	    t = pip_gettime();
	    memset( buffer, 123, sz );
	    CHECK( pip_couple(),                   RV,   return(EXIT_FAIL) );
	    feed_pipe( expp->fd, sz );
	    CHECK( pip_decouple(NULL),             RV,   return(EXIT_FAIL) );
	    t0[j] += pip_gettime() - t;
	    system ("umount /tmpfs" );
	  }
	  t0[j] /= nd;
	}
	min0 = t0[0];
	idx0 = 0;
	for( j=0; j<NSAMPLES; j++ ) {
	  printf( "[%d] pip:pipe : %g\n", j, t0[j] );
	  if( min0 > t0[j] ) {
	    min0 = t0[j];
	    idx0 = j;
	  }
	}
	printf( "[[%d]] %lu pip:pipe : %.3g\n", idx0, sz, t0[idx0] );
	fflush( NULL );
      }
      expp->done ++;


    } else {
      pip_task_t *task;
      int ql;

      CHECK( pip_get_task_from_pipid(PIP_PIPID_MYSELF,&task), RV, return(EXIT_FAIL) );
      while( 1 ) {
	CHECK( pip_task_queue_count(&expp->queue,&ql),        RV, return(EXIT_FAIL) );
	if( ql == ntasks - 1 ) break;
	pip_yield( PIP_YIELD_DEFAULT );
      }
      for( i=0; i<ntasks-1; i++ ) {
	CHECK( pip_dequeue_and_resume(&expp->queue,task),     RV, return(EXIT_FAIL) );
	do {
	  pip_yield( PIP_YIELD_DEFAULT );
	} while( expp->done == i );
      }
    }
  }
  CHECK( pip_fin(), RV, return(EXIT_FAIL) );
  return 0;
}
