require 'mkmf'

$CFLAGS << ' -O2 -Wall '

create_makefile('isomorfeus_ferret_ext')
