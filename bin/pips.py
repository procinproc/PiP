#!/bin/sh
# -*- mode:python -*-

# $PIP_license: <Simplified BSD License>
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#     Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
# $
# $RIKEN_copyright: Riken Center for Computational Sceience (R-CCS),
# System Software Development Team, 2016-2022
# $
# PIP_VERSION: Version 2.4.1
#
# $Author: Atsushi Hori
# Query:   procinproc-info@googlegroups.com
# User ML: procinproc-users@googlegroups.com
# $


# Comments below are for Doxygen

## \addtogroup PiP-Commands PiP Commands
# @{
# \defgroup pips pips
# @{
#
# \brief list or kill running PiP tasks
#
# \synopsis
# pips [PIPS-OPTIONS] [-] [PATTERN ..]
# 
# \param "[a][u][x]" same with the 'aux' options of the Linux ps command
# \param "--root" List PiP root(s)
# \param "--task" List PiP task(s)
# \param "--family" List PiP root(s) and PiP task(s) in family order
# \param "--kill" Send SIGTERM to PiP root(s) and task(s)
# \param "--signal" Send a signal to PiP root(s) and task(s). This
# option must be followed by a signal number of name.
# \param "--ps" Run the ps Linux command. This option may have \c ps
# command option(s) separated by comma (,)
# \param "--top" Run the top Linux command. This option may have
# \c top command option(s) separated by comma (,)
# \param "-" Simply ignored. This option can be used to avoid the
# ambiguity of the options and patterns.
# 
# \description
# \c pips is a filter to target only PiP tasks (including PiP root)
# to show status like the way what the \c ps commands does and send
# signals to the selected PiP tasks.
# 
# Just like the \c ps command, \c pips can take the most familiar
# \c ps options \c a, \c u, \c x. Here is an example;
# 
# \verbatim
# $ pips
# PID   TID   TT       TIME     PIP COMMAND
# 18741 18741 pts/0    00:00:00 RT  pip-exec
# 18742 18742 pts/0    00:00:00 RG  pip-exec
# 18743 18743 pts/0    00:00:00 RL  pip-exec
# 18741 18744 pts/0    00:00:00 0T  a
# 18745 18745 pts/0    00:00:00 0G  b
# 18746 18746 pts/0    00:00:00 0L  c
# 18747 18747 pts/0    00:00:00 1L  c
# 18741 18748 pts/0    00:00:00 1T  a
# 18749 18749 pts/0    00:00:00 1G  b
# 18741 18750 pts/0    00:00:00 2T  a
# 18751 18751 pts/0    00:00:00 2G  b
# 18741 18752 pts/0    00:00:00 3T  a
# \endverbatim
# 
# here, there are 3 \c pip\-exec root processes running. Four pip
# tasks running program 'a' with the ptherad mode, three PiP tasks
# running program 'b' with the process:got mode, and two PiP tasks
# running program 'c' with the process:preload mode.
# 
# Unlike the \c ps command, two columns 'TID' and 'PIP' are added. The
# 'TID' field is to identify PiP tasks in pthread execution mode.
# three PiP tasks running in the pthread mode. As for the 'PiP'
# field, if the first letter is 'R' then that pip task is running as a
# PiP root. If this letter is
# a number from '0' to '9' then this is a PiP task (not root).
# The number is the least\-significant
# digit of the PiP ID of that PiP task. The second letter represents
# the PiP execution mode which is common with PiP root and task. 'L' is
# 'process:preload,' 'C' is 'process:pipclone,', 'G' is 'process:got' (this is obsolete)
# and 'T' is 'thread.'
# 
# The last 'COMMAND' column of the
# \c pips output may be different from what the \c ps command shows,
# although it looks the same. It represents the command, not the
# command line consisting of a command and its argument(s). More
# precisely speaking, it is the first 14 letters of the command. This comes
# from the PiP's specificity. PiP tasks are not created by using the
# normal \c exec systemcall and the Linux assumes the same command
# line with the pip root process which creates the pip tasks.
# 
# If users want to have the other \c ps command options other than
# 'aux', then refer to the \c \-\-ps option described below. But in this
# case, the command lines of PiP tasks (excepting PiP roots) are not
# correct.
# 
# \li \c \-\-root (\c \-r) Only the PiP root tasks will be shown.
# \verbatim
# $ pips --root
# PID   TID   TT       TIME     PIP COMMAND
# 18741 18741 pts/0    00:00:00 RT  pip-exec
# 18742 18742 pts/0    00:00:00 RG  pip-exec
# 18743 18743 pts/0    00:00:00 RL  pip-exec
# \endverbatim
# 
# \li \c \-\-task (\c \-t) Only the PiP tasks (excluding root) will
# be shown. If both of \c \-\-root and \c \-\-task are specified,
# then firstly PiP roots are shown and then PiP tasks will be
# shown.
# \verbatim
# $ pips --tasks
# PID   TID   TT       TIME     PIP COMMAND
# 18741 18744 pts/0    00:00:00 0T  a
# 18745 18745 pts/0    00:00:00 0G  b
# 18746 18746 pts/0    00:00:00 0L  c
# 18747 18747 pts/0    00:00:00 1L  c
# 18741 18748 pts/0    00:00:00 1T  a
# 18749 18749 pts/0    00:00:00 1G  b
# 18741 18750 pts/0    00:00:00 2T  a
# 18751 18751 pts/0    00:00:00 2G  b
# 18741 18752 pts/0    00:00:00 3T  a
# \endverbatim
# \li \c \-\-family (\c \-f) All PiP roots and tasks of the selected
# PiP tasks by the \c PATTERN optional argument of \c pips.
# \verbatim
# $ pips - a
# PID   TID   TT       TIME     PIP COMMAND
# 18741 18744 pts/0    00:00:00 0T  a
# 18741 18748 pts/0    00:00:00 1T  a
# 18741 18750 pts/0    00:00:00 2T  a
# $ pips --family a
# PID   TID   TT       TIME     PIP COMMAND
# 18741 18741 pts/0    00:00:00 RT  pip-exec
# 18741 18744 pts/0    00:00:00 0T  a
# 18741 18748 pts/0    00:00:00 1T  a
# 18741 18750 pts/0    00:00:00 2T  a
# \endverbatim
# In this example, "pips - a" (the \- is needed not to confused the
# command name \c a as the \c pips option) shows the PiP tasks which
# is derived
# from the program \c a. The second run, "pips --family a," shows the
# PiP tasks of \c a and their PiP root (\c pip-exec, in this example).
#
# \li \c \-\-kill (\c \-k) Send SIGTERM signal to the selected PiP
# tasks.
# \li \c \-\-signal (\c \-s) \c SIGNAL Send the specified signal to
# the selected PiP tasks.
# \li \c \-\-ps (\c \-P) This option may be followed by the \c ps
# command options. When this option is specified, the PIDs of
# selected PiP tasks are passed to the \c ps command with the
# specified \c ps command options, if given.
# \li \c \-\-top (\c \-T) This option may be followed by the \c top
# command options. When this option is specified, the PIDs of
# selected PiP tasks are passed to the \c top command with the
# specified \c top command options, if given.
# \li \c PATTERN The last argument is the pattern(s) to select which PiP
# tasks to be selected and shown. This pattern can be a command name (only
# the first 14 characters are effective), PID, TID, or a Unix (Linux)
# filename matching pattern (if the fnmatch Python module is available).
# \verbatim
# $ pips - *-*
# PID   TID   TT       TIME     PIP COMMAND
# 18741 18741 pts/0    00:00:00 RT  pip-exec
# 18742 18742 pts/0    00:00:00 RG  pip-exec
# 18743 18743 pts/0    00:00:00 RL  pip-exec
# \endverbatim
# 
# \note
# \c pips collects PiP tasks' status by using the Linux's \c ps
# command. When the \c \-\-ps or \c \-\-top option is specified, the \c
# ps or \c top command is invoked after invoking the \c ps command for
# information gathering. This, however, may result some PiP tasks may not
# appear in the invoked \c ps or \c top command when one or more PiP tasks
# finished after the first \c ps command invocation.
# The same situation may also happen with the \c \-\-kill or \-\-signal
# option.
#
# \sa pip-exec
#
#  \author Atsushi Hori
# @}
# @}

