cmake_minimum_required(VERSION 3.20)
project(legend_za_recomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MINGW)
    # Sem -static o executavel pode carregar um libstdc++ de outra instalacao do MinGW no PATH e falhar ao iniciar
    add_link_options(-static)
endif()
# LDXP/STXP de 64 bits no codigo gerado viram cmpxchg16b; sem contracao de FP, a*b+c de instrucoes diferentes
# nao pode virar FMA (o ARM arredonda cada uma)
add_compile_options(-mcx16 -ffp-contract=off)

add_library(recomp_core STATIC src/nso.cpp src/a64_decode.cpp src/a64_simd.cpp src/emit_c.cpp src/emit_simd.cpp src/jump_tables.cpp)
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

add_executable(a64_insn_tests tools/a64_insn_tests.cpp)
target_link_libraries(a64_insn_tests PRIVATE recomp_core)
target_compile_options(a64_insn_tests PRIVATE -Wall -Wextra)

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

add_executable(simd_test tests/simd_test.cpp)
target_include_directories(simd_test PRIVATE include)
target_compile_options(simd_test PRIVATE -Wall -Wextra -ffp-contract=off)
add_test(NAME simd_test COMMAND simd_test)

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

    # Testes diferenciais por instrucao (R3): fora do build padrao; quem roda e recomp/tools/insn_difftest.py,
    # que compara o resultado com o Unicorn
    set(INSN_TESTS_PER_SHAPE 3 CACHE STRING "Instrucoes reais sorteadas por forma em insn_difftest")
    set(INSN_DIR ${CMAKE_CURRENT_BINARY_DIR}/insn)
    add_custom_command(OUTPUT ${INSN_DIR}/insn_tests.cpp ${INSN_DIR}/insn_tests.csv ${INSN_DIR}/image.bin
        COMMAND a64_insn_tests ${ZA_MAIN} --out-dir ${INSN_DIR} --per-shape ${INSN_TESTS_PER_SHAPE}
        DEPENDS a64_insn_tests ${ZA_MAIN}
        COMMENT "Sorteando e traduzindo instrucoes do exefs/main para insn_difftest")
    add_executable(insn_difftest EXCLUDE_FROM_ALL tests/insn_difftest.cpp ${INSN_DIR}/insn_tests.cpp)
    target_link_libraries(insn_difftest PRIVATE recomp_core)
    target_include_directories(insn_difftest PRIVATE tests)
else()
    message(STATUS "exefs/main ausente: fnv_poc_test e insn_difftest nao serao gerados")
endif()

# Jogo recompilado (R4): traduz main e sdk inteiros a partir do dump do usuario e compila o za_recomp.
# Demorado (~1,2 GB de C++); o codigo gerado fica em build/ e nunca vai para o git.
option(RECOMP_GAME "Traduzir main e sdk e compilar o za_recomp" OFF)
set(RECOMP_GUEST_JOBS 6 CACHE STRING "Compilacoes paralelas dos arquivos gerados (cada uma usa ~0,6 GB de RAM)")
set(ZA_SDK ${ZA_ROOT}/exefs/sdk)
if(RECOMP_GAME AND EXISTS ${ZA_MAIN} AND EXISTS ${ZA_SDK})
    set(GUEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/guest)
    set(GAP_FUNCTIONS ${ZA_ROOT}/analysis/recomp/generated/gap_functions.csv)
    set(MAIN_EXTRA)
    if(EXISTS ${GAP_FUNCTIONS})
        set(MAIN_EXTRA --extra ${GAP_FUNCTIONS})
    endif()
    function(guest_outputs dir count out)
        set(files ${dir}/missing.cpp ${dir}/dispatch_table.cpp)
        math(EXPR last "${count} - 1")
        foreach(i RANGE ${last})
            string(LENGTH "${i}" len)
            if(len EQUAL 1)
                set(i "00${i}")
            elseif(len EQUAL 2)
                set(i "0${i}")
            endif()
            list(APPEND files ${dir}/guest_${i}.cpp)
        endforeach()
        set(${out} ${files} PARENT_SCOPE)
    endfunction()
    guest_outputs(${GUEST_DIR}/main 256 MAIN_SOURCES)
    guest_outputs(${GUEST_DIR}/sdk 40 SDK_SOURCES)
    add_custom_command(OUTPUT ${MAIN_SOURCES}
        COMMAND a64_recomp ${ZA_MAIN} --all --out-dir ${GUEST_DIR}/main --files 256 ${MAIN_EXTRA}
        DEPENDS a64_recomp ${ZA_MAIN}
        COMMENT "Traduzindo exefs/main")
    add_custom_command(OUTPUT ${SDK_SOURCES}
        COMMAND a64_recomp ${ZA_SDK} --all --out-dir ${GUEST_DIR}/sdk --files 40 --base 0x7108000000 --prefix s_ --table kSdkFunctions
        DEPENDS a64_recomp ${ZA_SDK}
        COMMENT "Traduzindo exefs/sdk")
    set_source_files_properties(${MAIN_SOURCES} ${SDK_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-foptimize-sibling-calls;-w;-Wa,-mbig-obj")
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS guest_pool=${RECOMP_GUEST_JOBS})
    add_executable(za_recomp runtime/main.cpp runtime/loader.cpp runtime/dispatch.cpp runtime/kernel.cpp ${MAIN_SOURCES} ${SDK_SOURCES})
    target_link_libraries(za_recomp PRIVATE recomp_core)
    target_compile_options(za_recomp PRIVATE -Wall -Wextra)
    target_link_options(za_recomp PRIVATE -Wl,--stack,268435456)
    set_property(TARGET za_recomp PROPERTY JOB_POOL_COMPILE guest_pool)
endif()
