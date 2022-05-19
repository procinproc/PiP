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

#ifndef _pip_h_
#define _pip_h_

#ifndef DOXYGEN_INPROGRESS

#include <semaphore.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

#define PIP_OPTS_NONE			(0x0)

#define PIP_MODE_PTHREAD		(0x1000U)
/* the following two modes are a submode of PIP_MODE_PROCESS */
#define PIP_MODE_PROCESS_PRELOAD	(0x0100U)
#define PIP_MODE_PROCESS_PIPCLONE	(0x0400U)
#define PIP_MODE_PROCESS_GOT_OBS	(0x8000U) /* obsolete */
#define PIP_MODE_MASK			(0xFF00U)
#define PIP_MODE_PROCESS			\
  ( PIP_MODE_PROCESS_PRELOAD | PIP_MODE_PROCESS_PIPCLONE )

#define PIP_VALID_OPTS					\
  ( PIP_MODE_PTHREAD         |				\
    PIP_MODE_PROCESS_PRELOAD |				\
    PIP_MODE_PROCESS_GOT_OBS |				\
    PIP_MODE_PROCESS_PIPCLONE )

#define PIP_ENV_PRELOAD			"PIP_PRELOAD"
#define PIP_ENV_MODE			"PIP_MODE"
#define PIP_ENV_MODE_THREAD		"thread"
#define PIP_ENV_MODE_PTHREAD		"pthread"
#define PIP_ENV_MODE_PROCESS		"process"
#define PIP_ENV_MODE_PROCESS_PRELOAD	"process:preload"
#define PIP_ENV_MODE_PROCESS_PIPCLONE	"process:pipclone"
#define PIP_ENV_MODE_PROCESS_GOT	"process:got" /* obsolete from v2.4 */

#define PIP_ENV_STOP_ON_START		"PIP_STOP_ON_START"

#define PIP_ENV_GDB_PATH		"PIP_GDB_PATH"
#define PIP_ENV_GDB_COMMAND		"PIP_GDB_COMMAND"
#define PIP_ENV_GDB_SIGNALS		"PIP_GDB_SIGNALS"
#define PIP_ENV_SHOW_MAPS		"PIP_SHOW_MAPS"
#define PIP_ENV_SHOW_PIPS		"PIP_SHOW_PIPS"

#define PIP_ENV_QUIET			"PIP_QUIET"

#define PIP_ENV_STACKSZ			"PIP_STACKSZ"

#define PIP_MAGIC_NUM			(-747) /* "PIP" P=7, I=4 */

#define PIP_PIPID_ROOT			(PIP_MAGIC_NUM-1)
#define PIP_PIPID_MYSELF		(PIP_MAGIC_NUM-2)
#define PIP_PIPID_ANY			(PIP_MAGIC_NUM-3)
#define PIP_PIPID_NULL			(PIP_MAGIC_NUM-4)
#define PIP_PIPID_SELF			PIP_PIPID_MYSELF

#define PIP_NTASKS_MAX			(300)

#define PIP_CPUCORE_FLAG_SHIFT		(24)
#define PIP_CPUCORE_FLAG_MASK		(0xFFU<<PIP_CPUCORE_FLAG_SHIFT)
#define PIP_CPUCORE_CORENO_MAX		(1U<<20)
#define PIP_CPUCORE_ASIS 		(0x1U<<PIP_CPUCORE_FLAG_SHIFT)
#define PIP_CPUCORE_ABS 		(0x2U<<PIP_CPUCORE_FLAG_SHIFT)
#define PIP_CPUCORE_CORENO_MASK		((0x1U<<PIP_CPUCORE_FLAG_SHIFT)-1)

#define PIP_YIELD_DEFAULT		(0x0U)
#define PIP_YIELD_USER			(0x1U)
#define PIP_YIELD_SYSTEM		(0x2U)

/* PiP Version 2.4 or later */
#define PIP_HAVE_LDPIP

typedef struct {
  char		*prog;
  char		**argv;
  char		**envv;
  char		*funcname;
  void		*arg;
  void		*aux;
  void		*reserved[2];
} pip_spawn_program_t;

typedef int (*pip_spawnhook_t)( void* );

typedef struct {
  pip_spawnhook_t	before;
  pip_spawnhook_t	after;
  void 			*hookarg;
  void 			*reserved[5];
} pip_spawn_hook_t;

typedef uintptr_t	pip_id_t;

typedef struct pip_barrier {
  int			count_init;
  volatile uint32_t	count;
  volatile int		gsense;
  sem_t			semaphore[2];
} pip_barrier_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INLINE
#define INLINE		inline static
#endif

