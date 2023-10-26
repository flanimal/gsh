# CMakeList.txt : CMake project for CMakeProject1, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("gsh")

# Add source to this project's executable.
add_executable (gsh
	"src/builtin.c" 
	"include/builtin.h"
	"src/gsh.c" 
	"include/gsh.h"
	"src/history.c" 
	"include/history.h"
	"src/parse.c" 
	"include/parse.h"
	"include/process.h"
	"src/special.def"
	"src/main.c"
	"include/params.h" 
	"include/input.h"
)

add_compile_definitions(_GNU_SOURCE)
target_include_directories(gsh PRIVATE "include/")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET gsh PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
