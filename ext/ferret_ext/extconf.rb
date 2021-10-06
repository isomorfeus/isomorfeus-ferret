require 'mkmf'

$CFLAGS << ' -O2 -W -Wall -Wno-unused-parameter -Wbad-function-cast -Wuninitialized -g '

create_makefile('ferret_ext')