#endif /* DOXYGEN */

  /**
   * \defgroup PiP-API0-init-fin API: Initialization/Finalization
   * @{
   */

  /**
   * \defgroup pip_init pip_init
   * @{ */
  /** \description
   * This function initializes the PiP library. The PiP root process
   * must call this. A PiP task is not required to call this function
   * unless the PiP task calls any PiP functions.\n\n
   * When this function is called by a PiP root, \c ntasksp, and \c root_expp
   * are input parameters. If this is called by a PiP task, then those
   * parameters are output  
   * returning the same values input by the root.\n\n
   * A PiP task may or may not call this function. \c pip_init is
   * called implicitly even if the PiP task program is NOT explicitly
   * linked with the PiP library. 
   *
   * \param[out] pipidp When this is called by the PiP root
   *  process, then this returns \c PIP_PIPID_ROOT, otherwise it returns
   *  the PiP ID of the calling PiP task.
   * \param[in,out] ntasksp When called by the PiP root, it specifies
   *  the maximum number of PiP tasks. When called by a PiP task, then
   *  the number specified by the PiP root is returned.
   * \param[in,out] root_expp If the root PiP is ready to export a
   *  memory region to any PiP task(s), then this parameter is to pass
   *  the exporting address. If
   *  the PiP root is not ready to export or has nothing to export
   *  then this variable can be NULL. When called by a PiP task, it
   *  returns the exported address by the PiP root, if any.
   * \param[in] opts Specifying the PiP execution mode and See below.
   *
   * \par Execution mode option
   * Users may explicitly specify the PiP execution mode.
   * This execution mode can be categorized in two; process mode and
   * thread mode. In the process execution mode, each PiP task may
   * have its own file descriptors, signal handlers, and so on, just
   * like a process. Contrastingly, in the pthread executionn mode, file
   * descriptors and signal handlers are shared among PiP root and PiP
   * tasks while maintaining the privatized variables.
   * \n
   * To spawn a PiP task in the process mode, the PiP library modifies
   * the \b clone() flag so that the created PiP task can exhibit the
   * alomost same way with that of normal Linux process. One of the
   * option flag values or any combination of; \b PIP_MODE_PTHREAD and
   * \b PIP_MODE_PROCESS, can be specified as the option flag. Or,
   * users may specify the execution mode by the \b PIP_MODE environment
   * described below.
   *
   * \return Zero is returned if this function succeeds. Otherwise an
   * error number is returned.
   *
   * \retval EINVAL \a \*ntasksp is negative
   * \retval EBUSY PiP root called this function twice or more without
   * calling \ref pip_fin.
   * \retval EPERM \a opts is invalid or unacceptable
   * \retval EOVERFLOW \a \*ntasksp is too large
   * \retval ELIBSCN verssion miss-match between PiP root and PiP task
   *
   * \environment
   * \arg \b PIP_MODE Specifying the PiP execution mmode. Its value can be
   * either \c thread, \c pthread, or \c process.
   * \arg \b PIP_STACKSZ Sepcifying the stack size (in bytes). The
   * \b KMP_STACKSIZE and \b OMP_STACKSIZE are also effective. The 't',
   * 'g', 'm', 'k' and 'b' posfix character can be used, as
   * abbreviations of Tera, Giga, Mega, Kilo and Byte, respectively.
   * \arg \b PIP_STOP_ON_START Specifying the PIP ID to stop on start
   * to debug the specified PiP task from the beginning. If the
   * before hook is specified, then the PiP task will be stopped just
   * before calling the before hook.
   * \arg \b PIP_GDB_PATH If thisenvironment is set to the path pointing to the PiP-gdb
   * executable file, then PiP-gdb is automatically attached when an
   * excetion signal (SIGSEGV and SIGHUP by default) is delivered. The signals which
   * triggers the PiP-gdb invokation can be specified the
   * \c PIP_GDB_SIGNALS environment described below.
   * \arg \b PIP_GDB_COMMAND If this PIP_GDB_COMMAND is set to a filename
   * containing some GDB commands, then those GDB commands will be executed by the GDB
   * in batch mode, instead of backtrace.
   * \arg \b PIP_GDB_SIGNALS Specifying the signal(s) resulting
   * automatic PiP-gdb attach. 
   * Signal names (case insensitive) can be concatenated by the '+' or '-' symbol.
   * 'all' is reserved to specify most of the signals. For example, 'ALL-TERM'
   * means all signals excepting \c SIGTERM, another example,
   * 'PIPE+INT' means \c SIGPIPE 
   * and \c SIGINT. Some signals such as SIGKILL and SIGCONT cannot be
   * specified.
   * These PIP_GDB related settings are
   * only valid with the process mode.
   * \arg \b PIP_SHOW_MAPS If the value is 'on' and one of the above
   * exection signals is delivered, 
   * then the memory map will be shown.
   * \arg \b PIP_SHOW_PIPS If the value is 'on' and one of the above
   * exection signals is delivered, 
   * then the process status by using the \c pips command will be shown.
   *
   * \bugs
   * Is is NOT guaranteed that users can spawn tasks up to the number
   * specified by the \a ntasksp argument. There are some limitations
   * come from outside of the PiP library (from GLIBC).
   *
   * \sa pip_named_export
   * \sa pip_export
   * \sa pip_fin
   * \sa pip-mode
   * \sa pips
   *
   * \author Atsushi Hori
   */
  int pip_init( int *pipidp, int *ntasksp, void **root_expp, int opts );
  /** @} */

  /**
   * \defgroup pip_fin pip_fin
   * @{ */
  /** \description
   * This function finalizes the PiP library. After calling this, most
   * of the PiP functions will return the error code \c EPERM.
   *
   * \return zero is returned if this function succeeds. On error,
   * error number is returned. 
   * \retval EPERM \c pip_init is not yet called
   * \retval EBUSY \c one or more PiP tasks are not yet terminated
   *
   * \notes
   * The behavior of calling \ref pip_init after calling this \ref pip_fin
   * is not defined and recommended not to do so.
   *
   * \sa pip_init
   *
   * \author Atsushi Hori
   */
  int pip_fin( void );
  /** @} */
  /** @} */

  /**
   * \defgroup PiP-API1-spawn API: Spawning PiP task
   * @{
   */

  /**
   * \defgroup pip_spawn_from_main pip_spawn_from_main
   * @{ */
  /** \description
   * This function sets up the \c pip_spawn_program_t structure for
   * spawning a PiP task, starting from the mmain function.
   *
   * \param[out] progp Pointer to the \c pip_spawn_program_t
   *  structure in which the program invokation information will be set
   * \param[in] prog Path to the executiable file.
   * \param[in] argv Argument vector.
   * \param[in] envv Environment variables. If this is \c NULL, then
   * the \c environ variable is used for the spawning PiP task.
   * \param[in] aux Auxiliary data to be associated with the created PiP task
   *
   * \sa pip_task_spawn
   * \sa pip_spawn_from_func
   *
   * \author Atsushi Hori
   */

