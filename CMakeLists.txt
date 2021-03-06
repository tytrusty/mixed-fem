cmake_minimum_required(VERSION 3.0.0)

project("libIGL polyscope example project")

if (NOT CMAKE_BUILD_TYPE)
  message (STATUS "No build type selected, default to Release")
  set (CMAKE_BUILD_TYPE "Release")
endif ()

# Maybe stop from CMAKEing in the wrong place
if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Source and build directories cannot be the same. Go use the /build directory.")
endif()

# For nasoq
# set(CMAKE_POSITION_INDEPENDENT_CODE ON)

### Configure output locations
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

### Compiler options
set( CMAKE_EXPORT_COMPILE_COMMANDS 1 ) # Emit a compile flags file to support completion engines 

set(CMAKE_CXX_STANDARD 17)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  # using Clang (linux or apple) or GCC
  message("Using clang/gcc compiler flags")
  SET(BASE_CXX_FLAGS "-std=c++11")# -g3")
  SET(DISABLED_WARNINGS " -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-deprecated-declarations -Wno-missing-braces")
  SET(TRACE_INCLUDES " -H -Wno-error=unused-command-line-argument")

  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    message("Setting clang-specific options")
    SET(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -ferror-limit=5 -fcolor-diagnostics")
    SET(CMAKE_CXX_FLAGS_DEBUG          "-fsanitize=address -fno-limit-debug-info")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    SET(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -fmax-errors=5")
    message("Setting gcc-specific options")
    SET(DISABLED_WARNINGS "${DISABLED_WARNINGS} -Wno-maybe-uninitialized -Wreorder \
      -Wuninitialized -Wunused-result -Wunused-local-typedefs -Wsign-compare  \
      -Wno-format-zero-length -Wno-unused-but-set-parameter -Wno-unused-but-set-variable")
  endif()


  SET(CMAKE_CXX_FLAGS "${BASE_CXX_FLAGS} ${DISABLED_WARNINGS}")
  #SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TRACE_INCLUDES}") # uncomment if you need to track down where something is getting included from
  SET(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} -g3")
  SET(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
  SET(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # using Visual Studio C++
  message("Using Visual Studio compiler flags")
  set(BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  set(BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") # parallel build
  SET(DISABLED_WARNINGS "${DISABLED_WARNINGS} /wd\"4267\"")  # ignore conversion to smaller type (fires more aggressively than the gcc version, which is annoying)
  SET(DISABLED_WARNINGS "${DISABLED_WARNINGS} /wd\"4244\"")  # ignore conversion to smaller type (fires more aggressively than the gcc version, which is annoying)
  SET(DISABLED_WARNINGS "${DISABLED_WARNINGS} /wd\"4305\"")  # ignore truncation on initialization
  SET(CMAKE_CXX_FLAGS "${BASE_CXX_FLAGS} ${DISABLED_WARNINGS}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")

  add_definitions(/D "_CRT_SECURE_NO_WARNINGS")
  add_definitions(-DNOMINMAX)
  add_definitions(-D_USE_MATH_DEFINES)

else()
  # unrecognized
  message( FATAL_ERROR "Unrecognized compiler [${CMAKE_CXX_COMPILER_ID}]" )
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

# Find Eigen
find_package(Eigen3 3.4.0 REQUIRED)
find_package(OpenMP REQUIRED)
find_package(BLAS)
find_package(LAPACK)
find_package(CHOLMOD)
find_package(SuiteSparse)

add_subdirectory("deps/amgcl")

# Add polyscope
message("\n\n == CMAKE recursively building Polyscope\n")
add_subdirectory("deps/polyscope")
# Add libIGL
# With these lines commented out, we use libIGL in "header only" mode.  Uncomment to recurse and get the full package.
# (and see the libIGL build documentation)
#message("\n\n == CMAKE recursively building libIGL\n")
#set(LIBIGL_WITH_EMBREE            OFF CACHE BOOL "Use Embree")
#set(LIBIGL_WITH_OPENGL            OFF CACHE BOOL "Use OpenGL")
#set(LIBIGL_WITH_OPENGL            OFF CACHE BOOL "Use OpenGL")
#set(LIBIGL_WITH_OPENGL_GLFW       OFF CACHE BOOL "Use GLFW"  )
#set(LIBIGL_WITH_OPENGL_GLFW_IMGUI OFF CACHE BOOL "Use ImGui" )
#add_subdirectory("deps/libIGL")

# set(NASOQ_BLAS_BACKEND  "OpenBLAS")
# set(NASOQ_USE_CLAPACK  ON)
# set(NASOQ_WITH_EIGEN OFF)
# add_subdirectory("deps/nasoq")

file(GLOB SOURCES 
    ${PROJECT_SOURCE_DIR}/src/*.cpp 
    ${PROJECT_SOURCE_DIR}/src/svd/*.cpp 
    ${PROJECT_SOURCE_DIR}/src/energies/*.cpp
    ${PROJECT_SOURCE_DIR}/src/optimizers/*.cpp
    ${PROJECT_SOURCE_DIR}/src/variables/*.cpp
    ${PROJECT_SOURCE_DIR}/src/linear_solvers/*.cpp
    ${PROJECT_SOURCE_DIR}/src/time_integrators/*.cpp
    ${PROJECT_SOURCE_DIR}/src/factories/*.cpp
    ${PROJECT_SOURCE_DIR}/src/mesh/*.cpp)

# message("SOURCES" ${SOURCES})
add_library(mixed_fem_lib SHARED ${SOURCES})

include(FetchContent)
FetchContent_Declare(
    finite-diff
    GIT_REPOSITORY https://github.com/zfergus/finite-diff.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(finite-diff)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.x
)
FetchContent_MakeAvailable(Catch2)

# Add OMP library and includes
if(OpenMP_CXX_FOUND)

    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        execute_process(
            COMMAND brew --prefix libomp 
            RESULT_VARIABLE BREW_OMP
            OUTPUT_VARIABLE BREW_OMP_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        include_directories(${BREW_OMP_PREFIX}/include)
        target_link_libraries(mixed_fem_lib OpenMP::OpenMP_CXX)
        target_compile_definitions(mixed_fem_lib PUBLIC -DSIM_USE_OPENMP)

    else()
        include_directories(${OpenMP_CXX_INCLUDE_DIRS})
        target_link_libraries(mixed_fem_lib OpenMP::OpenMP_CXX)
        target_compile_definitions(mixed_fem_lib PUBLIC -DSIM_USE_OPENMP)
    endif()
    
endif()

# Blas, lapack, and cholmod
if(BLAS_FOUND)
  target_link_libraries(mixed_fem_lib ${BLAS_LIBRARIES})
endif()
if(LAPACK_FOUND)
  target_link_libraries(mixed_fem_lib ${LAPACK_LIBRARIES})
endif()
if(CHOLMOD_FOUND AND BLAS_FOUND AND LAPACK_FOUND)
message(CHOL INCLUDES ${CHOLMOD_INCLUDE_DIR})
  include_directories(${CHOLMOD_INCLUDE_DIR})
  target_link_libraries(mixed_fem_lib ${CHOLMOD_LIBRARIES})
  target_compile_definitions(mixed_fem_lib PUBLIC -DSIM_USE_CHOLMOD)
endif()

TRY_COMPILE(COMPILER_SUPPORTS_ARM_NEON ${CMAKE_BINARY_DIR}
   ${PROJECT_SOURCE_DIR}/cmake/check_arm_neon.cpp)
TRY_COMPILE(COMPILER_SUPPORTS_AVX    ${CMAKE_BINARY_DIR}
  ${PROJECT_SOURCE_DIR}//cmake/check_isa.cpp COMPILE_DEFINITIONS ${FLAGS_AVX})

if (COMPILER_SUPPORTS_ARM_NEON)
  target_compile_definitions(mixed_fem_lib PUBLIC -DUSE_NEON_IMPLEMENTATION)
elseif(COMPILER_SUPPORTS_AVX)  
  target_compile_definitions(mixed_fem_lib PUBLIC -DUSE_AVX_IMPLEMENTATION)
elseif ()
  target_compile_definitions(mixed_fem_lib PUBLIC -DUSE_SSE_IMPLEMENTATION)
ENDIF()

# Include settings
#target_include_directories(mixed_fem_lib PUBLIC 
# "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Shouldn't do this but the warnings aren't be suppressed and its making me insane
target_include_directories(mixed_fem_lib SYSTEM PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/deps/agmcl"
  "${CMAKE_CURRENT_SOURCE_DIR}/deps/Bartels/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/deps/libigl/include" 
#   #"${CMAKE_CURRENT_SOURCE_DIR}/deps/nasoq/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/deps/polyscope/deps/args"
  "${CMAKE_CURRENT_SOURCE_DIR}/deps/polyscope/deps/json/include")

# Link settings
target_link_libraries(mixed_fem_lib polyscope Eigen3::Eigen amgcl::amgcl)

# Create an executable
add_executable(tet_sim apps/tet_sim.cpp ${SOURCES})
target_link_libraries(tet_sim mixed_fem_lib)

add_executable(cloth_sim apps/cloth_sim.cpp ${SOURCES})
target_link_libraries(cloth_sim mixed_fem_lib)

add_executable(rod_sim apps/rod_sim.cpp ${SOURCES})
target_link_libraries(rod_sim mixed_fem_lib)

add_executable(decrement apps/newton_decrement.cpp ${SOURCES})
target_link_libraries(decrement mixed_fem_lib)

add_executable(energy apps/energy.cpp ${SOURCES})
target_link_libraries(energy mixed_fem_lib)

add_executable(tri2d_sim apps/tri2d_sim.cpp ${SOURCES})
target_link_libraries(tri2d_sim mixed_fem_lib)

#add_subdirectory(tests)