# This file is bilingual. The following shell code finds our preferred python.
# Following line is a shell no-op, and starts a multi-line Python comment.
# See https://stackoverflow.com/a/47886254
""":"
#pips: Show status of runnning PiP tasks
# Find a suitable python interpreter (adapt for your specific needs) 
for cmd in python3 python2 python; do
   command -v > /dev/null $cmd && exec $cmd $0 "$@"
done
echo "==> Error: pips could not find a python interpreter!" >&2
exit 1
":"""
# Line above is a shell no-op, and ends a python multi-line comment.
# The code above runs this file with our preferred python interpreter.

from __future__ import print_function
import os
import signal
import sys
import subprocess as sp

try :
    import fnmatch as fnm
    flag_fnmatch = True
except:
    flag_fnmatch = False


wtab = { 'USER':  8,
         'TT':    8,
         'TTY':   8,
         'START': 7 }

cmdself = os.path.basename( sys.argv.pop(0) )

check_gen    = None
check_out    = None
check_output = None

def print_usage( errmsg ):
    "print usage and exit"
    global check_gen, check_out, check_output
    print( cmdself+':', errmsg, file=sys.stderr )
    if check_gen is not None or check_out is not None:
        print( cmdself+':', errmsg, file=check_output )
    print( 'Usage:', cmdself, '[a][u][x] <PIPS_OPTIONS> [<CMDS>]', file=sys.stderr )
    sys.exit( 1 )

