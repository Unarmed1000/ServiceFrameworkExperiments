cmake_minimum_required(VERSION 3.23)

# Set up configuration mapping for multi-config generators
# Map RelWithDebInfo and MinSizeRel to Release since Conan packages only provide Debug and Release
# These must be CACHE variables with FORCE to persist across project() command
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL "Release;RelWithDebInfo;MinSizeRel;Debug" CACHE STRING "Configuration mapping for MinSizeRel" FORCE)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Release;RelWithDebInfo;MinSizeRel;Debug" CACHE STRING "Configuration mapping for RelWithDebInfo" FORCE)

# Check for Python 3
find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "Python 3 is required but not found. Please install Python 3.x")
endif()

# Allow user to specify profile via cache variable or environment variable
set(CONAN_PROFILE "" CACHE STRING "Conan profile to use (e.g., profiles/windows-vs-debug, profiles/windows-ninja-release)")
if(NOT CONAN_PROFILE AND DEFINED ENV{CONAN_PROFILE})
    set(CONAN_PROFILE "$ENV{CONAN_PROFILE}")
endif()

# Determine profile based on platform if not explicitly set
set(PROFILE_AUTO_SELECTED FALSE)
if(NOT CONAN_PROFILE)
    set(PROFILE_AUTO_SELECTED TRUE)
    if(WIN32)
        set(CONAN_PROFILE "profiles/windows-vs-debug")
    elseif(APPLE)
        set(CONAN_PROFILE "profiles/clang-debug")
    else()
        set(CONAN_PROFILE "profiles/gcc-debug")
    endif()

    # Check if profile exists, otherwise use windows-vs-debug as fallback
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${CONAN_PROFILE}")
        set(CONAN_PROFILE "profiles/windows-vs-debug")
    endif()

    message(WARNING
        "No Conan profile explicitly selected! Using auto-selected profile: ${CONAN_PROFILE}\n"
        "To explicitly select a profile, use one of:\n"
        "  - Set cache variable: cmake -DCONAN_PROFILE=profiles/windows-vs-debug ..\n"
        "  - Set environment variable: set CONAN_PROFILE=profiles/windows-vs-debug (Windows) or export CONAN_PROFILE=profiles/windows-vs-debug (Unix)\n"
        "Available profiles: windows-vs-debug, windows-vs-release, windows-ninja-debug, windows-ninja-release, ubuntu-ninja-debug, ubuntu-ninja-release")
else()
    message(STATUS "Using explicitly selected Conan profile: ${CONAN_PROFILE}")
endif()

# Determine output folder based on preset name (from CMAKE_PRESET_NAME if available)
# Default to "default" for backward compatibility
if(DEFINED ENV{CMAKE_PRESET_NAME})
    set(CONAN_OUTPUT_FOLDER "build/$ENV{CMAKE_PRESET_NAME}")
elseif(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CONAN_OUTPUT_FOLDER "build/windows-vs2026")
elseif(CMAKE_GENERATOR MATCHES "Ninja")
    if(WIN32)
        set(CONAN_OUTPUT_FOLDER "build/windows-ninja")
    else()
        set(CONAN_OUTPUT_FOLDER "build/ubuntu-ninja")
    endif()
else()
    set(CONAN_OUTPUT_FOLDER "build/default")
endif()

# Paths for Conan configuration
set(CONAN_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/${CONAN_OUTPUT_FOLDER}/build/generators/conan_toolchain.cmake")
set(CONAN_HASH_FILE "${CMAKE_SOURCE_DIR}/${CONAN_OUTPUT_FOLDER}/.conan_config_hash.json")
set(CONAN_PROFILE_PATH "${CMAKE_SOURCE_DIR}/${CONAN_PROFILE}")

# Determine if this is a multi-config generator
# Note: CMAKE_CONFIGURATION_TYPES is not available before project() command,
# so we detect based on generator name
if(CMAKE_GENERATOR MATCHES "Visual Studio|Xcode|Ninja Multi-Config")
    set(IS_MULTI_CONFIG TRUE)
else()
    set(IS_MULTI_CONFIG FALSE)
endif()

# Check if Conan configuration has changed
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_conan_config.py
        --conanfile ${CMAKE_SOURCE_DIR}/conanfile.txt
        --profile ${CONAN_PROFILE_PATH}
        --hash-file ${CONAN_HASH_FILE}
        --generator "${CMAKE_GENERATOR}"
        --multi-config ${IS_MULTI_CONFIG}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CONAN_CONFIG_CHECK
    OUTPUT_VARIABLE CONAN_CHECK_OUTPUT
    ERROR_VARIABLE CONAN_CHECK_ERROR
)

