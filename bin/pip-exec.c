/*
 * $PIP_license: <Simplified BSD License>
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 * 
 *     Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 * $
 * $RIKEN_copyright: Riken Center for Computational Sceience (R-CCS),
 * System Software Development Team, 2016-2022
 * $
 * $PIP_VERSION: Version 2.4.1$
 *
 * $Author: Atsushi Hori 
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

/** \addtogroup PiP-Commands PiP Commands
 * @{ */
/** \defgroup pip-exec pip-exec
 * @{ 
 * 
 * \brief run program(s) as PiP tasks
 *
 * \synopsis
 * pip-exec [OPTIONS] &lt;program&gt; ... [ : ... ]
 *
 * \description
 * Run a program as PiP task(s).  Mutiple programs can be specified
 * by separating them with colon (:). The \p pip-exec process becomes the PiP root
 * process and the processes derived from the user-specified programs
 * share the same virtual address space with the \p pip-exec process.
 *
 * \param "-n <N>" number of tasks
 * \param "-f <FUNC>" function name to start, defaul 'main'
 * \param "-c <CORE>" specify the CPU core number to bind
 *
 * \sa pipcc
 * \sa pipfc
 *
 * \author Atsushi Hori
 */
/** @} */
/** @} */

#ifndef DOXYGEN_INPROGRESS

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pip/pip.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <ctype.h>

#define COREBIND_RR	(-100)

static char *program;

static void print_usage( void ) {
  fprintf( stderr,
	   "Usage: %s [-n N] [-c C] [-f F] A.OUT ... "
	   "{ : [-n N] [-c C] [-f F] B.OUT ... }\n",
	   program );
  fprintf( stderr, "\t-n N\t: Number of PiP tasks (default is 1)\n" );
  fprintf( stderr, "\t-c C\t: CPU core binding pattern\n" );
  fprintf( stderr, "\t-f F\t: Function name in the program to start\n" );
  exit( 2 );
}

typedef struct corebind {
  struct corebind	*next;
  int 		n;
  int		s;
  int		r;
} corebind_t;

#ifdef NOT_YET
static int count_cpu( void ) {
  cpu_set_t cpuset;
  int c = -1;

  if( sched_getaffinity( 0, sizeof(cpuset), &cpuset ) == 0 ) {
    c = CPU_COUNT( &cpuset );
  }
  return c;
}

static corebind_t *new_corebind( char **p ) {
  void parse_r( corebind_t *cb, char **q ) {
    int r = 0;
    while( isdigit( **q ) ) {
      r += (**q) - '0';
      (*q) ++;
    }
    if( r == 0 ) print_usage();
    cb->r = r;
  };
  void parse_s( corebind_t *cb, char **q ) {
    int s = 0;
    while( isdigit( **q ) ) {
      s += (**q) - '0';
      (*q) ++;
    }
    cb->s = s;
    if( **q == 'x' ) {
      (*q) ++;
      parse_r( cb, q );
    }
  }
  corebind_t	*cb = (corebind_t*) malloc( sizeof( corebind_t ) );
  if( cb == NULL ) {
    fprintf( stderr, "%s: Not enough memory (corebind)\n", program );
    exit( 9 );
  }
  memset( cb, 0, sizeof( corebind_t ) );

  while( **p != '\0' || **p != ',' ) {
    if( isdigit( **p ) ) {
      int n = 0;
      while( isdigit( **p ) ) {
	n += (**p) - '0';
	(*p) ++;
      }
      cb->n = n;
      if( **p == ':' ) {
	(*p) ++;
	parse_s( cb, p );
      } else if( **p == 'x' ) {
	(*p) ++;
	parse_r( cb, p );
      }
      break;
    } else if( **p == ':' ) {
      (*p) ++;
      parse_s( cb, p );
      break;
    } else if( **p == 'x' ) {
      (*p) ++;
      parse_r( cb, p );
      break;
    } else {
      print_usage();
    }
  }
  if( cb->r == 0 ) cb->r = 1;
  return cb;
}

#ifdef DBGCB
void cb_dump( corebind_t *cb ) {
  int i = 0;
  if( cb != NULL ) {
    printf( "[%d] cb(%p) n:%d s:%d r:%d  (next:%p)\n",
	    i++, cb, cb->n, cb->s, cb->r, cb->next );
    cb_dump( cb->next );
  }
}
#endif
#endif

typedef struct arg {
  struct arg		*next;
  char			*arg;
} arg_t;