def eprint( msg ):
    "error message"
    global check_gen, check_out, check_output
    print( cmdself.upper()+':', msg, file=sys.stderr )
    if check_gen is not None or check_out is not None:
        print( cmdself.upper()+':', msg, file=check_output )

def ex_print( msg ):
    "print error message and exit"
    eprint( msg )
    try:
        if check_out is not None:
            check_output.close()
    except:
        pass
    sys.exit( 1 )

SIGTAB = [ ( 'HUP',   int (signal.SIGHUP  ) ), # 1
           ( 'INT',   int( signal.SIGINT  ) ), # 2
           ( 'QUIT',  int( signal.SIGQUIT ) ), # 3
           ( 'ILL',   int( signal.SIGILL  ) ), # 4
           ( 'TRAP',  int( signal.SIGTRAP ) ), # 5
           ( 'ABRT',  int( signal.SIGABRT ) ), # 6
           ( 'BUS',   int( signal.SIGBUS  ) ), # 7
           ( 'FPE',   int( signal.SIGFPE  ) ), # 8
           ( 'KILL',  int( signal.SIGKILL ) ), # 9
           ( 'USR1',  int( signal.SIGUSR1 ) ), # 10
           ( 'SEGV',  int( signal.SIGSEGV ) ), # 11
           ( 'USR2',  int( signal.SIGUSR2 ) ), # 12
           ( 'PIPE',  int( signal.SIGPIPE ) ), # 13
           ( 'ALRM',  int( signal.SIGALRM ) ), # 14
           ( 'TERM',  int( signal.SIGTERM ) ), # 15
           ( 'CHLD',  int( signal.SIGCHLD ) ), # 17
           ( 'CLD',   int( signal.SIGCHLD ) ), # 17
           ( 'CONT',  int( signal.SIGCONT ) ), # 18
           ( 'STOP',  int( signal.SIGSTOP ) ), # 19
           ( 'TSTP',  int( signal.SIGTSTP ) ), # 20
           ( 'TTIN',  int( signal.SIGTTIN ) ), # 21
           ( 'TTOU',  int( signal.SIGTTOU ) ), # 22
           ( 'URG',   int( signal.SIGURG  ) ), # 23
           ( 'XCPU',  int( signal.SIGXCPU ) ), # 24
           ( 'XFSZ',  int( signal.SIGXFSZ ) ), # 25
           ( 'VTALRM', int( signal.SIGVTALRM ) ), # 26
           ( 'PROF',  int( signal.SIGPROF ) ), # 27
           ( 'WINCH', int( signal.SIGWINCH ) ), # 28
           ( 'IO',    int( signal.SIGIO   ) ), # 29
##         ( 'PWR',   int( signal.SIGPWR )  ), # 30
           ( 'SYS',   int( signal.SIGSYS  ) )  # 31
]

def check_signal( sig ):
    "check if the specified signal is valid or not"
    global SIGTAB
    if sig is None:
        return None
    try:
        signum = int( sig )
        if signum < 0 or signum > int( signal.NSIG ):
            return None
        for ( name, signo ) in SIGTAB:
            if signum == signo:
                return ( name, signo )
        return None
    except ValueError:
        try:
            signame = sig.upper()
            if signame[0:3] == 'SIG':
                signame = signame[3:]
            for ( name, signo ) in SIGTAB:
                if signame == name:
                    return ( name, signo )
            return None
        except KeyError:
            return None

