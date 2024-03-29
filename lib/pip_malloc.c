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

#include <pip/pip_internal.h>
#include <malloc.h>

#ifdef DEBUG
#undef DEBUG
#endif

int	pip_dont_wrap_malloc PIP_PRIVATE = 1;

#define PIP_MALLOC
#ifdef PIP_MALLOC

typedef struct pip_malloc_info {
  uint32_t	magic;
  int		pipid;
} pip_malloc_info_t;

void *__libc_malloc(size_t);
void  __libc_free(void*);
void *__libc_calloc(size_t nmemb, size_t size);
void *__libc_realloc(void *ptr, size_t size);
void *__libc_memalign(size_t, size_t);

#define PIP_MALLOC_MAGIC	(0xA5B6C7D8U)

static size_t pip_malloc_usable_size_orig( void *ptr ) {
  return pip_libc_ftab(NULL)->malloc_usable_size( ptr );
}

static int pip_is_pip_malloced( void *addr ) {
  pip_malloc_info_t info;
  void		*p;
  size_t	sz;
  int 		pipid;

  sz = pip_malloc_usable_size_orig( addr );
  p  = addr + sz - sizeof(info);
  memcpy( &info, p, sizeof(info) );
  pipid = info.pipid;

  if( pip_root == NULL || pip_task == NULL ) {
    return -1;
  } else if( info.magic != PIP_MALLOC_MAGIC ) {
    return -1;
  } else if( pipid == PIP_PIPID_ROOT ) {
    return pipid;
  } else if( pipid <  0 || 
	     pipid >  pip_root->ntasks ) {
    /* possibly not allocated by pip_malloc */
    return -1;
  }
  return pipid;
}

size_t pip_malloc_usable_size( void *ptr ) {
  pip_malloc_info_t 	info;
  int		pipid;
  size_t 	sz;

  if( ptr == NULL ) return 0;
  sz = pip_malloc_usable_size_orig( ptr );
  pipid = pip_is_pip_malloced( ptr );
  if( pipid == PIP_PIPID_ROOT ||
      pipid >= 0 ) {
    sz -= sizeof(info);
  }
  return sz;
}

size_t malloc_usable_size( void *ptr ) {
  return pip_malloc_usable_size( ptr );
}

static int pip_get_pipid_curr( void ) {
  int pipid;

  if( pip_root == NULL || pip_task == NULL ) {
    return PIP_PIPID_NULL;
  }
  pipid = pip_task->pipid;
  if( ( pipid < 0 || pipid > pip_root->ntasks ) &&
      pipid != PIP_PIPID_ROOT ) {
    /* this may happen when the task is terminating */
    pipid = ( (uintptr_t)pip_task - (uintptr_t)&pip_root->tasks ) /
      sizeof(pip_task_t);
    ASSERTD( pipid >= 0 && pipid < pip_root->ntasks );
  }
  return pipid;
}

void pip_free_all( void ) {
  pip_task_t	*task = NULL;

  if( pip_task != NULL ) {
    task = pip_task;
  }

  if( task != NULL && task->malloc_free_list != 0 ) {
    pip_atomic_t free_list, *free_listp;

    ENTER;
    free_listp = &task->malloc_free_list;
    do {
      free_list = *free_listp;
    } while( pip_comp2_and_swap( free_listp, free_list, (pip_atomic_t) 0 ) == 0 );
    
    while( free_list != 0 ) {
      pip_atomic_t *next = *(pip_atomic_t**)free_list;
      DBGF( "free_list: %p", (void*)free_list );
      __libc_free( (void*)free_list );
      free_list = (pip_atomic_t) next;
    }
    RETURNV;
  }
}

void pip_free( void *addr ) {
  volatile pip_atomic_t free_list;
  pip_atomic_t	*free_listp;
  pip_task_t	*task;
  int 		pipid, self;

  pip_free_all();
  if( addr == NULL ) return;
  if( pip_dont_wrap_malloc ||
      pip_root == NULL     ||
      pip_task == NULL ) {
    __libc_free( addr );
  } else {
    pipid = pip_is_pip_malloced( addr );
    if( pipid < 0 && pipid != PIP_PIPID_ROOT ) {
      __libc_free( addr );
    } else {
      self = pip_get_pipid_curr();
      if( self == PIP_PIPID_NULL || self == pipid ) {
	__libc_free( addr );
      } else {
	task = ( pipid == PIP_PIPID_ROOT ) ? 
	  pip_root->task_root :
	  &pip_root->tasks[pipid];
	free_listp = (pip_atomic_t*) &task->malloc_free_list;
	do {
	  free_list = *free_listp;
	  *(pip_atomic_t*)addr = free_list;
	} while( pip_comp2_and_swap( free_listp, 
				     free_list, 
				     (pip_atomic_t) addr ) == 0 );
      }
    }
  }
}

