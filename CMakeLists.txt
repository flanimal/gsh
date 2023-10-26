# CMakeList.txt : CMake project for gsh, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("gsh")

# :^)

# Add source to this project's executable.
add_executable (gsh
	"include/builtin.h"
	"include/gsh.h"
	"include/history.h"
	"include/parse.h"
	"include/process.h"
	"include/params.h" 
	"include/input.h"
	"src/builtin.c" 
	"src/gsh.c" 
	"src/history.c" 
	"src/parse.c" 
	"src/special.def"
	"src/main.c"
)

target_compile_definitions(gsh PRIVATE _GNU_SOURCE)
target_include_directories(gsh PRIVATE "include/")
target_compile_options(gsh PRIVATE -Werror -Wall -Wextra -Wno-unused-parameter -pedantic-errors)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET gsh PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
