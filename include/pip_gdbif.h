/*
  * $RIKEN_copyright:$
  * $PIP_VERSION:$
  * $PIP_license:$
*/
/*
  * Written by Atsushi HORI <ahori@riken.jp>, 2018
*/

#ifndef _pip_gdb_if_h_
#define _pip_gdb_if_h_

enum pip_task_status {
  PIP_GDBIF_STATUS_NULL		= 0, /* just to make sure, there is nothing in thsi struct */
  PIP_GDBIF_STATUS_CREATED	= 1, /* just after pip_spawbn() is called and this sturucture is created */
  /* Note: the oerder of state transition of the next two depends on implementation (option) */
  /* do to rely on the order of the next two. */
  PIP_GDBIF_STATUS_LOADED	= 2, /* just after calling dlmopen */
  PIP_GDBIF_STATUS_SPAWNED	= 3, /* just after calling pthread_create() or clone() */
  PIP_GDBIF_STATUS_TERMINATED	= 4  /* when the task is about to die (killed) */
};

enum pip_task_exec_mode {	/* One of the value (except NULL) is set when this structure is created */
  /* and the value is left unchanged until the structure is put on the free list */
  PIP_GDBIF_EXMODE_NULL		= 0,
  PIP_GDBIF_EXMODE_PROCESS	= 1,
  PIP_GDBIF_EXMODE_THREAD	= 2
};

struct pip_task_gdbif {
  /* double linked list */
  struct pip_task_gdbif *next;
  struct pip_task_gdbif *prev;
  /* pathname of the program */
  char	*pathname;
  /* argc, argv and env */
  int	argc;
  char 	**argv;
  char 	**envv;
  /* handle of the link map */
  void	*handle;
  /* hook function address, these addresses are set when the PiP task gets PIP_GDBIF_STATUS_LOADED */
  void	*hook_before_main;
  void	*hook_after_main;
  /* PID or TID of the PiP task, the value of zero means nothing */
  pid_t	pid;
  /* exit code, this value is set when the PiP task gets PIP_GDBIF_STATUS_TERMINATED */
  int	exit_code;
  /* pip task exec mode */
  enum pip_task_exec_mode	exec_mode;
  /* task status, this is set by PiP lib */
  enum pip_task_status		status;
};

extern struct pip_task_gdbif	*pip_task_gdbif_root;
extern struct pip_task_gdbif	*pip_task_gdbif_freelist;

#endif /* _pip__gdb_if_h_ */