# Exit code handling: 0 = up-to-date, 1 = needs reinstall, 2 = error
if(CONAN_CONFIG_CHECK EQUAL 2)
    message(FATAL_ERROR "Conan configuration check failed:\n${CONAN_CHECK_ERROR}")
endif()

# Auto-run Conan install if config changed or toolchain doesn't exist
set(NEED_CONAN_INSTALL FALSE)
if(CONAN_CONFIG_CHECK EQUAL 1)
    message(STATUS "Conan configuration has changed")
    set(NEED_CONAN_INSTALL TRUE)
elseif(NOT EXISTS "${CONAN_TOOLCHAIN_FILE}")
    message(STATUS "Conan toolchain not found")
    set(NEED_CONAN_INSTALL TRUE)
endif()

if(NEED_CONAN_INSTALL)
    message(STATUS "Running Conan install...")

    # Verify Conan is available
    find_program(CONAN_EXECUTABLE conan)
    if(NOT CONAN_EXECUTABLE)
        message(FATAL_ERROR "Conan not found. Please install Conan (pip install conan) and ensure it's in PATH.")
    endif()

    # For multi-config generators (Visual Studio, Xcode), install all configurations
    if(CMAKE_GENERATOR MATCHES "Visual Studio|Xcode|Ninja Multi-Config")
        # Only install Debug and Release since Conan packages don't support RelWithDebInfo/MinSizeRel
        set(SUPPORTED_CONFIGS "Debug;Release")
        message(STATUS "Installing Conan dependencies for configurations: ${SUPPORTED_CONFIGS}")

        set(CONFIGS_TO_INSTALL)

        # Check which configurations need installation by looking for marker files
        foreach(CONFIG_TYPE ${SUPPORTED_CONFIGS})
            set(CONFIG_MARKER "${CMAKE_SOURCE_DIR}/${CONAN_OUTPUT_FOLDER}/build/.conan_installed_${CONFIG_TYPE}")

            if(NOT EXISTS "${CONFIG_MARKER}")
                list(APPEND CONFIGS_TO_INSTALL ${CONFIG_TYPE})
            endif()
        endforeach()

        # If any configs are missing or hash changed, install all (safer for consistency)
        if(CONAN_CONFIG_CHECK EQUAL 1)
            set(CONFIGS_TO_INSTALL ${SUPPORTED_CONFIGS})
            message(STATUS "Configuration changed, reinstalling all build types")
        endif()

        if(CONFIGS_TO_INSTALL)
            # Install all configurations first before creating any marker files
            foreach(CONFIG_TYPE ${CONFIGS_TO_INSTALL})
                message(STATUS "Installing Conan dependencies for ${CONFIG_TYPE}...")

                # Determine the profile for this configuration
                string(TOLOWER ${CONFIG_TYPE} CONFIG_LOWER)
                string(REPLACE "${CMAKE_SOURCE_DIR}/profiles/" "" PROFILE_BASE ${CONAN_PROFILE})
                string(REGEX REPLACE "-debug$|-release$" "" PROFILE_PREFIX ${PROFILE_BASE})

                # Map configuration types to profile suffixes
                if(CONFIG_TYPE STREQUAL "Debug")
                    set(CONFIG_PROFILE "${CMAKE_SOURCE_DIR}/profiles/${PROFILE_PREFIX}-debug")
                else()
                    set(CONFIG_PROFILE "${CMAKE_SOURCE_DIR}/profiles/${PROFILE_PREFIX}-release")
                endif()

                # Fallback to original profile if specific one doesn't exist
                if(NOT EXISTS "${CONFIG_PROFILE}")
                    set(CONFIG_PROFILE "${CONAN_PROFILE_PATH}")
                endif()

                execute_process(
                    COMMAND conan install .
                        --output-folder=${CONAN_OUTPUT_FOLDER}
                        --profile=${CONFIG_PROFILE}
                        --build=missing
                        -s build_type=${CONFIG_TYPE}
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    RESULT_VARIABLE CONAN_RESULT
                    OUTPUT_VARIABLE CONAN_OUTPUT
                    ERROR_VARIABLE CONAN_ERROR
                )

                if(NOT CONAN_RESULT EQUAL 0)
                    message(FATAL_ERROR "Conan install failed for ${CONFIG_TYPE}:\n${CONAN_ERROR}")
                endif()
            endforeach()

            # Create all marker files only after ALL installs succeed
            foreach(CONFIG_TYPE ${CONFIGS_TO_INSTALL})
                set(CONFIG_MARKER "${CMAKE_SOURCE_DIR}/${CONAN_OUTPUT_FOLDER}/build/.conan_installed_${CONFIG_TYPE}")
                file(WRITE "${CONFIG_MARKER}" "${CONFIG_TYPE} installed at ${CMAKE_CURRENT_LIST_FILE}")
            endforeach()
        else()
            message(STATUS "All configurations already installed")
        endif()
    else()
        # Single-config generator (Ninja, Unix Makefiles, etc.)
        execute_process(
            COMMAND conan install .
                --output-folder=${CONAN_OUTPUT_FOLDER}
                --profile=${CONAN_PROFILE}
                --build=missing
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE CONAN_RESULT
            OUTPUT_VARIABLE CONAN_OUTPUT
            ERROR_VARIABLE CONAN_ERROR
        )

        if(NOT CONAN_RESULT EQUAL 0)
            message(FATAL_ERROR "Conan install failed:\n${CONAN_ERROR}")
        endif()
    endif()

    message(STATUS "Conan dependencies installed successfully")

    # Update hash file after successful install
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_conan_config.py
            --conanfile ${CMAKE_SOURCE_DIR}/conanfile.txt
            --profile ${CONAN_PROFILE_PATH}
            --hash-file ${CONAN_HASH_FILE}
            --generator "${CMAKE_GENERATOR}"
            --multi-config ${IS_MULTI_CONFIG}
            --update-hash
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE CONAN_HASH_UPDATE
        OUTPUT_VARIABLE CONAN_HASH_OUTPUT
        ERROR_VARIABLE CONAN_HASH_ERROR
    )

    if(NOT CONAN_HASH_UPDATE EQUAL 0)
        message(WARNING "Failed to update Conan configuration hash:\n${CONAN_HASH_ERROR}")
    endif()