#ifndef DOXYGEN_INPROGRESS
INLINE
#endif
void pip_spawn_from_main( pip_spawn_program_t *progp,
			  char *prog, char **argv, char **envv,
			  void *aux ) {
  memset( progp, 0, sizeof(pip_spawn_program_t) );
  if( prog != NULL ) {
    progp->prog   = prog;
  } else {
    progp->prog   = argv[0];
  }
  progp->argv     = argv;
  if( envv == NULL ) {
    extern char **environ;
    progp->envv = environ;
  } else {
    progp->envv = envv;
  }
  progp->aux = aux;
}
  /** @} */

  /**
   * \defgroup pip_spawn_from_func pip_spawn_from_func
   * @{ */
  /** \description
   * This function sets the required information to invoke a program,
   * starting from the \c main() function. The function should have
   * the function prototype as shown below;
     \code
     int start_func( void *arg )
     \endcode
   * This start function must be globally defined in the program..
   * The returned integer of the start function will be treated in the
   * same way as the \c main function. This implies that the
   * \c pip_wait function family called from the PiP root can retrieve
   * the return code.
   *
   * \param[out] progp Pointer to the \c pip_spawn_program_t
   *  structure in which the program invokation information will be set
   * \param[in] prog Path to the executiable file.
   * \param[in] funcname Function name to be started
   * \param[in] arg Argument which will be passed to the start function
   * \param[in] envv Environment variables. If this is \c NULL, then
   * the \c environ variable is used for the spawning PiP task.
   * \param[in] aux Auxiliary data to be associated with the created PiP task
   *
   * \sa pip_task_spawn
   * \sa pip_spawn_from_main
   *
   * \author Atsushi Hori
   */
#ifndef DOXYGEN_INPROGRESS
INLINE
#endif
void pip_spawn_from_func( pip_spawn_program_t *progp,
			  char *prog, char *funcname, void *arg, char **envv,
			  void *aux ) {
  memset( progp, 0, sizeof(pip_spawn_program_t) );
  progp->prog     = prog;
  progp->funcname = funcname;
  progp->arg      = arg;
  if( envv == NULL ) {
    extern char **environ;
    progp->envv = environ;
  } else {
    progp->envv = envv;
  }
  progp->aux = aux;
}
  /** @} */

  /**
   * \defgroup pip_spawn_hook pip_spawn_hook
   * @{ */
  /** \description
   * The \a before and \a after functions are introduced to follow the
   * programming model of the \c fork and \c exec.
   * \a before function does the prologue found between the
   * \c fork and \c exec. \a after function is to free the argument if
   * it is \c malloc()ed, for example.
   * \pre
   * It should be noted that the \a before and \a after
   * functions are called in the \e context of PiP root, although they
   * are running as a part of PiP task (i.e., having PID of the
   * spawning PiP task). Conversely
   * speaking, those functions cannot access the variables defined in
   * the spawning PiP task.
   * \pre
   * The before and after hook functions  should have
   * the function prototype as shown below;
     \code
     int hook_func( void *hookarg )
     \endcode
   *
   * \param[out] hook Pointer to the \c pip_spawn_hook_t
   *  structure in which the invocation hook information will be set
   * \param[in] before Just before the executing of the spawned PiP
   *  task, this function is called so that file descriptors inherited
   *  from the PiP root, for example, can deal with. This is only
   *  effective with the PiP process mode. This function is called
   *  with the argument \a hookarg described below.
   * \param[in] after This function is called when the PiP task
   *  terminates for the cleanup purpose. This function is called
   *  with the argument \a hookarg described below.
   * \param[in] hookarg The argument for the \a before and \a after
   *  function call.
   *
   * \note
   * Note that the file descriptors and signal
   * handlers are shared between PiP root and PiP tasks in the pthread
   * execution mode.
   *
   * \sa pip_task_spawn
   *
   * \author Atsushi Hori
   */
