PROJECT(cachalot_json C)

ADD_FILTER(
src
    cachalot_json.h
    cachalot_json.c
)

INCLUDE_DIRECTORIES(${CACHALOT_CONFIGURATION_DIRECTORY}/yyjson/include)
INCLUDE_DIRECTORIES(${CACHALOT_DEPENDENCIES_SOURCE_DIRECTORY}/yyjson/src)

ADD_LIBRARY(${PROJECT_NAME} STATIC ${CACHALOT_SOURCE_FILES})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${CACHALOT_DEPENDENCIES_LIBRARY_DIRECTORY}/${CMAKE_STATIC_LIBRARY_PREFIX}yyjson${CMAKE_STATIC_LIBRARY_SUFFIX})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} cachalot_config)