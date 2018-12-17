cmake_minimum_required(VERSION 3.0)
project(athanor)

if ("${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
message(FATAL_ERROR "**Error: do not invoke cmake from the root directory.  Create a build directory and run cmake from their.
Alternatively, run `make` for an automated build.
**Warning, you may need to clean some files first.  To do this, run the following command from the project root:
rm -fr CMakeFiles CMakeCache.txt")

endif()

#libs
#auto arg parse

add_subdirectory (include/auto-arg-parse)
add_subdirectory (include/murmurHash)
add_subdirectory (include/variant)
add_subdirectory (include/Optional)
include_directories( include/PicoSHA2)
include_directories( include/json)
#allow includes relative to src
include_directories( src )

#use c++ 14
set(CMAKE_CXX_STANDARD 14)
message("")
message("")
#Configure CCache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
        message("Found ccache on the system.  Using ccache to speed up recompilations.")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

#git revision
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/getGitRevision/")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/getGitRevision/gitRevision.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/gitRevision.cpp" @ONLY)


#find sources
file(GLOB_RECURSE sources      src/*.cpp src/*.h src/*.hpp "${CMAKE_CURRENT_BINARY_DIR}/gitRevision.cpp")



####
#exec
    add_executable(athanor ${sources})
    set(athanorBuildTarget "athanor")

#flags
if(FLAGS)
    message("You are currently overriding the default flags.  To revert to the default, run cmake . -UFLAGS")
endif()

#release flags
#choosing not to inherit the previous release flags as don't want -ndebug 
if(FLAGS)
set(CMAKE_CXX_FLAGS_RELEASE "${FLAGS}")
else()
set(CMAKE_CXX_FLAGS_RELEASE "-Wextra -Wall -O3 ")
endif()

#debug flags
if(FLAGS)
set(CMAKE_CXX_FLAGS_DEBUG "${FLAGS}")
else()
set(CMAKE_CXX_FLAGS_DEBUG "-Wextra -Wall  -DDEBUG_MODE=1 -g")
endif()

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
    elseif("${CMAKE_BUILD_TYPE}" STREQUAL "debug")
        message("To switch to a release build run ``cmake . -DCMAKE_BUILD_TYPE=release''")
        message("")
        message("Using flags: ${CMAKE_CXX_FLAGS_DEBUG}")
    else()
        message( FATAL_ERROR "CMAKE_BUILD_TYPE must be \"release\" or \"debug\".  Received: \"${CMAKE_BUILD_TYPE}\".  Leave CMAKE_BUILD_TYPE unspecified to have it default to release." )
endif()
if(NOT FLAGS)
message("To override these flags, run cmake . -DFLAGS='put flags here'")
endif() 
message("")

target_link_libraries(athanor mpark_variant)
target_link_libraries(athanor optional)
target_link_libraries (athanor autoArgParse)
target_link_libraries (athanor murmurHash)