#ifndef DOXYGEN_INPROGRESS
INLINE
#endif
void pip_spawn_hook( pip_spawn_hook_t *hook,
		     pip_spawnhook_t before,
		     pip_spawnhook_t after,
		     void *hookarg ) {
  hook->before  = before;
  hook->after   = after;
  hook->hookarg = hookarg;
}
  /** @} */

  /**
   * \defgroup pip_task_spawn pip_task_spawn
   * @{ */
  /**
   * \description
   * This function spawns a PiP task specified by \c progp argument.
   *
   * \param[out] progp Pointer to the \p pip_spawn_hook_t
   *  structure in which the invocation hook information is set
   * \param[in] coreno CPU core number for the PiP task to be bound to. By
   *  default, \p coreno is set to zero, for example, then the calling
   *  task will be bound to the first core available. This is in mind
   *  that the available core numbers are not contiguous. To specify
   *  an absolute core number, \p coreno must be bitwise-ORed with
   *  \p PIP_CPUCORE_ABS. If
   *  \p PIP_CPUCORE_ASIS is specified, then the core binding will not
   *  take place.
   * \param[in] opts option flags
   * \param[in,out] pipidp Specify PiP ID of the spawned PiP task. If
   *  \p PIP_PIPID_ANY is specified, then the PiP ID of the spawned PiP
   *  task is up to the PiP library and the assigned PiP ID will be
   *  returned.
   * \param[in] hookp Hook information to be invoked before and after
   *  the program invokation.
   *
   * \return Zero is returned if this function succeeds. On error, an
   * error number is returned.
   * \retval EPERM PiP library is not yet initialized, or PiP task
   * tries to spawn a child task 
   * \retval EINVAL \c progp is \c NULL, \c opts is invalid and/or
   * unacceptable, the value off \c pipidp is invalid, or EINVAL the
   * coreno is larger than or equal to \p PIP_CPUCORE_CORENO_MAX.
   * \retval EBUSY specified PiP ID is alredy occupied
   * \retval ENOMEM not enough memory
   * \retval ENXIO \c dlmopen failss
   *
   * \note
   * In the process execution mode, each PiP task may have its own
   * file descriptors, signal handlers, and so on, just like a
   * process. The file descriptors having the
   * \c FD_CLOEXEC flag is closed and will not be passed to the spawned
   * PiP task. Contrastingly, in the pthread executionn mode, file
   * descriptors and signal handlers are shared among PiP root and PiP
   * tasks while maintaining the privatized variables. And the
   * simulated close-on-exec will not take place in this mode.
   *
   * \environment
   * \arg \b PIP_STOP_ON_START Specifying the PIP ID to stop on start
   * to debug the specified PiP task from the beginning. If the
   * before hook is specified, then the PiP task will be stopped just
   * before calling the before hook.
   *
   * \bugs
   * In theory, there is no reason to restrict for a PiP task to
   * spawn another PiP task. However, the current glibc implementation
   * does not allow to do so.
   * \par
   * If the root process is multithreaded, only the main
   * thread can call this function.
   *
   * \sa pip_spawn_from_main
   * \sa pip_spawn_from_func
   * \sa pip_spawn_hook
   * \sa pip_spawn
   *
   * \author Atsushi Hori
   */
  int pip_task_spawn( pip_spawn_program_t *progp,
		      uint32_t coreno,
		      uint32_t opts,
		      int *pipidp,
		      pip_spawn_hook_t *hookp );
  /** @} */

  /**
   * \defgroup pip_spawn pip_spawn.
   * @{ */
  /**
   * \description
   * Another function to spawn a PiP task. This function was
   * introdcued from the beginning of PiP release and \p
   * pip_task_spawn is the newer and more flexible one. Refer to \p
   * pip_task_spawn for more details.
   *
   * \param[in] filename The executable to run as a PiP task
   * \param[in] argv Argument(s) for the spawned PiP task
   * \param[in] envv Environment variables for the spawned PiP task
   * \param[in] coreno CPU core number for the PiP task to be bound to. By
   *  default, \p coreno is set to zero, for example, then the calling
   *  task will be bound to the first core available. This is in mind
   *  that the available core numbers are not contiguous. To specify
   *  an absolute core number, \p coreno must be bitwise-ORed with
   *  \p PIP_CPUCORE_ABS. If
   *  \p PIP_CPUCORE_ASIS is specified, then the core binding will not
   *  take place.
   * \param[in,out] pipidp Specify PiP ID of the spawned PiP task. If
   *  \c PIP_PIPID_ANY is specified, then the PiP ID of the spawned PiP
   *  task is up to the PiP library and the assigned PiP ID will be
   *  returned.
   * \param[in] before Just before the executing of the spawned PiP
   *  task, this function is called so that file descriptors inherited
   *  from the PiP root, for example, can deal with. This is only
   *  effective with the PiP process mode. This function is called
   *  with the argument \a hookarg described below.
   * \param[in] after This function is called when the PiP task
   *  terminates for the cleanup purpose. This function is called
   *  with the argument \a hookarg described below.
   * \param[in] hookarg The argument for the \a before and \a after
   *  function call.
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not yet initialized, or PiP task
   * tries to spawn child task 
   * \retval EINVAL \c progp is \c NULL, \c opts is invalid and/or
   * unacceptable, the value off \c pipidp is invalid, or
   the coreno is larger than or equal to \p PIP_CPUCORE_CORENO_MAX.
   * \retval EBUSY specified PiP ID is alredy occupied
   * \retval ENOMEM not enough memory
   * \retval ENXIO \c dlmopen failss
   *
   * \bugs
   * In theory, there is no reason to restrict for a PiP task to
   * spawn another PiP task. However, the current glibc implementation
   * does not allow to do so.
   * \par
   * If the root process is multithreaded, only the main
   * thread can call this function.
   *
   * \sa pip_task_spawn
   * \sa pip_spawn_from_main
   * \sa pip_spawn_from_func
   * \sa pip_spawn_hook
   * \sa pip_task_spawn
   *
   * \author Atsushi Hori
   */
  int pip_spawn( char *filename, char **argv, char **envv,
		 int coreno, int *pipidp,
		 pip_spawnhook_t before, pip_spawnhook_t after, void *hookarg);
  /** @} */
  /** @} */

  /**
   * \defgroup PiP-API2-export API: Export/Import
   * @{
   */

  /**
   * \defgroup pip_named_export pip_named_export
   * @{ */
  /**
   * \description
   * Pass an address of a memory region to the other PiP task. Unlike
   * the simmple \p pip_export and \p pip_import
   * functions which can only export one address per task,
   * \p pip_named_export and \p pip_named_import can associate a name
   * with an address so that PiP root or PiP task can exchange
   * arbitrary number of addressess.
   *
   * \param[in] exp an address to be passed to the other PiP task
   * \param[in] format a \c printf format to give the exported address
   * a name. If this is \p NULL, then the name is assumed to be "".
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM \p pip_init is not yet called.
   * \retval EBUSY The name is already registered.
   * \retval ENOMEM Not enough memory
   *
   * \note
   * The addresses exported by \ref pip_named_export cannot be imported
   * by calling \ref pip_import, and vice versa.
   *
   * \sa pip_named_import
   * \sa pip_named_tryimport
   * \sa pip_export
   * \sa pip_import
   *
   * \author Atsushi Hori
   */
  int pip_named_export( void *exp, const char *format, ... )
    __attribute__ ((format (printf, 2, 3)));
  /** @} */

  /**
   * \defgroup pip_named_import pip_named_import
   * @{ */
  /**
   * \description
   * Import an address exported by the specified PiP task and having
   * the specified name. If it is not exported yet, the calling task
   * will be blocked.
   *
   * \param[in] pipid The PiP ID to import the exposed address
   * \param[out] expp The starting address of the exposed region of
   *  the PiP task specified by the \a pipid.
   * \param[in] format a \c printf format to give the exported address a name
   *
   * \note
   * There is a possibility of deadlock when two or more tasks are
   * mutually waiting for exported addresses.
   * \par
   * The addresses exported by \ref pip_export cannot be imported
   * by calling \ref pip_named_import, and vice versa.
   *
   * \return zero is returned if this function succeeds. On error, an
   * error number is returned.
   * \retval EPERM \p pip_init is not yet called.
   * \retval EINVAL The specified \p pipid is invalid
   * \retval ENOMEM Not enough memory
   * \retval ECANCELED The target task is terminated
   * \retval EDEADLK \p pipid is the calling task and tries to block
   * itself
   *
   * \sa pip_named_export
   * \sa pip_named_tryimport
   * \sa pip_export
   * \sa pip_import
   *
   * \author Atsushi Hori
   */
  int pip_named_import( int pipid, void **expp, const char *format, ... )
    __attribute__ ((format (printf, 3, 4)));
  /** @} */

  /**
   * \defgroup pip_named_tryimport pip_named_tryimport
   * @{ */
  /**
   * \description
   * Import an address exported by the specified PiP task and having
   * the specified name. If it is not exported yet, this returns \p EAGAIN.
   *
   * \param[in] pipid The PiP ID to import the exposed address
   * \param[out] expp The starting address of the exposed region of
   *  the PiP task specified by the \a pipid.
   * \param[in] format a \c printf format to give the exported address a name
   *
   * \note
   * The addresses exported by \ref pip_export cannot be imported
   * by calling \ref pip_named_import, and vice versa.
   *
   * \return Zero is returned if this function succeeds. On error, an
   * error number is returned.
   * \retval EPERM \p pip_init is not yet called.
   * \retval EINVAL The specified \p pipid is invalid
   * \retval ENOMEM Not enough memory
   * \retval ECANCELED The target task is terminated
   * \retval EAGAIN Target is not exported yet
   *
   * \sa pip_named_export
   * \sa pip_named_import
   * \sa pip_export
   * \sa pip_import
   *
   * \author Atsushi Hori
   */
  int pip_named_tryimport( int pipid, void **expp, const char *format, ... )
    __attribute__ ((format (printf, 3, 4)));
  /** @} */

  /**
   * \defgroup pip_export pip_export
   * @{ */
  /**
   * \description
   * Pass an address of a memory region to the other PiP task. This is
   * a very naive implementation in PiP v1 and deprecated. Once a task
   * export an address, there is no way to change the exported address
   * or undo export. You cannot export \b NULL.
   *
   * \param[in] exp An addresss
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not initialized yet
   * \retval EINVAL tried to export \b NULL
   *
   * \note 
   * This function was introduced from the first PiP version and is
   * deprecated.
   *
   * \sa pip_named_export
   * \sa pip_named_import
   * \sa pip_named_tryimport
   * \sa pip_import
   *
   * \author Atsushi Hori
   */
  int pip_export( void *exp );
  /** @} */

  /**
   * \defgroup pip_import pip_import
   * @{ */
  /**
   * \description
   * Get an address exported by the specified PiP task. This is
   * a very naive implementation in PiP v1 and deprecated. If the
   * address is not yet exported at the time of calling this function,
   * then \p NULL is returned.
   *
   * \param[in] pipid The PiP ID to import the exported address
   * \param[out] expp The exported address
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not initialized yet
   *
   * \sa pip_export
   * \sa pip_named_export
   * \sa pip_named_import
   * \sa pip_named_tryimport
   *
   * \note 
   * This function was introduced from the first PiP version and is
   * deprecated.
   *
   * \author Atsushi Hori
   */
  int pip_import( int pipid, void **expp );
  /** @} */

  /**
   * \defgroup pip_set_aux pip_set_aux
   * @{ */
  /**
   * \description
   * associate arbitrary data with a PiP task
   *
   * \param[in] aux Pointer to the user dats to assocate with the calling PiP task
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not yet initialized or already
   * finalized
   *
   * \sa pip_get_aux
   *
   * \author Atsushi Hori
   */
  int pip_set_aux( void *aux );
  /** @} */

  /**
   * \defgroup pip_get_aux pip_get_aux
   * @{ */
  /**
   * \description
   * obtain the data which was associated by \a pip_set_aus().
   *
   * \param[out] auxp Returned user data
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not yet initialized or already
   * finalized
   *
   * \sa pip_set_aux
   *
   * \author Atsushi Hori
   */
  int pip_get_aux( void **auxp );
  /** @} */
  /** @} */

  /**
   * \defgroup PiP-API3-wait API: Waiting for PiP task termination
   * @{
   */

  /**
   * \defgroup pip_wait pip_wait
   * @{ */
  /**
   * \description
   * This function can be used regardless to the PiP execution mode.
   * This function blocks until the specified PiP task terminates.
   * The macros such as \p WIFEXITED and so on defined in Glibc can be 
   * applied to the returned \p status value.
   *
   * \param[in] pipid PiP ID to wait for.
   * \param[out] status Status value of the terminated PiP task
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not initialized yet, or This
   * function is called other than PiP root.
   * \retval EDEADLK The specified \c pipid is the one of PiP root
   * \retval ECHILD The target PiP task does not exist or it was already
   * terminated and waited for
   *
   * \sa pip_exit
   * \sa pip_trywait
   * \sa pip_wait_any
   * \sa pip_trywait_any
   * \sa wait(Linux 2)
   *
   * \author Atsushi Hori
   */
  int pip_wait( int pipid, int *status );
  /** @} */

  /**
   * \defgroup pip_trywait pip_trywait
   * @{ */
  /**
   * \description
   * This function can be used regardless to the PiP execution mode.
   * This function behaves like the \p wait function of glibc and the
   * macros such as \p WIFEXITED and so on can be applied to the
   * returned \p status value.
   *
   * \param[in] pipid PiP ID to wait for.
   * \param[out] status Status value of the terminated PiP task
   *
   * \note This function can be used regardless to the PiP execution
   * mode.
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM The PiP library is not initialized yet, or this
   * function is called other than PiP root 
   * \retval EDEADLK The specified \c pipid is the one of PiP root
   * \retval ECHILD The target PiP task does not exist or it was already
   * terminated and waited for
   *
   * \sa pip_exit
   * \sa pip_wait
   * \sa pip_wait_any
   * \sa pip_trywait_any
   * \sa wait(Linux 2)
   *
   * \author Atsushi Hori
   */
  int pip_trywait( int pipid, int *status );
  /** @} */

  /**
   * \defgroup pip_wait_any pip_wait_any
   * @{ */
  /**
   * \description
   * This function can be used regardless to the PiP execution mode.
   * This function blocks until any of PiP tasks terminates.
   * The macros such as \p WIFEXITED and so on defined in Glibc can be
   * applied to the returned \p status value.
   *
   * \param[out] pipid PiP ID of terminated PiP task.
   * \param[out] status Exit statusof the terminated PiP task
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM The PiP library is not initialized yet, or the
   * function is called other than PiP root 
   * \retval ECHILD The target PiP task does not exist or it was already
   * terminated and waited for
   *
   * \sa pip_exit
   * \sa pip_wait
   * \sa pip_trywait
   * \sa pip_trywait_any
   * \sa wait(Linux 2)
   *
   * \author Atsushi Hori
   */
  int pip_wait_any( int *pipid, int *status );
  /** @} */

  /**
   * \defgroup pip_trywait_any pip_trywait_any
   * @{ */
  /**
   * \description
   * This function can be used regardless to the PiP execution mode.
   * This function blocks until any of PiP tasks terminates.
   * The macros such as \p WIFEXITED and so on defined in Glibc can be
   * applied to the returned \p status value.
   *
   * \param[out] pipid PiP ID of terminated PiP task.
   * \param[out] status Exit status of the terminated PiP task
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM The PiP library is not initialized yet, or the
   * function is called other than PiP root 
   * \retval ECHILD There is no PiP task to wait for
   *
   * \sa pip_exit
   * \sa pip_wait
   * \sa pip_trywait
   * \sa pip_wait_any
   * \sa wait(Linux 2)
   *
   * \author Atsushi Hori
   */
  int pip_trywait_any( int *pipid, int *status );
  /** @} */
  /** @} */

  /**
   * \defgroup PiP-API4-query API: Query Functions
   * @{
   */

  /**
   * \defgroup pip_get_pipid pip_get_pipid
   * @{ */
  /**
   * \description
   * obtain PIPID of the calling PiP task
   *
   * \param[out] pipidp This parameter points to the variable which
   *  will be set to the PiP ID of the calling task
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not initialized yet
   *
   * \sa pip_init
   *
   * \author Atsushi Hori
   */
  int pip_get_pipid( int *pipidp );
  /** @} */

  /**
   * \defgroup pip_is_initialized pip_is_initialized
   * @{ */
  /**
   * \description
   * query function if \a pip_init() is called already or not
   *
   * \return Return a non-zero value if PiP is already
   * initialized. Otherwise this returns zero.
   *
   * \note
   * A spawned PiP task is initialized implicitly and this function
   * returns true until it calles \p pip_fun.
   *
   * \sa pip_init
   *
   * \author Atsushi Hori
   */
  int pip_is_initialized( void );
  /** @} */

  /**
   * \defgroup pip_get_ntasks pip_get_ntasks
   * @{ */
  /**
   * \description 
   * Obtain the maximum number of PiP tasks able to be created
   *
   * \param[out] ntasksp Maximum number of PiP tasks is returned
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not yet initialized
   *
   * \sa pip_init
   *
   * \author Atsushi Hori
   */
  int pip_get_ntasks( int *ntasksp );
  /** @} */

  /**
   * \defgroup pip_get_mode pip_get_mode
   * @{ */
  /**
   * \description
   * Obtain the current PiP execution mode
   *
   * \param[out] modep Returned PiP execution mode
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not yet initialized
   *
   * \sa pip_get_mode_str
   *
   * \author Atsushi Hori
   */
  int pip_get_mode( int *modep );
  /** @} */

  /**
   * \defgroup pip_get_mode_str pip_get_mode_str
   * @{ */
  /**
   * \description
   * Obtain the current PiP execution mode in character string
   *
   * \return Return the name string of the current execution mode. If
   * PiP library is not initialized yet, then this returns \p NULL.
   *
   * \sa pip_get_mode
   *
   * \author Atsushi Hori
   */
  const char *pip_get_mode_str( void );
  /** @} */

  /**
   * \defgroup pip_get_system_id pip_get_system_id
   * @{ */
  /**
   * \description
   * The returned object depends on the PiP execution mode. In the process
   * mode it returns TID (Thread ID, not PID) and in the thread mode
   * it returns thread (\p pthread_t) associated with the PiP task
   * This function can be used regardless to the PiP execution
   * mode.
   *
   * \param[out] pipid PiP ID of a target PiP task
   * \param[out] idp a pointer to store the ID value
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM The PiP library is not initialized yet
   *
   * \sa getpid(Linux 2)
   * \sa pthread_self(Linux 3)
   *
   * \author Atsushi Hori
   */
  int pip_get_system_id( int pipid, pip_id_t *idp );
  /** @} */

  /**
   * \defgroup pip_isa_root pip_isa_root
   * @{ */
  /**
   * \description
   * a predicate if the calling task is the PiP root or not
   *
   * \return Return a non-zero value if the caller is the PiP
   * root. Otherwise this returns zero.
   *
   * \sa pip_init
   *
   * \author Atsushi Hori
   */
  int pip_isa_root( void );
  /** @} */

  /**
   * \defgroup pip_isa_task pip_isa_task
   * @{ */
  /**
   * \description
   * a predicate if the calling task is a PiP task or not
   *
   * \return Return a non-zero value if the caller is the PiP
   * task. Otherwise this returns zero.
   *
   * \sa pip_init
   *
   * \author Atsushi Hori
   */
  int pip_isa_task( void );
  /** @} */

  /**
   * \defgroup pip_is_threaded pip_is_threaded
   * @{ */
  /**
   * \description
   * a predicate if the current PiP execution mode is thread or not.
   *
   * \param[out] flagp set to a non-zero value if PiP execution mode is
   * Pthread
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM The PiP library is not initialized yet
   *
   * \sa pip_init
   *
   * \author Atsushi Hori
   */
  int pip_is_threaded( int *flagp );
  /** @} */

  /**
   * \defgroup pip_is_shared_fd pip_is_shared_fd
   * @{ */
  /**
   * \description
   * a predicate if the FDs (file descriptors) are shared among PiP
   * tasks or not
   *
   * \param[out] flagp set to a non-zero value if FDs are shared
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM The PiP library is not initialized yet
   *
   * \sa pip_init
   *
   * \author Atsushi Hori
   */
  int pip_is_shared_fd( int *flagp );
  /** @} */
  /** @} */

  /**
   * \defgroup PiP-API5-exit API: termination and signaling
   * @{
   */

  /**
   * \defgroup pip_exit pip_exit
   * @{ */
  /**
   * \description
   * When the main function or the start function of a PiP task
   * returns with an integer value, then it has the same effect of
   * calling \ref pip_exit with the returned value.
   *
   * \param[in] status This status is returned to PiP root.
   *
   * \note
   * This function can be used regardless to the PiP execution mode.
   * \p exit(3) is called in the process mode and \p pthread_exit(3)
   * is called in the pthread mode.
   *
   * \sa pip_wait
   * \sa pip_trywait
   * \sa pip_wait_any
   * \sa pip_trywait_any
   * \sa exit(Linux 3)
   * \sa pthread_exit(Linux 3)
   *
   * \author Atsushi Hori
   */
  void pip_exit( int status );
  /** @} */

  /**
   * \defgroup pip_kill_all_child_tasks pip_kill_all_child_tasks
   * @{ */
  /**
   * \description
   * kill all PiP tasks (excluding PiP root)
   *
   * \note
   * This function must be called from PiP root.
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM The PiP library is not initialized yet or not called from root
   *
   * \author Atsushi Hori
   */
  int pip_kill_all_child_tasks( void );
  /** @} */

  /**
   * \defgroup pip_abort pip_abort
   * @{ */
  /**
   * \description Kill all PiP tasks and then kill PiP root by the
   * SIGABRT signal.
   *
   * \author Atsushi Hori
   */
  void pip_abort(void);
  /** @} */

  /**
   * \defgroup pip_kill pip_kill
   * @{ */
  /**
   * \description 
   * deliver a signal to PiP task
   *
   * \param[out] pipid PiP ID of a target PiP task to deliver the signal
   * \param[out] signal signal number to be delivered
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not yet initialized
   * \retval EINVAL An invalid signal number or invalid PiP ID is
   * specified
   *
   * \sa tkill(Luinux 2)
   *
   * \author Atsushi Hori
   */
  int pip_kill( int pipid, int signal );
  /** @} */

  /**
   * \defgroup pip_sigmask pip_sigmask
   * @{ */
  /**
   * \description
   * set signal mask of the current PiP task
   * 
   * \note
   * This function is agnostic to the PiP execution mode.
   *
   * \param[in] how see \b sigprogmask or \b pthread_sigmask
   * \param[in] sigmask signal mask
   * \param[out] oldmask old signal mask
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not yet initialized
   * \retval EINVAL An invalid signal number or invalid PiP ID is
   * specified
   *
   * \sa sigprocmask(Linux 2)
   * \sa pthread_sigmask(Linux 3)
   *
   * \author Atsushi Hori
   */
  int pip_sigmask( int how, const sigset_t *sigmask, sigset_t *oldmask );
  /** @} */

  /**
   * \defgroup pip_signal_wait pip_signal_wait
   * @{ */
  /**
   * \description
   * This function is agnostic to the PiP execution mode.
   *
   * \param[in] signal signal to wait
   *
   * \return Return 0 on success. Return an error code on error.
   *
   * \note This function does NOT return the \p EINTR error. This case
   * is treated as normal return;
   *
   * \sa sigwait(Linux 3)
   * \sa sigsuspend(Linux 2)
   *
   * \author Atsushi Hori
   */
  int pip_signal_wait( int signal );
  /** @} */
  /** @} */

  /**
   * \defgroup PiP-API6-sync API: Synchronization
   * @{
   */

  /**
   * \defgroup pip_yield pip_yield
   * @{ */
  /**
   * \description yield
   *
   * \param[in] flag to specify the behavior of yielding. Unused and
   * reserved for BLT/ULP (in PiP-v3)
   *
   * \return Thuis function always succeeds and returns zero.
   *
   * \sa sched_yield(Linux 2)
   * \sa pthread_yield(Linux 3)
   *
   * \author Atsushi Hori
   */
  int pip_yield( int flag );
  /** @} */

  /**
   * \defgroup pip_barrier_init pip_barrier_init
   * @{ */
  /**
   * \description 
   * initialize barrier synchronization structure
   *
   * \param[in] barrp pointer to a PiP barrier structure
   * \param[in] n number of participants of this barrier synchronization
   *
   * \return Return 0 on success. Return an error code on error.
   *
   * \retval EPERM PiP library is not yet initialized or already
   * finalized
   * \retval EINAVL \c n is invalid
   *
   * \sa pip_barrier_wait
   * \sa pip_barrier_fin
   *
   * \author Atsushi Hori
   */
  int pip_barrier_init( pip_barrier_t *barrp, int n );
  /** @} */

  /**
   * \defgroup pip_barrier_wait pip_barrier_wait
   * @{ */
  /**
   * \description wait on barrier synchronization
   *
   * \param[in] barrp pointer to a PiP barrier structure
   *
   * \return Return 0 on success. Return an error code on error.
   *
   * \retval EPERM PiP library is not yet initialized or already
   * finalized
   *
   * \sa pip_barrier_init
   * \sa pip_barrier_fin
   *
   * \author Atsushi Hori
   */
  int pip_barrier_wait( pip_barrier_t *barrp );
  /** @} */

  /**
   * \defgroup pip_barrier_fin pip_barrier_fin
   * @{ */
  /**
   * \description
   * finalize barrier synchronization structure
   *
   * \param[in] barrp pointer to a PiP barrier structure
   *
   * \return Return 0 on success. Return an error code on error.
   * \retval EPERM PiP library is not yet initialized or already
   * finalized
   * \retval EBUSY there are some tasks wating for barrier
   * synchronization
   *
   * \sa pip_barrier_init
   * \sa pip_barrier_wait
   *
   * \author Atsushi Hori
   */
  int pip_barrier_fin( pip_barrier_t *barrp );
  /** @} */
  /** @} */

