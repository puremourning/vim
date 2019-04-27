import os
import logging
import subprocess
import shlex
from ycmd import utils

# Map files to translation units.
file_to_tu = {
  'nbdebug.c': 'netbeans.c',
  'if_py_both.h': 'if_python3.c'
}

# By default we just use ALL_CFLAGS, but some modules require specific flags.
tu_to_flags = {
  'gui_gtk_gresources.c': [ 'PERL_CFLAGS', 'ALL_CFLAGS' ],
  'if_lua.c': [ 'LUA_CFLAGS', 'ALL_CFLAGS' ],
  'if_perl.c': [ 'PERL_CFLAGS', 'ALL_CFLAGS' ],
  'if_perlsfio.c': [ 'PERL_CFLAGS', 'ALL_CFLAGS' ],
  'if_python3.c': [ 'PYTHON3_CFLAGS', 'PYTHON3_CFLAGS_EXTRA', 'ALL_CFLAGS' ],
  'if_python.c': [ 'PYTHON_CFLAGS', 'PYTHON_CFLAGS_EXTRA', 'ALL_CFLAGS' ],
  'if_ruby.c': [ 'RUBY_CFLAGS', 'ALL_CFLAGS' ],
  'if_tcl.c': [ 'TCL_CFLAGS', 'ALL_CFLAGS' ],
  'option.c': [ 'LUA_CFLAGS',
                'PERL_CFLAGS',
                'PYTHON_CFLAGS',
                'PYTHON3_CFLAGS',
                'RUBY_CFLAGS',
                'TCL_CFLAGS',
                'ALL_CFLAGS' ],

}

_logger = logging.getLogger(__name__)


SRC_DIR = os.path.join( os.path.dirname( os.path.abspath( __file__ ) ),
                        'src' )

def GetMacro( m ):
  return utils.ToUnicode(
    subprocess.check_output( [ 'make',
                               '-s',
                               '-C',
                               SRC_DIR,
                               'show_' + m ] ).strip()
   )

def FlagsForFile(file_name, **kwargs):
    base_name = os.path.basename(file_name)
    tu = (os.path.join(os.path.dirname(file_name), file_to_tu[base_name])
          if base_name in file_to_tu else file_name)

    _logger.info('Using tu %s for %s vs %s', tu, base_name, file_name)

    macros = tu_to_flags.get( os.path.basename( tu ), [ 'ALL_CFLAGS' ] )
    _logger.info( "Using macros %s for %s",
                  macros,
                  os.path.basename( tu ) )
    flags = []
    for macro in macros:
      flags.extend( shlex.split( GetMacro( macro ) ) )


    return {
        'include_paths_relative_to_dir': os.path.dirname(tu),
        'override_filename': tu,
        'flags': flags
    }
