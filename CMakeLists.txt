cmake_minimum_required(VERSION 3.0)
project(sns)

#libs
#auto arg parse

add_subdirectory (include/auto-arg-parse)
add_subdirectory (include/3rdparty/variant)
include_directories( include/PicoSHA2)

#allow includes relative to src
include_directories( src )

#use c++ 14
set(CMAKE_CXX_STANDARD 14)

# Configure CCache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)


#find sources
file(GLOB_RECURSE sources      src/*.cpp src/*.h src/*.inc)



####
#exec or static lib
if(NOT STATIC_LIB)
    add_executable(sns ${sources})
    set(snsBuildTarget "sns")
    message("Configuring for executable build...")
    message("For static library build, run ``cmake . -DSTATIC_LIB=1''")
    message("")
else()
    add_library(sns STATIC ${sources} )
    install(TARGETS sns DESTINATION lib)
    install(FILES src/cInterface/snsC.h DESTINATION include)
    set(snsBuildTarget "sns")
    message("Configuring for static library build...")
    message("For executable build, run ``cmake . -DSTATIC_LIB=0''")
    message("")
endif()


#release flags
#choosing not to inherit the previous release flags as don't want -ndebug 
set(CMAKE_CXX_FLAGS_RELEASE "-Wextra -Wall -O3 ")


#debug flags
set(CMAKE_CXX_FLAGS_DEBUG "-Wextra -Wall  -DDEBUG_MODE=1 -g")

#default build type:
if(NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE release)
endif(NOT CMAKE_BUILD_TYPE)

#print info on build type and flags
message("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
if("${CMAKE_BUILD_TYPE}" STREQUAL "release") 
        message("To switch to a debug build run ``cmake . -DCMAKE_BUILD_TYPE=debug''")
        message("")
        message("Using flags: ${CMAKE_CXX_FLAGS_RELEASE}")
        message("To add a flag, run ``cmake . -DCMAKE_CXX_FLAGS_RELEASE=\"put flags here\"''")
    elseif("${CMAKE_BUILD_TYPE}" STREQUAL "debug")
        message("To switch to a release build run ``cmake . -DCMAKE_BUILD_TYPE=release''")
        message("")
        message("Using flags: ${CMAKE_CXX_FLAGS_DEBUG}")
        message("To add a flag, run ``cmake . -DCMAKE_CXX_FLAGS_DEBUG=\"put flags here\"''")
    else()
        message( FATAL_ERROR "CMAKE_BUILD_TYPE must be \"release\" or \"debug\".  Received: \"${CMAKE_BUILD_TYPE}\".  Leave CMAKE_BUILD_TYPE unspecified to have it default to release." )
endif()
message("")
target_link_libraries(sns mpark_variant)
target_link_libraries (sns autoArgParse-header-only)