void free( void *addr ) {
  pip_free( addr );
}

void *pip_malloc( size_t size ) {
  void *rv;

  if( pip_dont_wrap_malloc ) {
    rv = __libc_malloc( size );
  } else {
    pip_free_all();
    if( pip_root == NULL || pip_task == NULL ) {
      rv = __libc_malloc( size );
    } else {
      pip_malloc_info_t	info;
      size_t		sz;
      
      if( ( rv = __libc_malloc( size + sizeof(info) ) ) != NULL ) {
	info.magic = PIP_MALLOC_MAGIC;
	info.pipid = pip_get_pipid_curr();
	sz = pip_malloc_usable_size_orig( rv );
	memcpy( rv + sz - sizeof(info), &info, sizeof(info) );
      }
    }
  }
  return rv;
}

void *malloc( size_t size ) {
  return pip_malloc( size );
}

void *pip_calloc( size_t nmemb, size_t size ) {
  size_t	sz = nmemb * size;
  void 		*rv = pip_malloc( sz );
  if( rv != NULL ) memset( rv, 0, sz );
  return rv;
}

void *calloc( size_t nmemb, size_t size ) {
  return pip_calloc( nmemb, size );
}

void *pip_realloc( void *ptr, size_t size ) {
  pip_malloc_info_t	info;
  size_t		sz;
  void			*rv;

  pip_free_all();
  if( pip_root == NULL || pip_task == NULL ) {
    rv = __libc_realloc( ptr, size );
  } else if( ptr == NULL ) {
    rv = pip_malloc( size );
  } else {
    if( ( rv = pip_malloc( size ) ) != NULL ) {
      sz = pip_malloc_usable_size_orig( ptr );
      int pipid = pip_is_pip_malloced( ptr );
      if( pipid == PIP_PIPID_ROOT || pipid >= 0 ) {
	sz -= sizeof(info);
      }
      sz = ( sz > size ) ? size : sz;
      memcpy( rv, ptr, sz );
      pip_free( ptr );
    }
  }
  return rv;
}

void *realloc( void *ptr, size_t size ) {
  return pip_realloc( ptr, size );
}

void *pip_memalign( size_t alignment, size_t size ) {
  void	*rv;

  if( pip_dont_wrap_malloc ) {
    rv = __libc_memalign( alignment, size );
  } else {
    pip_free_all();
    if( pip_root == NULL || pip_task == NULL ) {
      rv = __libc_memalign( alignment, size );
    } else {
      pip_malloc_info_t	info;
      size_t		sz;

      rv = __libc_memalign( alignment, size + sizeof(info) );
      if( rv != NULL ) {
	info.magic = PIP_MALLOC_MAGIC;
	info.pipid = pip_get_pipid_curr();
	sz = pip_malloc_usable_size_orig( rv );
	memcpy( rv + sz - sizeof(info), &info, sizeof(info) );
      }
    }
  }
  return rv;
}

void *memalign( size_t alignment, size_t size ) {
  return pip_memalign( alignment, size );
}

void *aligned_alloc( size_t alignment, size_t size ) {
  return pip_memalign( alignment, size );
}

int pip_posix_memalign( void **memptr, size_t alignment, size_t size ) {
  size_t al;
  int bc;
  int err = 0;

  for( al=alignment,bc=0; al>0; al>>=1 ) {
    if( al & 1 ) {
      bc ++;
      if( bc > 1 ) break;
    }
  }

  if( bc == 0 || bc > 1 ) {
    err = EINVAL;
  } else {
    void *addr = pip_memalign( alignment, size );
    if( addr == NULL ) {
      err = ENOMEM;
    } else {
      *memptr = addr;
    }
  }
  return err;
}

int posix_memalign( void **memptr, size_t alignment, size_t size ) {
  return pip_posix_memalign( memptr, alignment, size );
}

#else  /* PIP_MALLOC */

size_t pip_malloc_usable_size( void *ptr ) {
  return malloc_usable_size( ptr );
}

void *pip_malloc( size_t size ) {
  return malloc( size );
}

void pip_free( void *addr ) {
  free( addr );
}

void *pip_calloc( size_t nmemb, size_t size ) {
  return calloc( nmemb, size );
}

void *pip_realloc( void *ptr, size_t size ) {
  return realloc( ptr, size );
}

int pip_posix_memalign( void **memptr, size_t alignment, size_t size ) {
  return posix_memalign( memptr, alignment, size );
}

void pip_free_all( void ) { return; }

#endif /* PIP_MALLOC */
