#Windows specifc build of named pipe test
cmake_minimum_required (VERSION VERSION 3.12...4.0)
project (named_pipe_test)

include_directories( ".." )
file(GLOB SRC "*.cpp")

add_definitions(-D_CRT_SECURE_NO_WARNINGS )
include_directories( include "../../common" )
add_executable (named_pipe_test ${SRC} "../../common/gptp_log.cpp" "../../windows/daemon_cl/platform.cpp")
target_link_libraries(named_pipe_test Iphlpapi Ws2_32)
