set(LIBMOBILE_SOURCES commands.c commands.h debug_cmd.h mobile.c mobile.h serial.c serial.h weak_defs.c)
add_library(libmobile STATIC ${LIBMOBILE_SOURCES})