def delimit( lst, delimiter ):
    "flatten list"
    if lst == [] or lst is None:
        return []

    token = lst.pop( 0 )
    if delimiter is not None and delimiter in token:
        return token.split( delimiter ) + delimit( lst, delimiter )
    return [ token ] + delimit( lst, delimiter )

error = False

ps_ax        = ''
flag_bsd_u   = False
flag_root    = True
flag_task    = True
flag_family  = False
thread_mode  = False
flag_kill    = False
flag_verbose = False
flag_debug   = False
flag_descent = False
ps_opts      = None
top_opts     = None
killsig      = None
mode_sel     = None

patterns = []

def is_option( opt ):
    "check if option"
    if opt is None:
        return False
    return opt[0] == '-'

def is_skip( opt ):
    "check if the skip argument"
    if opt is None:
        return True
    return opt in [ '-', '--' ]

check_gen = os.environ.get( 'PIPS_CHECK_GEN' )
check_out = os.environ.get( 'PIPS_CHECK_OUT' )
# for checking
if check_gen is not None and check_out is not None:
    print_usage( '????' )
# generating test data for checking afterwords
if check_gen is not None:
    check_input  = open( check_gen+'.input',  mode='w' )
    check_output = open( check_gen+'.output', mode='w' )
    print( 'simply ignore this line', file=check_input )
if check_out is not None:
    check_output = open( check_out+'.check',  mode='w' )

if len(sys.argv) > 0:
    args = sys.argv
    aux = args[0]

    if aux[0] != '-':
        if 'a' in aux:
            ps_ax = ps_ax + 'a'
        if 'x' in aux:
            ps_ax = ps_ax + 'x'
        if 'u' in aux:
            flag_bsd_u = True
        if ps_ax != '' or flag_bsd_u:
            args.pop(0)

    argc = len(args)
    idx  = 0
    while idx < argc:
        opt = args[idx]
        idx += 1
        if idx < argc:
            nxt = args[idx]
        else:
            nxt = None
        if opt in [ '--root', '-r' ]:
            flag_task = False
        elif opt in [ '--task', '-t' ]:
            flag_root = False
        elif opt in [ '--family', '-f' ]:
            flag_family = True
        elif opt in [ '--descentants', '--descent', '-d' ]:
            flag_descent = True
        elif opt in [ '--mode', '-m' ]:
            if nxt is None:
                print_usage( 'No mode specified' )
            else:
                nxtu = nxt.upper()
                for m in nxtu:
                    if not m in "PLGCT":
                        print_usage( 'Invalid mode' )
                mode_sel = nxtu
            idx += 1
        elif opt in [ '--kill', '-k' ]:
            flag_kill = True
        elif opt in [ '--signal', '-s' ]:
            if nxt is None or is_skip( nxt ) or is_option( nxt ):
                print_usage( 'No signal specified' )
            if killsig is not None: # killsig is already set
                print_usage( 'Multiple signal options' )
            killsig = check_signal( nxt )
            if killsig is None:
                print_usage( 'Invalid signal' )
            idx += 1
        elif opt in [ '--ps', '-P' ]:
            if ps_opts is None:
                ps_opts = []
            if not is_skip( nxt ):
                ps_opts = ps_opts + nxt.split( ',' )
                idx += 1
        elif opt in [ '--top', '-T' ]:
            if top_opts is None:
                top_opts = []
            if not is_skip( nxt ):
                top_opts = top_opts + nxt.split( ',' )
                idx += 1
        elif opt in [ '--verbose', '-v' ]:
            flag_verbose = True
        elif opt == '--debug':
            flag_debug = True
        elif is_skip( opt ):
            continue
        elif opt[0] == '-':
            print_usage( 'Unknown option: ' + opt )
        else:
            patterns = args[idx-1:]
            break
        continue

if flag_debug:
    print( 'Selections:', patterns )
    print( 'mode_sel:', mode_sel )

if ps_opts is not None and top_opts is not None:
    ex_print( 'either --ps or --top can be specified' )

