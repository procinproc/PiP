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
 * $PIP_VERSION: Version 2.1.0$
 *
 * $Author: Atsushi Hori (R-CCS)
 * Query:   procinproc-info@googlegroups.com
 * User ML: procinproc-users@googlegroups.com
 * $
 */

#include <pip/pip_internal.h>

#include <sys/mman.h>
#include <link.h>

typedef struct {
  char			*dsoname;
  char			**exclude;
  pip_patch_list_t	*patch_list;
} pip_phdr_itr_args;

typedef struct {
  void 			**got_entry;
  void			*old_addr;
} pip_got_undo_list_t;

static pip_got_undo_list_t	*pip_got_undo_list = NULL;
static int			pip_got_undo_size  = 0;
static int			pip_got_undo_currp = 0;

static void pip_unprotect_page( void *addr ) {
  ssize_t pgsz = sysconf( _SC_PAGESIZE );
  void   *page;

  ASSERT( pgsz > 0 );
  page = (void*) ( (uintptr_t)addr & ~( pgsz - 1 ) );
  ASSERT( mprotect( page, (size_t) pgsz, PROT_READ | PROT_WRITE ) == 0 );
}

static void pip_undo_got_patch( void ) {
  int i;
  for( i=0; i<pip_got_undo_currp; i++ ) {
    void **got_entry = pip_got_undo_list[i].got_entry;
    void *old_addr   = pip_got_undo_list[i].old_addr;
    *got_entry = old_addr;
  }
  free( pip_got_undo_list );
  pip_got_undo_list  = NULL;
  pip_got_undo_size  = 0;
  pip_got_undo_currp = 0;
}

static void pip_add_got_patch( void **got_entry, void *old_addr ) {
  if( pip_got_undo_list == NULL ) {
    size_t pgsz = sysconf(_SC_PAGESIZE);
    pip_page_alloc( pgsz, (void**) &pip_got_undo_list );
    pip_got_undo_size  = pgsz / sizeof(pip_got_undo_list_t);
    pip_got_undo_currp = 0;
  } else if( pip_got_undo_currp == pip_got_undo_size ) {
    pip_got_undo_list_t *expanded;
    int			 newsz = pip_got_undo_size * 2;
    pip_page_alloc( newsz*sizeof(pip_got_undo_list_t), (void**) &expanded );
    memcpy( expanded, pip_got_undo_list, pip_got_undo_currp );
    free( pip_got_undo_list );
    pip_got_undo_list = expanded;
    pip_got_undo_size = newsz;
  }
  pip_got_undo_list[pip_got_undo_currp].got_entry = got_entry;
  pip_got_undo_list[pip_got_undo_currp].old_addr  = old_addr;
  pip_got_undo_currp ++;
}

static ElfW(Dyn) *pip_get_dynsec( struct dl_phdr_info *info ) {
  int i;
  for( i=0; i<info->dlpi_phnum; i++ ) {
    /* search DYNAMIC ELF section */
    if( info->dlpi_phdr[i].p_type == PT_DYNAMIC ) {
      return (ElfW(Dyn)*) ( info->dlpi_addr + info->dlpi_phdr[i].p_vaddr );
    }
  }
  return NULL;
}

static int pip_get_dynent_val( ElfW(Dyn) *dyn, int type ) {
  int i;
  for( i=0; dyn[i].d_tag!=0||dyn[i].d_un.d_val!=0; i++ ) {
    if( dyn[i].d_tag == type ) return dyn[i].d_un.d_val;
  }
  return 0;
}

static void *pip_get_dynent_ptr( ElfW(Dyn) *dyn, int type ) {
  int i;
  for( i=0; dyn[i].d_tag!=0||dyn[i].d_un.d_val!=0; i++ ) {
    if( dyn[i].d_tag == type ) return (void*) dyn[i].d_un.d_ptr;
  }
  return NULL;
}

static int pip_replace_got_itr( struct dl_phdr_info *info,
				size_t size,
				void *args ) {
  pip_phdr_itr_args *itr_args = (pip_phdr_itr_args*) args;
  char	       *dsoname = itr_args->dsoname;
  char        **exclude = itr_args->exclude;
  pip_patch_list_t *list = itr_args->patch_list;
  pip_patch_list_t *patch;
  char	*fname, *bname, *symname;
  void	*new_addr;
  int	i, j;

  fname = (char*) info->dlpi_name;
  if( fname == NULL ) return 0;

  if( ( bname = strrchr( fname, '/' ) ) != NULL ) {
    bname ++;		/* skp '/' */
  } else {
    bname = fname;
  }

  if( exclude != NULL && *bname != '\0' ) {
    for( i=0; exclude[i]!=NULL; i++ ) {
      if( strncmp( exclude[i], bname, strlen(exclude[i]) ) == 0 ) {
	return 0;
      }
    }
  }
  if( dsoname == NULL || *dsoname == '\0' ||
      strncmp( dsoname, bname, strlen(dsoname) ) == 0 ) {
    ElfW(Dyn) 	*dynsec = pip_get_dynsec( info );
    ElfW(Rela) 	*rela   = (ElfW(Rela)*) pip_get_dynent_ptr( dynsec, DT_JMPREL );
    ElfW(Rela) 	*irela;
    ElfW(Sym)	*symtab = (ElfW(Sym)*)  pip_get_dynent_ptr( dynsec, DT_SYMTAB );
    char	*strtab = (char*)       pip_get_dynent_ptr( dynsec, DT_STRTAB );
    int		nrela   = pip_get_dynent_val(dynsec,DT_PLTRELSZ)/sizeof(ElfW(Rela));

    for( i=0; ; i++ ) {
      patch = &list[i];
      symname  = patch->name;
      new_addr = patch->addr;
      DBGF( "symname: '%s'  addr: %p", symname, new_addr );
      if( symname == NULL ) break;

      irela = rela;
      for( j=0; j<nrela; j++,irela++ ) {
	int symidx;
	if( sizeof(void*) == 8 ) {
	  symidx = ELF64_R_SYM(irela->r_info);
	} else {
	  symidx = ELF32_R_SYM(irela->r_info);
	}
	char *sym = strtab + symtab[symidx].st_name;
	//DBGF( "%s : %s", sym, symname );
	if( strcmp( sym, symname ) == 0 ) {
	  void	*secbase    = (void*) info->dlpi_addr;
	  void	**got_entry = (void**) ( secbase + irela->r_offset );
	  
	  DBGF( "%s:GOT[%d] '%s'  GOT:%p", bname, j, sym, got_entry );
	  pip_unprotect_page( (void*) got_entry );
	  *got_entry = new_addr;
	  pip_add_got_patch( got_entry, *got_entry );
	  break;
	}
      }
    }
  }
  return 0;
}

int pip_patch_GOT( char *dsoname, char **exclude, pip_patch_list_t *patch_list ) {
  pip_phdr_itr_args itr_args;

  ENTER;
  itr_args.dsoname    = dsoname;
  itr_args.exclude    = exclude;
  itr_args.patch_list = patch_list;

  RETURN_NE( dl_iterate_phdr( pip_replace_got_itr, (void*) &itr_args ) );
}

void pip_undo_patch_GOT( void ) {
  ENTER;
  pip_undo_got_patch();
  RETURNV;
}
