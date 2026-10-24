cmake_minimum_required(VERSION 3.20)
project(legend_za_recomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MINGW)
    # Sem -static o executavel pode carregar um libstdc++ de outra instalacao do MinGW no PATH e falhar ao iniciar
    add_link_options(-static)
endif()

add_library(recomp_core STATIC src/nso.cpp)
target_include_directories(recomp_core PUBLIC include)
target_compile_options(recomp_core PRIVATE -Wall -Wextra)

add_executable(nso_inspect tools/nso_inspect.cpp)
target_link_libraries(nso_inspect PRIVATE recomp_core)
target_compile_options(nso_inspect PRIVATE -Wall -Wextra)

enable_testing()
add_executable(nso_test tests/nso_test.cpp)
target_link_libraries(nso_test PRIVATE recomp_core)
target_compile_options(nso_test PRIVATE -Wall -Wextra)
set(ZA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME nso_test COMMAND nso_test ${ZA_ROOT}/exefs)
