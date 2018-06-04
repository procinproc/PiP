/*
 * $RIKEN_copyright:$
 * $PIP_VERSION:$
 * $PIP_license:$
 */
/*
 * Written by Atsushi HORI <ahori@riken.jp>, 2016
 */

#define PIP_INTERNAL_FUNCS

//#define DEBUG

#include <test.h>
#include <pip_ulp.h>

#ifdef DEBUG
# ifdef NTASKS
#  undef NTASKS
#  define NTASKS	(10)
# endif
#else
# ifdef NTASKS
#  undef NTASKS
#  define NTASKS	(250)	/* must be less tahn 256 (0xff) */
# endif
#endif

#define NULPS		(NTASKS-10)
//#define NULPS	(10)
//#define NULPS	(3)

int main( int argc, char **argv ) {
  int ntasks, nulps;
  int i, pipid;

  if( argc   > 1 ) {
    ntasks = atoi( argv[1] );
  } else {
    ntasks = NTASKS;
  }
  if( ntasks < 2 ) {
    fprintf( stderr,
	     "Too small number of PiP tasks (must be latrger than 1)\n" );
    exit( 1 );
  }
  if( ntasks >= 256 ) {
    fprintf( stderr,
	     "Too many number of PiP tasks (must be less than 256)\n" );
    exit( 1 );
  }
  nulps = ntasks - 1;

  TESTINT( pip_init( &pipid, NULL, NULL, 0 ) );
  if( pipid == PIP_PIPID_ROOT ) {
    pip_spawn_program_t prog;
    pip_ulp_t ulps;

    PIP_ULP_INIT( &ulps );
    pip_spawn_from_main( &prog, argv[0], argv, NULL );
    for( i=0; i<nulps; i++ ) {
      pipid = i + 1;
      TESTINT( pip_ulp_new( &prog, &pipid, &ulps, NULL ) );
    }
    pipid = 0;
    TESTINT( pip_task_spawn( &prog, PIP_CPUCORE_ASIS, &pipid, NULL, &ulps ));
    for( i=0; i<ntasks; i++ ) {
      int status;
      TESTINT( pip_wait( i, &status ) );
      if( status == i ) {
	fprintf( stderr, "Succeeded (%d)\n", i );
      } else {
	fprintf( stderr, "pip_wait(%d):%d\n", i, status );
      }
    }
  } else {
    fprintf( stderr, "<%d> Hello !!\n", pipid );
    TESTINT( pip_exit( pipid ) );
  }
  TESTINT( pip_fin() );
  return 0;
}
