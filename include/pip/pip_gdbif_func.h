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

#ifndef _pip_gdbif_func_h_
#define _pip_gdbif_func_h_

extern struct pip_gdbif_root	*pip_gdbif_root PIP_PRIVATE;

void pip_gdbif_load( struct pip_task* ) PIP_PRIVATE;
void pip_gdbif_exit( struct pip_task*, int ) PIP_PRIVATE;
void pip_gdbif_task_commit( struct pip_task* ) PIP_PRIVATE;
void pip_gdbif_task_new( struct pip_task* ) PIP_PRIVATE;
void pip_gdbif_initialize_root( int ) PIP_PRIVATE;
void pip_gdbif_finalize_task( struct pip_task* ) PIP_PRIVATE;
void pip_gdbif_hook_before( struct pip_task* ) PIP_PRIVATE;
void pip_gdbif_hook_after( struct pip_task* ) PIP_PRIVATE;

#endif /* _pip_gdbif_func_h_ */
