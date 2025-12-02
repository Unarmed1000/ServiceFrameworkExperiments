# Wrapper for Conan toolchain that sets up configuration mappings
# Conan only provides Debug and Release, so we map the other configurations

# Set up configuration mapping before including Conan toolchain
# This maps RelWithDebInfo and MinSizeRel to use Release libraries (with fallbacks)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL "Release;RelWithDebInfo;MinSizeRel;Debug" CACHE STRING "" FORCE)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Release;RelWithDebInfo;MinSizeRel;Debug" CACHE STRING "" FORCE)

# Also map for Python interpreter (used by CMake scripts)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL_INTERP "Release;Debug" CACHE STRING "" FORCE)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO_INTERP "Release;Debug" CACHE STRING "" FORCE)

# Determine the Conan toolchain location based on generator
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CONAN_TOOLCHAIN "${CMAKE_CURRENT_LIST_DIR}/../build/windows-vs2026/build/generators/conan_toolchain.cmake")
elseif(CMAKE_GENERATOR MATCHES "Ninja")
    if(WIN32)
        set(CONAN_TOOLCHAIN "${CMAKE_CURRENT_LIST_DIR}/../build/windows-ninja/build/generators/conan_toolchain.cmake")
    else()
        set(CONAN_TOOLCHAIN "${CMAKE_CURRENT_LIST_DIR}/../build/ubuntu-ninja/build/generators/conan_toolchain.cmake")
    endif()
endif()

# Include the actual Conan toolchain if it exists
if(EXISTS "${CONAN_TOOLCHAIN}")
    include("${CONAN_TOOLCHAIN}")
else()
    message(STATUS "Conan toolchain not found at: ${CONAN_TOOLCHAIN}")
    message(STATUS "Will be generated during CMake configuration")
endif()
