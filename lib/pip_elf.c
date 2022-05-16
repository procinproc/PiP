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
 * System Software Development Team, 2016-2021
 * $
 * $PIP_VERSION: Version 2.4.0$
 *
 * $Author: Atsushi Hori (R-CCS)
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#include <pip/pip_internal.h>

static ElfW(Dyn) *pip_get_dyn_seg( struct dl_phdr_info *info ) {
  int i;
  for( i=0; i<info->dlpi_phnum; i++ ) {
    /* search DYNAMIC ELF section */
    if( info->dlpi_phdr[i].p_type == PT_DYNAMIC ) {
      return (ElfW(Dyn)*) ( info->dlpi_addr + info->dlpi_phdr[i].p_vaddr );
    }
  }
  return NULL;
}

static pip_libc_ftab_t  *pip_libc_ftab_last_resort;

typedef struct pip_libc_func {
  char 		*name;
  size_t	offset;
} pip_libc_func_t;

#define LIBC_FUNC(N)	{ #N, offsetof(pip_libc_ftab_t,N) }
#define LIBC_FUNC_NULL	{ NULL, 0 }
  
typedef struct pip_libc_dso_syms {
  char 			*dso;
  pip_libc_func_t	funcs[];
} pip_libc_dso_syms_t;

typedef struct pip_find_syms_arg {
  pip_libc_dso_syms_t	**dsos;
  pip_libc_ftab_t 	*ftab;
} pip_find_syms_arg_t;

static void pip_find_faddr_seg( pip_libc_func_t *funcs, 
				off_t base, 
				ElfW(Sym) *symtab, 
				char *strtab,
				void *libc_ftabp ) {
  pip_libc_func_t *func;
  int 	c, i;

  c = 0;
  for( func=funcs; func->name!=NULL; func++ ) c++;

  for( i=0; c>0; i++ ) {
    char *name = strtab + symtab[i].st_name;
    if( name == NULL || *name == '\0' ) continue;
    for( func=funcs; func->name!=NULL; func++ ) {
      /* FIXME: no way to stop this loop if not found !! */
      if( strcmp( func->name, name ) == 0  ) {
	void *faddr = ((void*)symtab[i].st_value) + base;
	c --;
	*((void**)(libc_ftabp + func->offset)) = faddr;
	break;
      }
    }
  }
}

static void pip_search_funcs_seg( pip_libc_func_t *funcs, 
				  off_t secbase, 
				  ElfW(Dyn) *dyn,
				  pip_libc_ftab_t *libc_ftab ) {
  ElfW(Sym)	*symtab = NULL;
  char		*strtab = NULL;
  int 		i;

  for( i=0; dyn[i].d_tag!=0||dyn[i].d_un.d_val!=0; i++ ) {
    switch( (int) dyn[i].d_tag ) {
    case DT_SYMTAB: 
      symtab = (ElfW(Sym)*) dyn[i].d_un.d_ptr;
      break;
    case DT_STRTAB:
      strtab = (char*)      dyn[i].d_un.d_ptr;
      break;
    }
    if( symtab != NULL && strtab != NULL ) {
      pip_find_faddr_seg( funcs, secbase, symtab, strtab, libc_ftab );
      break;
    }
  }
}

static int 
pip_find_symbols_seg( struct dl_phdr_info *info, size_t size, void *varg ) {
  pip_find_syms_arg_t *arg = (pip_find_syms_arg_t*) varg;
  pip_libc_dso_syms_t *dso;
  const char *fname = info->dlpi_name;
  const char *bname;
  int i;

  DBGF( "DSO: '%s' @ %p", fname, (void*) info->dlpi_addr );
  if( fname != NULL && *fname != '\0' ) {
    if( ( bname = strrchr( fname, '/' ) ) != NULL ) {
      bname ++;		/* skp '/' */
    } else {
      bname = fname;
    }
    for( i=0; 
	 ( dso=arg->dsos[i] ), ( dso != NULL && dso->dso != NULL ); 
	 i++ ) {
      DBGF( "DSO: %s  %s", bname, dso->dso );
      if( strncmp( dso->dso, bname, strlen(dso->dso) ) == 0 &&
	  ( bname[strlen(dso->dso)] == '.' ||
	  bname[strlen(dso->dso)] == '-' ) ) {
	pip_search_funcs_seg( dso->funcs, 
			      (off_t) info->dlpi_addr, 
			      pip_get_dyn_seg( info ), 
			      arg->ftab );
	break;
      }
    }
  }
  return 0;
}

static void pip_find_dso_symbols_seg( pip_libc_dso_syms_t **dsos, 
				      pip_libc_ftab_t *libc_ftab ) {
  pip_find_syms_arg_t arg = { dsos, libc_ftab };
  dl_iterate_phdr( pip_find_symbols_seg, (void*) &arg );
}

static pip_libc_dso_syms_t pip_dso_libc =
  { "libc", 
    {
      LIBC_FUNC( fflush ),
      LIBC_FUNC( exit ),
      LIBC_FUNC( sbrk ),
      LIBC_FUNC( malloc_usable_size ),
      LIBC_FUNC( posix_memalign ),
      LIBC_FUNC( getaddrinfo ),
      LIBC_FUNC( freeaddrinfo ),
      LIBC_FUNC( gai_strerror ),
      LIBC_FUNC_NULL
    }
  };

static pip_libc_dso_syms_t pip_dso_libpthread =
  { "libpthread", 
    {
      LIBC_FUNC( pthread_exit ),
      LIBC_FUNC_NULL
    }
  };

static pip_libc_dso_syms_t pip_dso_libdl =
  { "libdl", 
    {
      LIBC_FUNC( dlsym ),
      LIBC_FUNC( dlopen ),
      LIBC_FUNC( dlmopen ),
      LIBC_FUNC( dlinfo ),
      LIBC_FUNC( dladdr ),
      LIBC_FUNC( dlvsym ),
      LIBC_FUNC( dlerror ),
      LIBC_FUNC_NULL
    }
  };

static void pip_setup_libc_ftab( pip_libc_ftab_t *libc_ftab ) {
  pip_libc_dso_syms_t *pip_libc_dsos[] = {
    &pip_dso_libc,
    &pip_dso_libpthread,
    &pip_dso_libdl,
    NULL
  };
  int i, c = 0;

  if( libc_ftab->fflush == NULL ) { /* check the first one */
    pip_find_dso_symbols_seg( pip_libc_dsos, libc_ftab );
  }
  int n = sizeof(pip_libc_ftab_t)/sizeof(void*);
  for( i=0; i<n; i++ ) {
    if( ((void**)libc_ftab)[i] == NULL ) {
      c ++;
      DBGF( "libc_ftab[%d] is NULL", i );
    }
  }
  ASSERT( c == 0 );
  pip_libc_ftab_last_resort = libc_ftab;
}

void pip_set_libc_ftab( pip_libc_ftab_t *ftabp ) {
  pip_libc_ftab_last_resort = ftabp;
}

pip_libc_ftab_t *pip_libc_ftab( pip_task_t *task ) {
  static pip_libc_ftab_t ftab;

  if( task == NULL ) task = pip_task;
  if( task == NULL ) {
    if( pip_libc_ftab_last_resort == NULL ) {
      pip_setup_libc_ftab( &ftab );
    }
    return pip_libc_ftab_last_resort;
  }
  ASSERTD( task->libc_ftabp != NULL );
  return task->libc_ftabp;
}