if killsig is not None and ps_opts is not None:
    ex_print( 'either --signal or --ps options can be specified' )
if killsig is not None and top_opts is not None:
    ex_print( 'either --signal or --top options can be specified' )
if flag_kill and killsig is not None:
    ex_print( 'either --kill or --signal can be specified' )
if flag_kill and ps_opts is not None:
    ex_print( 'either --kill or --ps can be specified' )
if flag_kill and top_opts is not None:
    ex_print( 'either --kill or --top can be specified' )
if flag_kill or killsig is not None:
    tgkill_path = os.path.abspath( 
        os.path.join( os.path.join( os.getcwd(), os.path.dirname( __file__ ) ),
                      "pip-tgkill" ) )
    if not os.path.exists( tgkill_path ):
        ex_print( tgkill_path + ' not found' )
if flag_kill:
    killsig = ( 'TERM',  int( signal.SIGTERM ) )

pips_postfix = 'comm tid pid ppid'
if flag_bsd_u:
    fmt = 'user pid tid %cpu %mem vsz rss tty stat start_time cputime'
else:
    fmt = 'pid tid tty stat time'
ps_cmd = [ 'ps', ps_ax+'H', '--format' ] + [ fmt + ' ' + pips_postfix ]

if flag_debug:
    print( ps_cmd, file=sys.stderr )

def debug_ps( tag, procs ):
    if flag_debug:
        print( tag )
        for ps in procs:
            print( '\t', ps )

def byte_to_char( byte_str ):
    "convert byte string to char (for compatibility between Python2 and 3)"
    try:
        if byte_str == '' or byte_str[0] in 'abc':
            ch_str = byte_str   # python2
        else:
            ch_str = byte_str
    except:                     # python3
        ch_str = byte_str.decode()
    return ch_str

def get_command( psline ):
    "get command from the ps output line"
    return psline[-4]

def get_tid( psline ):
    "get TID from the ps output line"
    return psline[-3]

def get_pid( psline ):
    "get PID from the ps output line"
    return psline[-2]

def get_ppid( psline ):
    "get PPID from the ps output line"
    return psline[-1]

def format_header( header ):
    "re-format the header line of ps output"
    head = header[:-3]
    head.insert( -1, 'PIP' )
    return head

def get_mode( cmd ):
    second = cmd[1]
    if second == ':':
        mode = 'L'
    elif second == ';':
        mode = 'C'
    elif second == '.':
        mode = 'G'
    elif second == '|':
        mode = 'T'
    else:
        mode = '?'
    return mode

def format_psline( psline ):
    "re-format from the ps format to the pips format"
    cmd = get_command( psline )
    first  = cmd[0]
    mode = get_mode( cmd );
    line = psline[:-4]
    line += [ first+mode, cmd[2:] ]
    return line

def pip_symbol( sym ):
    "check if sym is a pip symbol"
    if sym in ":;.|?":
        return True
    return False

def pip_root_symbol( sym ):
    "check if sym is the root symbol"
    if sym == 'R':
        return True
    return False

def pip_task_symbol( sym ):
    "check if sym is the task symbol"
    if sym in '0123456789':
        return True
    return False

def get_mode_symbol( ps ):
    "return PiP execution mode symbol"
    cmd = get_command( ps )
    return cmd[1]

def is_threaded( mode ):
    "check if the execution mode is thread"
    if mode == '|':
        return True
    return False

def isa_piproot( cmd ):
    "check if root task or not"
    if pip_root_symbol( cmd[0] ):
        return True
    return False

def isa_piptask( cmd ):
    "check if the PiP task or not"
    if pip_task_symbol( cmd[0] ):
        return True
    return False

def is_selected( ps, cmd, patterns ):
    "check if the psline matches the patterns"
    if patterns == []:
        return True

    tid = get_tid( ps )
    if tid in patterns:
        return True
    pid = get_pid( ps )
    if pid in patterns:
        return True
    if len(cmd) > 2 and cmd[2:] in patterns:
        return True

    if flag_fnmatch:
        for pat in patterns:
            if fnm.fnmatch( cmd[2:], pat ):
                return True
    return False

def have_pc_relation( parent, child ):
    "check if they have the parent-child relationship"
    mp = get_mode_symbol( parent )
    mc = get_mode_symbol( child  )
    if mp != mc:
        return False
    if is_threaded( mp ):
        if get_pid( parent ) == get_pid( child ):
            return True
        return False
    if get_pid( parent ) == get_ppid( child ):
        return True
    return False