static arg_t *new_arg( char *a ) {
  arg_t	*arg = (arg_t*) malloc( sizeof( arg_t ) );
  if( arg == NULL ) {
    fprintf( stderr, "%s: Not enough memory (arg)\n", program );
    exit( 9 );
  }
  memset( arg, 0, sizeof( arg_t ) );
  arg->arg = a;
  return arg;
}

typedef struct spawn {
  struct spawn		*next;
  corebind_t		*cb;
  corebind_t		*cb_tail;
  int			ntasks;
  int			argc;
  char			*func;
  arg_t			*args;
  arg_t			*tail;
} spawn_t;

static int isa_digit_string( const char *str ) {
  const char *p;
  for( p=str; *p!='\0'; p++ ) {
    if( !isdigit( *p ) ) return( 0 );
  }
  return 1;
}

static spawn_t *new_spawn( void ) {
  spawn_t *spawn = (spawn_t*) malloc( sizeof( spawn_t ) );
  if( spawn == NULL ) {
    fprintf( stderr, "%s: Not enough memory (spawn)\n", program );
    exit( 9 );
  }
  memset( spawn, 0, sizeof( spawn_t ) );
  spawn->ntasks = 1;
  return spawn;
}

static void free_arg( arg_t *arg ) {
  if( arg == NULL ) return;
  free_arg( arg->next );
  free( arg );
}

static void free_corebind( corebind_t *cb ) {
  if( cb == NULL ) return;
  free_corebind( cb->next );
  free( cb );
}

static void free_spawn( spawn_t *spawn ) {
  if( spawn == NULL ) return;
  free_spawn( spawn->next );
  free_arg( spawn->args );
  free_corebind( spawn->cb );
  free( spawn );
}

#ifdef NOT_YET
static int nth_core( corebind_t *cb, int start, int *ithp ) {
  int	ith = *ithp;
  int	c = start;

  if( cb == NULL ) return 0;

  printf( "cb->n:%d cb->s:%d cb->r:%d start:%d ith:%d\n",
	  cb->n, cb->s, cb->r, start, ith );
  if( ith == 0 ) {
    c += cb->n;
  } else if( ith > cb->r ) {
    c += cb->n + cb->s * ith;
    *ithp = ith - cb->r;
  } else {
    c += cb->n + cb->s * ith;
    *ithp = 0;
  }
  if( cb->next != NULL ) {
    c = nth_core( cb->next, c, &ith );
  }
  return c;
}
#endif

static int isa_sep( char *str ) {
  return ( strcmp( str, ":"  ) == 0 ||
	   strcmp( str, "::" ) == 0 );
}

