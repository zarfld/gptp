cmake_minimum_required (VERSION 3.12...4.0)
project (gptp)
include(CheckCXXCompilerFlag)
if (MSVC)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  # Add /FS flag to allow multiple processes to write to the same PDB file
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FS")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /FS")
else()
  CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
  CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

  if(COMPILER_SUPPORTS_CXX11)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  elseif(COMPILER_SUPPORTS_CXX0X)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
  else()
          message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
  endif()
endif()

include_directories( "./common" )
file(GLOB GPTP_COMMON "./common/*.cpp" "./common/*.c")

if(UNIX)
  include_directories( include "./linux/src" )
  file(GLOB GPTP_OS 
  "./linux/src/daemon_cl.cpp"
  "./linux/src/linux_ipc.cpp"
  "./linux/src/platform.cpp"
  "./linux/src/linux_hal_persist_file.cpp"
  "./linux/src/linux_hal_generic.cpp"
  "./linux/src/linux_hal_generic_adj.cpp"
  "./linux/src/linux_hal_common.cpp")
  add_executable (gptp ${GPTP_COMMON} ${GPTP_OS})
  target_link_libraries(gptp pthread rt)
elseif(WIN32)
  # ✅ IMPLEMENTING: WinPcap to Npcap Migration - Dual SDK Support
  # Supports both modern Npcap (preferred) and legacy WinPcap (fallback)
  # for maximum Windows 10/11 compatibility
  
  # Detect available packet capture SDK
  if(DEFINED ENV{NPCAP_DIR})
    # Modern Npcap SDK (preferred for Windows 10/11)
    set(PCAP_ROOT_DIR $ENV{NPCAP_DIR})
    set(PCAP_LIBRARY_NAME "wpcap")  # Both Npcap and WinPcap use wpcap.lib for pcap API
    message(STATUS "✅ Using Npcap SDK from: ${PCAP_ROOT_DIR}")
    add_definitions(-DUSING_NPCAP)
  elseif(DEFINED ENV{WPCAP_DIR})
    # Legacy WinPcap SDK (fallback for older systems)  
    set(PCAP_ROOT_DIR $ENV{WPCAP_DIR})
    set(PCAP_LIBRARY_NAME "wpcap")   # WinPcap uses wpcap.lib
    message(STATUS "⚠️  Using legacy WinPcap SDK from: ${PCAP_ROOT_DIR}")
    add_definitions(-DUSING_WINPCAP)
  else()
    message(FATAL_ERROR "❌ Neither NPCAP_DIR nor WPCAP_DIR environment variable found. Please install Npcap SDK or WinPcap SDK.")
  endif()

  # Set up include and library directories
  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    link_directories(${PCAP_ROOT_DIR}/Lib/x64)
  elseif( CMAKE_SIZEOF_VOID_P EQUAL 4 )
    link_directories(${PCAP_ROOT_DIR}/Lib)
  endif()

  # HAVE_REMOTE change pcap include options
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DHAVE_REMOTE)
  include_directories( include "./windows/daemon_cl" ${PCAP_ROOT_DIR}/Include )
  file(GLOB GPTP_OS "./windows/daemon_cl/*.cpp" "./windows/daemon_cl/gptp.manifest")
  add_executable (gptp ${GPTP_COMMON} ${GPTP_OS})
  target_link_libraries(gptp ${PCAP_LIBRARY_NAME} Iphlpapi Ws2_32)

  # Copy configuration files to output directory for deployment
  add_custom_command(TARGET gptp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/gptp_cfg.ini"
      "$<TARGET_FILE_DIR:gptp>/gptp_cfg.ini"
    COMMENT "Copying gptp_cfg.ini to output directory"
  )
  
  add_custom_command(TARGET gptp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/test_milan_config.ini"
      "$<TARGET_FILE_DIR:gptp>/test_milan_config.ini"
    COMMENT "Copying test_milan_config.ini to output directory"
  )
  
  add_custom_command(TARGET gptp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/avnu_presonus_config.ini"
      "$<TARGET_FILE_DIR:gptp>/avnu_presonus_config.ini"
    COMMENT "Copying avnu_presonus_config.ini to output directory"
  )
  
  add_custom_command(TARGET gptp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/avnu_base_config.ini"
      "$<TARGET_FILE_DIR:gptp>/avnu_base_config.ini"
    COMMENT "Copying avnu_base_config.ini to output directory"
  )
  
  add_custom_command(TARGET gptp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/test_unified_profiles.ini"
      "$<TARGET_FILE_DIR:gptp>/test_unified_profiles.ini"
    COMMENT "Copying test_unified_profiles.ini to output directory"
  )

  add_subdirectory("windows/named_pipe_test")

endif()