def check_mode( cmd, mode ):
    "check is the mode of the command is the one to be selected"
    m = get_mode( cmd )
    #print( 'check_mode( ', cmd, ' , ', mode, ' , ', m,  ' )' )
    if 'P' in mode:
        if m in 'LCG':
            return True
        else:
            return False
    elif m in mode:
        return True
    else:
        return False

os.environ['LANG'] = 'C'
header  = None
def read_ps_output( fstream ):
    "parse the ps output"
    global thread_mode, header

    ps_output = []
    while True:
        ps = byte_to_char( fstream.readline() )
        if ps == '':
            break
        if check_gen is not None:
            print( ps, end='', file=check_input )

        psl = ps.split()
        if header is None:
            header = psl
        else:
            if psl[-4] is '<defunct>':
                psl = psl[0:-6] + [ psl[-5]+' '+psl[-4] ] + psl[-3:]
            ps_output += [ psl ]
            cmd = get_command( psl )
            if isa_piproot( cmd ) or isa_piptask( cmd ):
                if is_threaded( cmd[1] ):
                    thread_mode = True
        continue
    return ps_output

def find_descents( parents, descents, candidates ):
    par = parents
    can = candidates
    while par != [] and can != []:
        npar = []
        ncan = []
        for pa in par:
            pid = get_pid( pa )
            for ca in can:
                if get_ppid( ca ) == pid:
                    npar += [ ca ]
                else:
                    ncan += [ ca ]
        par = npar
        can = ncan
        descents += par
    return descents


fstream = None
pslines = []
if check_out == None:
    psproc = sp.Popen( ps_cmd, stdout=sp.PIPE )
    fstream = psproc.stdout
    pslines = read_ps_output( fstream )
else:
    fstream = open( check_out+'.input', mode='r' )
    fstream.readline() # skip the very first line
    pslines = read_ps_output( fstream )
debug_ps( 'pslines:', pslines )

sel_procs = []
for ps in pslines:
    cmd = get_command( ps )
    if is_selected( ps, cmd, patterns ):
        sel_procs += [ ps ]
debug_ps( 'sel_procs:', sel_procs )

all_procs = []
if flag_descent:
    all_procs = sel_procs + find_descents( sel_procs, [], pslines )
else:
    all_procs = sel_procs
debug_ps( 'all_procs:', all_procs )

all_roots = []
all_tasks = []
for ps in all_procs:
    cmd = get_command( ps )
    if isa_piproot( cmd ):
        all_roots += [ ps ]
    elif isa_piptask( cmd ):
        all_tasks += [ ps ]
debug_ps( 'all_roots:', all_roots )
debug_ps( 'all_tasks:', all_tasks )

if not flag_family:
    sel_roots = all_roots
    sel_tasks = all_tasks
else:
    fmly_roots = all_roots
    fmly_tasks = all_tasks
    for rs in all_roots:
        for ts in pslines:
            # if rs is multi-threaded, same ts may appaer twice or more
            if have_pc_relation( rs, ts ) and ts not in fmly_tasks:
                fmly_tasks += [ ts ]
    debug_ps( 'fmly_tasks:', fmly_tasks )
    for ts in all_tasks:
        for rs in pslines:
            if have_pc_relation( rs, ts ) and rs not in fmly_roots:
                fmly_roots += [ rs ]
    debug_ps( 'fmly_roots:', fmly_roots )
    for rs in fmly_roots:
        for ts in pslines:
            if have_pc_relation( rs, ts ) and ts not in fmly_tasks:
                fmly_tasks += [ ts ]
    sel_roots = fmly_roots
    sel_tasks = fmly_tasks
debug_ps( 'sel_roots:', sel_roots )
debug_ps( 'sel_tasks:', sel_tasks )

if mode_sel is not None:
    sel = []
    for rs in sel_roots:
        cmd = get_command( rs )
        if check_mode( cmd, mode_sel ):
            sel += [ rs ]
    sel_roots = sel
    sel = []
    for ts in sel_tasks:
        cmd = get_command( ts )
        if check_mode( cmd, mode_sel ):
            sel += [ ts ]
    sel_tasks = sel
