#!/usr/bin/python3
from mkpy.utility import *

modes = {
        'debug': '-O0 -g -Wall',
        'profile_debug': '-O3 -g -pg -Wall',
        'release': '-O3 -g -DNDEBUG -Wall'
        }
mode = store('mode', get_cli_arg_opt('-M,--mode', modes.keys()), 'debug')
C_FLAGS = modes[mode]

ensure_dir('bin')

def default ():
    target = store_get('last_snip', default='scrapbook')
    call_user_function(target)

def scrapbook ():
    ex('gcc {C_FLAGS} -o bin/scrapbook scrapbook.c -mavx -maes -lm')

if __name__ == "__main__":
    # Everything above this line will be executed for each TAB press.
    # If --get_completions is set, handle_tab_complete() calls exit().
    handle_tab_complete()

    pymk_default()

