ADD_SUBDIRECTORY(ch_grid)

set(HUMMINGBIRD_JSON_INCLUDES
    "${CACHALOT_CONFIGURATION_DIRECTORY}/json/include"
    "${CACHALOT_DEPENDENCIES_SOURCE_DIRECTORY}/json/include"
    CACHE STRING "CACHALOT_JSON_INCLUDES" FORCE)
    
set(HUMMINGBIRD_LIBEVENT_INCLUDES
    "${CACHALOT_CONFIGURATION_DIRECTORY}/libevent/include"
    "${CACHALOT_DEPENDENCIES_SOURCE_DIRECTORY}/libevent/include"
    CACHE STRING "HUMMINGBIRD_LIBEVENT_INCLUDES" FORCE)

set(HUMMINGBIRD_LIBRARY_ARCHIVE OFF CACHE BOOL "HUMMINGBIRD_LIBRARY_ARCHIVE" FORCE)
set(HUMMINGBIRD_LIBRARY_CACHE OFF CACHE BOOL "HUMMINGBIRD_LIBRARY_CACHE" FORCE)
set(HUMMINGBIRD_LIBRARY_DB OFF CACHE BOOL "HUMMINGBIRD_LIBRARY_DB" FORCE)
set(HUMMINGBIRD_LIBRARY_HTTP ON CACHE BOOL "HUMMINGBIRD_LIBRARY_HTTP" FORCE)
set(HUMMINGBIRD_LIBRARY_JSON ON CACHE BOOL "HUMMINGBIRD_LIBRARY_JSON" FORCE)
set(HUMMINGBIRD_LIBRARY_STORAGE OFF CACHE BOOL "HUMMINGBIRD_LIBRARY_STORAGE" FORCE)

ADD_SUBDIRECTORY(${CACHALOT_DEPENDENCIES_SOURCE_DIRECTORY}/hummingbird ${CACHALOT_CONFIGURATION_DIRECTORY}/hummingbird)