endif()

# Map non-standard configurations to Conan-supported configurations
# Conan only provides Debug and Release, not RelWithDebInfo or MinSizeRel
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)

project(ServiceFramework2025
    VERSION 0.1.0
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Deferred VS Code IntelliSense update (runs after all configuration)
function(_update_vscode_intellisense)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(NOT Python3_Interpreter_FOUND)
        # Check if running in VS Code
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env
            OUTPUT_VARIABLE ENV_VARS
            ERROR_QUIET
        )
        if(ENV_VARS MATCHES "VSCODE_PID|VSCODE_IPC_HOOK|TERM_PROGRAM=vscode")
            message(WARNING "VS Code detected but Python 3 not found. IntelliSense configuration will not be auto-generated.")
            message(WARNING "Install Python 3.x to enable automatic IntelliSense updates.")
        endif()
        return()
    endif()

    # Detect VS Code environment
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/detect_vscode.py ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE VSCODE_DETECTED
        ERROR_QUIET
    )

    # Update IntelliSense configuration
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/update_vscode_includes.py
            --workspace-root ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE UPDATE_RESULT
        OUTPUT_VARIABLE UPDATE_OUTPUT
        ERROR_VARIABLE UPDATE_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    # Context-aware messaging
    if(VSCODE_DETECTED EQUAL 0)
        # Running in VS Code
        if(UPDATE_RESULT EQUAL 0)
            message(STATUS "VS Code IntelliSense: ${UPDATE_OUTPUT}")
        elseif(UPDATE_RESULT EQUAL 2)
            # No changes, silent (don't clutter output)
        else()
            message(WARNING "Failed to update VS Code IntelliSense: ${UPDATE_ERROR}")
        endif()
    endif()
    # Not in VS Code: silent (avoid confusing users in CI/command-line builds)