#ifndef DOXYGEN_INPROGRESS

  void *pip_malloc( size_t );
  size_t pip_malloc_usable_size( void* );
  void  pip_free( void* );
  void *pip_calloc( size_t, size_t );
  void *pip_realloc( void*, size_t );
  int   pip_posix_memalign( void**, size_t, size_t );
  void *pip_dlsym_unsafe( void*, const char* );
  void *pip_dlsym( void*, const char* );
  void *pip_dlopen_unsafe( const char*, int );
  int   pip_dlclose( void* );
  void *pip_dlmopen( long , const char*, int );
  char *pip_dlerror( void );
  int   pip_dlinfo( void*, int, void* );

  pid_t  pip_gettid( void );
  void   pip_libc_lock( void );
  void   pip_libc_unlock( void );
  void   pip_universal_lock( void );
  void   pip_universal_unlock( void );
  void   pip_debug_info( void );
  size_t pip_idstr( char*, size_t );

  void pip_info_fmesg( FILE *fp, const char *format, ... )
    __attribute__((format (printf, 2, 3)));
  void pip_info_mesg( const char *format, ... )
    __attribute__((format (printf, 1, 2)));
  void pip_err_mesg( const char *format, ... )
    __attribute__((format (printf, 1, 2)));
  void pip_warn_mesg( const char *format, ... )
    __attribute__((format (printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* DOXYGEN */

#endif	/* _pip_h_ */
