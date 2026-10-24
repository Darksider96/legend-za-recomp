cmake_minimum_required(VERSION 3.20)
project(legend_za_recomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MINGW)
    # Sem -static o executavel pode carregar um libstdc++ de outra instalacao do MinGW no PATH e falhar ao iniciar
    add_link_options(-static)
endif()

add_library(recomp_core STATIC src/nso.cpp src/a64_decode.cpp src/emit_c.cpp)
target_include_directories(recomp_core PUBLIC include)
target_compile_options(recomp_core PRIVATE -Wall -Wextra)

add_executable(nso_inspect tools/nso_inspect.cpp)
target_link_libraries(nso_inspect PRIVATE recomp_core)
target_compile_options(nso_inspect PRIVATE -Wall -Wextra)

add_executable(a64_recomp tools/a64_recomp.cpp)
target_link_libraries(a64_recomp PRIVATE recomp_core)
target_compile_options(a64_recomp PRIVATE -Wall -Wextra)

add_executable(a64_dump tools/a64_dump.cpp)
target_link_libraries(a64_dump PRIVATE recomp_core)
target_compile_options(a64_dump PRIVATE -Wall -Wextra)

enable_testing()
add_executable(nso_test tests/nso_test.cpp)
target_link_libraries(nso_test PRIVATE recomp_core)
target_compile_options(nso_test PRIVATE -Wall -Wextra)
set(ZA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME nso_test COMMAND nso_test ${ZA_ROOT}/exefs)

add_executable(context_test tests/context_test.cpp)
target_include_directories(context_test PRIVATE include)
target_compile_options(context_test PRIVATE -Wall -Wextra)
add_test(NAME context_test COMMAND context_test)

# Prova de conceito R2: traduz a funcao 0x133f0c0 do dump do usuario no build (o codigo gerado nunca vai para o git)
set(ZA_MAIN ${ZA_ROOT}/exefs/main)
if(EXISTS ${ZA_MAIN})
    set(FNV_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated/fnv_poc.cpp)
    add_custom_command(OUTPUT ${FNV_GENERATED}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
        COMMAND a64_recomp ${ZA_MAIN} --function 0x133f0c0 --out ${FNV_GENERATED}
        DEPENDS a64_recomp ${ZA_MAIN}
        COMMENT "Traduzindo f_133f0c0 (FNV-1a 64) do exefs/main")
    add_executable(fnv_poc_test tests/fnv_poc_test.cpp src/runtime_stubs.cpp ${FNV_GENERATED})
    target_include_directories(fnv_poc_test PRIVATE include ${ZA_ROOT}/za_port/include)
    target_compile_options(fnv_poc_test PRIVATE -Wall -Wextra)
    add_test(NAME fnv_poc_test COMMAND fnv_poc_test)
else()
    message(STATUS "exefs/main ausente: fnv_poc_test nao sera gerado")
endif()