endfunction()

cmake_language(DEFER CALL _update_vscode_intellisense)

# Find dependencies installed by Conan
find_package(Boost REQUIRED COMPONENTS system)
find_package(fmt REQUIRED)
find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)

# Common compile settings function
function(configure_target target_name)
    target_link_libraries(${target_name}
        PRIVATE
            Boost::boost
            Boost::system
            fmt::fmt
            spdlog::spdlog
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()# Executable 1: Main application
add_executable(main_app src/main.cpp)
configure_target(main_app)

# Executable 2: Test program 1 - Service Framework (Google Test)
add_executable(test1
    UnitTest/Test1/ServiceFrameworkTest.cpp
    include/Common/ComplexData.hpp
    include/Test1/ServiceBase.hpp
    include/Test1/AddService.hpp
    include/Test1/SubtractService.hpp
    include/Test1/MultiplyService.hpp
    include/Test1/DivideService.hpp
    include/Test1/CalculatorService.hpp
    include/Test1/ComplexService.hpp
)
configure_target(test1)
target_include_directories(test1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test1 PRIVATE GTest::gtest GTest::gtest_main)

# Organize files into folders for Visual Studio
source_group("Header Files\\Common" FILES include/Common/ComplexData.hpp)
source_group("Header Files\\Test1" FILES
    include/Test1/ServiceBase.hpp
    include/Test1/AddService.hpp
    include/Test1/SubtractService.hpp
    include/Test1/MultiplyService.hpp
    include/Test1/DivideService.hpp
    include/Test1/CalculatorService.hpp
    include/Test1/ComplexService.hpp
)
source_group("Source Files\\UnitTest\\Test1" FILES UnitTest/Test1/ServiceFrameworkTest.cpp)

# Executable 3: Test program 2
add_executable(test2
    src/test2.cpp
    src/Test2/Framework/Provider/ServiceProvider.cpp
    include/Test2/Framework/Service/IService.hpp
    include/Test2/Framework/Service/IServiceControl.hpp
    include/Test2/Framework/Service/IServiceFactory.hpp
    include/Test2/Framework/Service/ServiceCreateInfo.hpp
    include/Test2/Framework/Service/ServiceDescription.hpp
    include/Test2/Framework/Service/ServiceInitResult.hpp
    include/Test2/Framework/Service/ServiceProcessResult.hpp
    include/Test2/Framework/Service/ServiceShutdownResult.hpp
    include/Test2/Framework/Service/Async/AsyncServiceBase.hpp
    include/Test2/Framework/Provider/IServiceProvider.hpp
    include/Test2/Framework/Provider/ServiceProvider.hpp
    include/Test2/Framework/Provider/ServiceProviderProxy.hpp
    include/Test2/Framework/Registry/IServiceRegistry.hpp
    include/Test2/Framework/Registry/ServiceLaunchPriority.hpp
    include/Test2/Framework/Registry/ServiceThreadGroupId.hpp
    include/Test2/Framework/Registry/ServiceRegistry.hpp
    include/Test2/Framework/Registry/ServiceRegistrationRecord.hpp
    include/Test2/Services/ServiceConfig.hpp
    include/Test2/Services/Add/IAddService.hpp
    include/Test2/Services/Add/AddService.hpp
    include/Test2/Services/Add/AddServiceFactory.hpp
    include/Test2/Services/Subtract/ISubtractService.hpp
    include/Test2/Services/Subtract/SubtractService.hpp
    include/Test2/Services/Subtract/SubtractServiceFactory.hpp
    include/Test2/Services/Multiply/IMultiplyService.hpp
    include/Test2/Services/Multiply/MultiplyService.hpp
    include/Test2/Services/Multiply/MultiplyServiceFactory.hpp
    include/Test2/Services/Divide/IDivideService.hpp
    include/Test2/Services/Divide/DivideService.hpp
    include/Test2/Services/Divide/DivideServiceFactory.hpp
    include/Test2/Services/Calculator/ICalculatorService.hpp
    include/Test2/Services/Calculator/CalculatorService.hpp
    include/Test2/Services/Calculator/CalculatorServiceFactory.hpp
    include/Test2/Services/Calculator/CalculatorServiceRegistration.hpp
)
configure_target(test2)
target_include_directories(test2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Organize Test2 files into folders for Visual Studio
source_group("Header Files\\Test2\\Framework\\Service" FILES
    include/Test2/Framework/Service/IService.hpp
    include/Test2/Framework/Service/IServiceControl.hpp
    include/Test2/Framework/Service/IServiceFactory.hpp
    include/Test2/Framework/Service/ServiceCreateInfo.hpp
    include/Test2/Framework/Service/ServiceDescription.hpp
    include/Test2/Framework/Service/ServiceInitResult.hpp
    include/Test2/Framework/Service/ServiceProcessResult.hpp
    include/Test2/Framework/Service/ServiceShutdownResult.hpp
)
source_group("Header Files\\Test2\\Framework\\Service\\Async" FILES
    include/Test2/Framework/Service/Async/AsyncServiceBase.hpp
)
source_group("Header Files\\Test2\\Framework\\Provider" FILES
    include/Test2/Framework/Provider/IServiceProvider.hpp
    include/Test2/Framework/Provider/ServiceProvider.hpp
)
source_group("Header Files\\Test2\\Framework\\Registry" FILES
    include/Test2/Framework/Registry/IServiceRegistry.hpp
    include/Test2/Framework/Registry/ServiceLaunchPriority.hpp
    include/Test2/Framework/Registry/ServiceThreadGroupId.hpp
    include/Test2/Framework/Registry/ServiceRegistry.hpp
    include/Test2/Framework/Registry/ServiceRegistrationRecord.hpp
)
source_group("Header Files\\Test2\\Framework\\Manager" FILES
    include/Test2/Framework/Manager/IServiceManager.hpp
)
source_group("Header Files\\Test2\\Services" FILES
    include/Test2/Services/ServiceConfig.hpp
)
source_group("Header Files\\Test2\\Services\\Add" FILES
    include/Test2/Services/Add/IAddService.hpp
    include/Test2/Services/Add/AddService.hpp
    include/Test2/Services/Add/AddServiceFactory.hpp
)
source_group("Header Files\\Test2\\Services\\Subtract" FILES
    include/Test2/Services/Subtract/ISubtractService.hpp
    include/Test2/Services/Subtract/SubtractService.hpp
    include/Test2/Services/Subtract/SubtractServiceFactory.hpp
)
source_group("Header Files\\Test2\\Services\\Multiply" FILES
    include/Test2/Services/Multiply/IMultiplyService.hpp
    include/Test2/Services/Multiply/MultiplyService.hpp
    include/Test2/Services/Multiply/MultiplyServiceFactory.hpp
)
source_group("Header Files\\Test2\\Services\\Divide" FILES
    include/Test2/Services/Divide/IDivideService.hpp
    include/Test2/Services/Divide/DivideService.hpp
    include/Test2/Services/Divide/DivideServiceFactory.hpp
)
source_group("Header Files\\Test2\\Services\\Calculator" FILES
    include/Test2/Services/Calculator/ICalculatorService.hpp
    include/Test2/Services/Calculator/CalculatorService.hpp
    include/Test2/Services/Calculator/CalculatorServiceFactory.hpp
    include/Test2/Services/Calculator/CalculatorServiceRegistration.hpp
)
source_group("Source Files\\Test2\\Framework\\Provider" FILES
    src/Test2/Framework/Provider/ServiceProvider.cpp
)

# Executable 4: Test program 3
add_executable(test3 src/test3.cpp)
configure_target(test3)

# Executable 5: ServiceRegistry test
add_executable(test_service_registry
    UnitTest/Test2/Registry/ServiceRegistryTest.cpp
    include/Test2/Framework/Service/IService.hpp
    include/Test2/Framework/Service/IServiceFactory.hpp
    include/Test2/Framework/Service/ServiceCreateInfo.hpp
    include/Test2/Framework/Provider/ServiceProvider.hpp
    include/Test2/Framework/Registry/IServiceRegistry.hpp
    include/Test2/Framework/Registry/ServiceLaunchPriority.hpp
    include/Test2/Framework/Registry/ServiceThreadGroupId.hpp
    include/Test2/Framework/Registry/ServiceRegistry.hpp
    include/Test2/Framework/Registry/ServiceRegistrationRecord.hpp
    include/Test2/Framework/Exception/DuplicateServiceRegistrationException.hpp
    include/Test2/Framework/Exception/InvalidServiceFactoryException.hpp
    include/Test2/Framework/Exception/RegistryExtractedException.hpp
)
configure_target(test_service_registry)
target_include_directories(test_service_registry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_service_registry PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Registry" FILES UnitTest/Test2/Registry/ServiceRegistryTest.cpp)

# Executable 6: AggregateException test
add_executable(test_aggregate_exception
    UnitTest/Common/AggregateExceptionTest.cpp
    include/Common/AggregateException.hpp
)
configure_target(test_aggregate_exception)
target_include_directories(test_aggregate_exception PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_aggregate_exception PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Common" FILES UnitTest/Common/AggregateExceptionTest.cpp)

# Executable 7: ManagedThreadServiceProvider test
add_executable(test_managed_thread_service_provider
    UnitTest/Test2/Host/ManagedThreadServiceProviderTest.cpp
    include/Test2/Framework/Service/IService.hpp
    src/Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp
    include/Test2/Framework/Exception/InvalidPriorityOrderException.hpp
    include/Test2/Framework/Exception/EmptyPriorityGroupException.hpp
    include/Test2/Framework/Registry/ServiceLaunchPriority.hpp
)
configure_target(test_managed_thread_service_provider)
target_include_directories(test_managed_thread_service_provider PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_managed_thread_service_provider PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Host" FILES UnitTest/Test2/Host/ManagedThreadServiceProviderTest.cpp)

# Executable 8: ServiceProviderProxy test
add_executable(test_service_provider_proxy
    UnitTest/Test2/Provider/ServiceProviderProxyTest.cpp
    include/Test2/Framework/Service/IService.hpp
    include/Test2/Framework/Provider/IServiceProvider.hpp
    include/Test2/Framework/Provider/ServiceProviderProxy.hpp
    include/Test2/Framework/Exception/ServiceProviderException.hpp
)
configure_target(test_service_provider_proxy)
target_include_directories(test_service_provider_proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_service_provider_proxy PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Provider" FILES UnitTest/Test2/Provider/ServiceProviderProxyTest.cpp)

# Executable 9: ServiceProvider template methods test
add_executable(test_service_provider
    UnitTest/Test2/Provider/ServiceProviderTest.cpp
    src/Test2/Framework/Provider/ServiceProvider.cpp
    include/Test2/Framework/Service/IService.hpp
    include/Test2/Framework/Provider/IServiceProvider.hpp
    include/Test2/Framework/Provider/ServiceProvider.hpp
    include/Test2/Framework/Exception/ServiceCastException.hpp
    include/Test2/Framework/Exception/UnknownServiceException.hpp
)
configure_target(test_service_provider)
target_include_directories(test_service_provider PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_service_provider PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Provider" FILES UnitTest/Test2/Provider/ServiceProviderTest.cpp)

# Executable 10: ServiceHostBase test (shared logic, uses CooperativeThreadServiceHost for simplicity)
add_executable(test_service_host_base
    UnitTest/Test2/Host/ServiceHostBaseTest.cpp
    src/Test2/Framework/Provider/ServiceProvider.cpp
    include/Test2/Framework/Host/Managed/ManagedThreadHost.hpp
    include/Test2/Framework/Host/Managed/ManagedThreadRecord.hpp
    src/Test2/Framework/Host/Managed/ManagedThreadServiceHost.hpp
    src/Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp
    src/Test2/Framework/Host/ServiceHostBase.hpp
    include/Test2/Framework/Host/StartServiceRecord.hpp
    include/Test2/Framework/Registry/ServiceLaunchPriority.hpp
)
configure_target(test_service_host_base)
target_include_directories(test_service_host_base PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_service_host_base PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Host" FILES UnitTest/Test2/Host/ServiceHostBaseTest.cpp)

# Executable 11: ManagedThreadServiceHost test (thread-specific behavior)
add_executable(test_managed_thread_service_host
    UnitTest/Test2/Host/ManagedThreadServiceHostTest.cpp
    src/Test2/Framework/Provider/ServiceProvider.cpp
    src/Test2/Framework/Host/Managed/ManagedThreadHost.cpp
    src/Test2/Framework/Host/ServiceHostProxy.cpp
    include/Test2/Framework/Host/Managed/ManagedThreadHost.hpp
    include/Test2/Framework/Host/Managed/ManagedThreadRecord.hpp
    src/Test2/Framework/Host/Managed/ManagedThreadServiceHost.hpp
    src/Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp
    src/Test2/Framework/Host/ServiceHostBase.hpp
    include/Test2/Framework/Host/StartServiceRecord.hpp
    include/Test2/Framework/Registry/ServiceLaunchPriority.hpp
)
configure_target(test_managed_thread_service_host)
target_include_directories(test_managed_thread_service_host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_managed_thread_service_host PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Host" FILES UnitTest/Test2/Host/ManagedThreadServiceHostTest.cpp)

# Executable 12: ProcessResult test
add_executable(test_process_result
    UnitTest/Test2/Service/ProcessResultTest.cpp
    include/Test2/Framework/Service/ProcessStatus.hpp
    include/Test2/Framework/Service/ProcessResult.hpp
)
configure_target(test_process_result)
target_include_directories(test_process_result PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_process_result PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Service" FILES UnitTest/Test2/Service/ProcessResultTest.cpp)

# Executable 13: CooperativeThreadServiceHost test
add_executable(test_cooperative_thread_service_host
    UnitTest/Test2/Host/CooperativeThreadServiceHostTest.cpp
    src/Test2/Framework/Provider/ServiceProvider.cpp
    src/Test2/Framework/Host/Cooperative/CooperativeThreadServiceHost.hpp
    src/Test2/Framework/Host/ServiceHostBase.hpp
    src/Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp
    include/Test2/Framework/Host/StartServiceRecord.hpp
    include/Test2/Framework/Registry/ServiceLaunchPriority.hpp
)
configure_target(test_cooperative_thread_service_host)
target_include_directories(test_cooperative_thread_service_host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_cooperative_thread_service_host PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Host" FILES UnitTest/Test2/Host/CooperativeThreadServiceHostTest.cpp)

# Executable 14: LifecycleManager test
add_executable(test_lifecycle_manager
    UnitTest/Test2/Lifecycle/LifecycleManagerTest.cpp
    src/Test2/Framework/Provider/ServiceProvider.cpp
    include/Test2/Framework/Lifecycle/LifecycleManager.hpp
    include/Test2/Framework/Lifecycle/LifecycleManagerConfig.hpp
    include/Test2/Framework/Host/Cooperative/CooperativeThreadHost.hpp
    src/Test2/Framework/Host/Cooperative/CooperativeThreadHost.cpp
    src/Test2/Framework/Host/Cooperative/CooperativeThreadServiceHost.hpp
    include/Test2/Framework/Host/ServiceHostProxy.hpp
    src/Test2/Framework/Host/ServiceHostProxy.cpp
    src/Test2/Framework/Host/Managed/ManagedThreadHost.cpp
    include/Test2/Framework/Host/Managed/ManagedThreadHost.hpp
    src/Test2/Framework/Host/Managed/ManagedThreadServiceHost.hpp
    src/Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp
    src/Test2/Framework/Host/ServiceHostBase.hpp
    include/Test2/Framework/Host/StartServiceRecord.hpp
    include/Test2/Framework/Registry/ServiceLaunchPriority.hpp
    include/Test2/Framework/Registry/ServiceThreadGroupId.hpp
    include/Test2/Framework/Registry/ServiceRegistrationRecord.hpp
)
configure_target(test_lifecycle_manager)
target_include_directories(test_lifecycle_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_lifecycle_manager PRIVATE GTest::gtest GTest::gtest_main)
source_group("Source Files\\UnitTest\\Test2\\Lifecycle" FILES UnitTest/Test2/Lifecycle/LifecycleManagerTest.cpp)