int main( int argc, char **argv ) {
  pip_spawn_program_t prog;
  spawn_t	*spawn, *head, *tail;
  arg_t		*arg;
  char		**nargv = NULL;
  int ntasks, nt_start;;
#ifdef NOT_YET
  int ncores = count_cpu();
#endif
  int argc_max;
  //  int flag_dryrun = 0;
  int pipid;
  int i, j, d;
  int extval, errsig;
  int err = 0;

  program = basename( argv[0] );
  if( argc < 2 || argv[1] == NULL ) print_usage();

  head = tail = NULL;
  i = 1;
  for( ; i<argc; i++ ) {
    spawn = new_spawn();
    if( head == NULL ) head = spawn;
    if( tail != NULL ) tail->next = spawn;
    tail = spawn;
    for( ; i<argc; i++ ) {
      if( isa_sep( argv[i] ) ) {
	if( spawn->args == NULL ) print_usage();
	break;
      } else if( *argv[i] != '-' ) {
	spawn->args = spawn->tail = new_arg( argv[i++] );
	spawn->argc = 1;
	for( ; i<argc; i++ ) {
	  if( argv[i] == NULL || isa_sep( argv[i] ) ) break;
	  arg = new_arg( argv[i] );
	  spawn->tail->next = arg;
	  spawn->tail       = arg;
	  spawn->argc ++;
	}
	break;
      } else if( strcmp( argv[i], "-h"     ) == 0 ||
		 strcmp( argv[i], "--help" ) == 0  ) {
	print_usage();
      } else if( strcmp( argv[i], "-n" ) == 0 ) {
	if( argv[i+1] == NULL ||
	    !isa_digit_string( argv[i+1] ) ||
	    ( spawn->ntasks = strtol( argv[i+1], NULL, 10 ) ) == 0 ) {
	  print_usage();
	}
	i ++;
#ifdef NOTYET
      } else if( strcmp( argv[i], "-c" ) == 0 &&
		 argv[++i] != NULL ) {
	corebind_t 	*cb;
	char 		*p = argv[i];
	spawn->cb = spawn->cb_tail = new_corebind( &p );
	while( *p == ',' ) {
	  p ++;
	  cb = new_corebind( &p );
	  spawn->cb_tail->next = cb;
	  spawn->cb_tail       = cb;
	}
#endif
      } else if( strcmp( argv[i], "-f" ) == 0 && argv[i+1] != NULL ) {
	spawn->func = argv[++i];
      } else if( strcmp( argv[i], "-d" ) == 0 ) {
	//flag_dryrun = 1;
      } else {
	print_usage();
      }
    }
  }

#ifdef DEBUG
  for( spawn = head, i = 0; spawn != NULL; spawn = spawn->next ) {
    int j;
    printf( "[%d] ntasks:%d argc:%d func:'%s'\n",
	    i++, spawn->ntasks, spawn->argc, spawn->func );
    for( arg = spawn->args, j = 0; arg != NULL; arg = arg->next ) {
      printf( "    (%d) '%s'\n", j++, arg->arg );
    }
  }
#endif

  ntasks   = 0;
  argc_max = 0;
  for( spawn = head; spawn != NULL; spawn = spawn->next ) {
    if( spawn->args == NULL || spawn->args->arg == NULL ) {
      print_usage();
    }
    ntasks += spawn->ntasks;
    argc_max = ( spawn->argc > argc_max ) ? spawn->argc : argc_max;
  }
  if( ntasks > PIP_NTASKS_MAX ) {
    fprintf( stderr, "%s: Too many tasks\n", program );
    err = EOVERFLOW;
    goto error;
  }

  argc_max ++;
  nargv = (char**) malloc( sizeof( char* ) * argc_max );
  if( nargv == NULL ) {
    fprintf( stderr, "%s: Not enough memory (nargv)\n", program );
    err = ENOMEM;
    goto error;
  }

  if( ( err = pip_init( NULL, &ntasks, NULL, 0 ) ) != 0 ) {
    fprintf( stderr, "%s: pip_init()=%d\n", program, err );
    goto error;
  }
  j = 0;
  nt_start = 0;
  for( spawn = head; spawn != NULL; spawn = spawn->next ) {
    for( arg = spawn->args, i = 0; arg != NULL; arg = arg->next ) {
      nargv[i++] = arg->arg;
    }
    nargv[i] = NULL;
    if( spawn->func == NULL ) {
      pip_spawn_from_main( &prog, nargv[0], nargv,
			   NULL, NULL );
    } else {
      pip_spawn_from_func( &prog, nargv[0], spawn->func, NULL,
			   NULL, NULL );
    }
    for( i=0; i<spawn->ntasks; i++ ) {
#ifdef NOT_YET
      int j = i;
      int s = nt_start;
      int c;
      if( j == 0 ) {
	c = nth_core( spawn->cb, s, &j );
	s = c;
      } else {
	while( j > 0 ) {
	  c = nth_core( spawn->cb, s, &j );
	  s = c;
	}
      }
      d = ( s + nt_start ) % ncores;
#else
      d = PIP_CPUCORE_ASIS;
#endif
      pipid = j++;
      err = pip_task_spawn( &prog, d, 0, &pipid, NULL );
      if( err ) {
	(void) pip_kill_all_child_tasks();
	pip_exit( err );
      }
    }
    nt_start += spawn->ntasks;
  }
  extval = 0;
  errsig = 0;
  for( i=0; i<ntasks; i++ ) {
    int threaded, status, ex;

    pip_is_threaded( &threaded );
    if( threaded ) {
      pipid = i;
      if( ( err = pip_wait( pipid, &status ) ) < 0 ) break;
    } else {
      if( ( err = pip_wait_any( &pipid, &status ) ) < 0 ) break;
    }
    if( WIFEXITED( status ) ) {
      ex = WEXITSTATUS( status );
      if( ex > extval ) extval = ex;
    } else if( WIFSIGNALED( status ) ) {
      int sig = WTERMSIG( status );
      fprintf( stderr, "%s: PIPID:%d signaled (%s)\n", program, pipid, strsignal(sig) );
      if( errsig == 0 ) errsig = sig;
    }
  }
  err = extval;
 error:
  if( nargv != NULL ) free( nargv );
  free_spawn( head );

  if( errsig > 0 ) err = errsig;

  return err;
}

#endif	/* DOXYGEN */
