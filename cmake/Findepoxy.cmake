find_path(EPOXY_INCLUDE_DIRS NAMES epoxy/gl.h PATH_SUFFIX include)
find_library(EPOXY_LIBRARIES NAMES epoxy_0 PATH_SUFFIX)

if(EPOXY_LIBRARIES AND EPOXY_INCLUDE_DIRS)
    set(epoxy_FOUND TRUE)
endif()

if (WIN32 AND NOT epoxy_FOUND)
    message(FATAL_ERROR "Windows requires epoxy module!")
endif()
