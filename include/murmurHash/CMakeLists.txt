cmake_minimum_required (VERSION 3.6)

# List of C++ features used.
# Should be `cxx_std_11`, but this style specification was introduced from `CMake` 3.8
set (CXX_FEATURES_USED cxx_auto_type
                       cxx_defaulted_functions
                       cxx_generalized_initializers
                       cxx_variadic_templates)

add_library (murmurHash STATIC ${CMAKE_CURRENT_SOURCE_DIR}/MurmurHash3.cpp)
target_compile_features (murmurHash PUBLIC ${CXX_FEATURES_USED})
target_include_directories (murmurHash PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                            $<INSTALL_INTERFACE:include>)
