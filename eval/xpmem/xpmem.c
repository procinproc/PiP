/*
 * $RIKEN_copyright:$
 * $PIP_VERSION:$
 * $PIP_license:$
 */
/*
 * Written by Atsushi HORI <ahori@riken.jp>, 2017
 */

#include <xpmem_eval.h>
#include <xpmem.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main( int argc, char **argv ) {
  extern char **environ;
  xpmem_segid_t segid;
  pid_t pid;
  char *nargv[4], segidstr[32];
  void *vaddr;
  uint64_t tm;

#ifdef OVERALL
  if( argc < 2 ) {
    printf( "no iteration\n" );
    exit( 1 );
  }
#else
  int ver = xpmem_version();
  //printf( "XPMEM: %d 0x%x\n", ver, ver );
#endif
  TESTINT( create_region( &vaddr ) );
  tm = rdtscp();
  segid = xpmem_make( vaddr, MMAP_SIZE, XPMEM_PERMIT_MODE, (void*)0666 );
#ifndef OVERALL
  tm = rdtscp() - tm;
  printf( "xpmem_make(): %lu\n", tm );
#endif
  if( segid == -1 ) {
    printf( "xpmem_make(): errno=%d\n", errno );
  } else {
    if( ( pid = fork() ) == 0 ) {
      corebind( 1 );
      sprintf( segidstr, "%lx", segid );
      //printf( "fork(%s)\n", segidstr );

      nargv[0] = "./eval-xpmem";
      nargv[1] = segidstr;
      nargv[2] = argv[1];
      nargv[3] = NULL;

      execve( nargv[0], nargv, environ );
      printf( "????\n" );
    }
    wait( NULL );
  }
  return 0;
}