debug_ps( 'sel_roots:', sel_roots )
debug_ps( 'sel_tasks:', sel_tasks )

if not flag_root and not flag_task:
    flag_root = True
    flag_task = True
all_pips = []
if flag_root:
    all_pips += sel_roots
if flag_task:
    all_pips += sel_tasks

if all_pips == []:
    ex_print( 'no PiP task found' )

def sort_tid( psline ):
    "sort lines with TID"
    return int( get_tid( psline ) )

all_pips.sort( key=sort_tid )
debug_ps( 'all_pips:', all_pips )

def kill_task( ps, killsig ):
    ( signame, signum ) = killsig
    try:
        cmd = get_command( ps )
        tid = get_tid( ps )
        pid = get_pid( ps )
        if flag_verbose:
            print( 'pip-tgkill ' + signame + ' ' + 
                   pid + ' ' + tid + 
                   ' (' + cmd + ')' )
        if check_gen is None and check_out is None:
            sp.check_call( [ tgkill_path, str(signum), pid, tid, 'no_esrch' ] )
        else:
            print( 'pip-tgkill', signame, tid, cmd, file=check_output )
    except Exception as e:
        print( tgkill_path + ' ', signame + ' ' + 
               pid + ' ' + tid + 
               ' (' + cmd + '): ', e )

if killsig is not None:
    # tasks must be killed at first
    if flag_task:
        for ps in sel_tasks:
            kill_task( ps, killsig );
    if flag_root:
        for ps in sel_roots:
            kill_task( ps, killsig );

elif ps_opts is not None:
    ps_cmd = 'ps'
    ps_uH = ''
    if flag_bsd_u:
        ps_uH += 'u'
    if thread_mode:
        ps_uH += 'H'
    ps_opts = [ ps_ax + ps_uH ] + ps_opts
    pid_list = []
    for ps in all_pips:
        pid = get_pid( ps )
        if not pid in pid_list:
            pid_list += [ pid ]
    try:
        if check_gen is None and check_out is None:
            if flag_debug:
                print( ps_cmd, ps_opts, pid_list )
            os.execvp( ps_cmd, [ps_cmd] + ps_opts + pid_list )
        else:
            print( 'execvp(', ps_cmd, ' '.join(ps_opts+pid_list), ')',
                   file=check_output )
    except Exception as e:
        eprint( 'execvp(ps):' + str(e) )

elif top_opts is not None:
    top_cmd = 'top'
    if thread_mode:
        top_opts += [ '-H' ]
    pid_list = []
    for ps in all_pips:
        pid_list += [ get_pid( ps ) ]
    pid_opt = [ '-p', ','.join( pid_list ) ]
    try:
        if check_gen is None and check_out is None:
            if flag_debug:
                print( ps_cmd, top_opts, pid_opt )
            os.execvp( top_cmd, [top_cmd] + top_opts + pid_opt )
        else:
            print( 'execvp(', top_cmd, ' '.join(top_opts+pid_opt), ')',
                   file=check_output )
    except Exception as e:
        eprint( 'execvp(top):' + str(e) )

else:
    out_list = []
    if flag_family:
        for pr in sel_roots:
            if flag_root:
                out_list += [ pr ]
            if flag_task:
                for pt in all_pips:
                    if have_pc_relation( pr, pt ):
                        out_list += [ pt ]
    else:
        for ps in all_pips:
            out_list += [ ps ]
    # format
    outl = [ format_header( header ) ]
    for out in out_list:
        outl += [ format_psline( out ) ]
    w = []
    for tkn in outl[0]:
        if tkn in wtab:
            w += [ wtab[tkn] + 1 ]
        else:
            w += [ len(tkn) + 1 ]
    for line in outl[1:]:
        idx = 0
        for tkn in line:
            wt = len( tkn ) + 1
            if wt > w[idx]:
                w[idx] = wt
            idx += 1
    # output
    for line in outl:
        idx = 0
        for tkn in line:
            print( tkn.ljust(w[idx]), end='' )
            if check_gen is not None or check_out is not None:
                print( tkn.ljust(w[idx]), end='', file=check_output )
            idx += 1
        print( '' )             # newline
        if check_gen is not None or check_out is not None:
            print( '', file=check_output )

if check_gen is not None or check_out is not None:
    check_output.close()

if error:
    sys.exit( 1 )
sys.exit( 0 )
