cmake_minimum_required(VERSION 3.20)
project(za_port VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Diretórios de include
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Arquivos de código da camada de compatibilidade
set(SOURCES
    src/main.cpp
    src/services/os.cpp
    src/services/time.cpp
    src/services/hid.cpp
    src/services/fs.cpp
    src/services/vi.cpp
    src/services/nvn.cpp
    src/services/audio.cpp
    src/services/audio_playback.cpp
    src/gfx/renderer3d.cpp
    src/gfx/bntx.cpp
    src/gfx/trmdl.cpp
    src/gfx/tranm.cpp
    src/gfx/texture_codec.cpp
    src/gfx/ik_character.cpp
    src/gfx/trcol.cpp
    src/gfx/trscn.cpp
    src/gfx/scene_loader.cpp
    src/gfx/scene_assembly.cpp
    src/game/poke_actor.cpp
    src/audio/wwise.cpp
    src/audio/wem_decoder.cpp
    src/audio/ambient_stream.cpp
    src/data/personal_table.cpp
    src/data/message_text.cpp
    src/data/move_table.cpp
    src/data/learnset_table.cpp
    src/config/game_config.cpp
    src/loader/nso_loader.cpp
    src/loader/got_patcher.cpp
    src/vfs/trpfs.cpp
    src/vfs/oodle.cpp
    src/vfs/asset_export.cpp
    src/vfs/name_recovery.cpp
    src/nn/diag.cpp
    src/za/subsystem_registry.cpp
    src/za/logger.cpp
    src/za/crash_handler.cpp
    src/za/profiler.cpp
)

add_executable(za_port ${SOURCES})

# Configuração para Windows
if(WIN32)
    target_compile_definitions(za_port PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(za_port PRIVATE gdi32 user32 winmm psapi dbghelp)
endif()

# Ferramenta CLI de extração de assets TRPFS
set(TRPFS_SOURCES
    src/vfs/trpfs.cpp
    src/vfs/oodle.cpp
    src/vfs/asset_export.cpp
    src/vfs/name_recovery.cpp
    src/gfx/bntx.cpp
    src/gfx/trmdl.cpp
    src/gfx/tranm.cpp
    src/gfx/texture_codec.cpp
    src/gfx/ik_character.cpp
    src/gfx/trcol.cpp
    src/gfx/trscn.cpp
    src/gfx/scene_loader.cpp
    src/gfx/scene_assembly.cpp
    src/audio/wwise.cpp
    src/audio/wem_decoder.cpp
    src/audio/ambient_stream.cpp
    src/data/personal_table.cpp
    src/data/message_text.cpp
    src/data/move_table.cpp
    src/data/learnset_table.cpp
)

find_package(Threads REQUIRED)

add_executable(trpfs_tool tools/trpfs_tool.cpp ${TRPFS_SOURCES})
target_link_libraries(trpfs_tool PRIVATE Threads::Threads)
if(WIN32)
    target_compile_definitions(trpfs_tool PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

add_executable(za_configurator src/tools/configurator_main.cpp src/config/game_config.cpp)

# Teste de configuracao unitario
add_executable(config_test tests/config_test.cpp src/config/game_config.cpp)
add_test(NAME config_test COMMAND config_test)

# Testes de integracao contra o RomFS real (../romfs) usando a DLL do Oodle de za_port/
enable_testing()
set(ZA_ROMFS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../romfs)
foreach(test_name trpfs_test assets_test audio_test personal_test world_test battle_test learnset_test combat_rules_test)
    add_executable(${test_name} tests/${test_name}.cpp ${TRPFS_SOURCES})
    target_link_libraries(${test_name} PRIVATE Threads::Threads)
endforeach()
add_test(NAME trpfs_test COMMAND trpfs_test ${ZA_ROMFS_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/trpfs_refs.txt)
add_test(NAME assets_test COMMAND assets_test ${ZA_ROMFS_DIR})
add_test(NAME audio_test COMMAND audio_test ${ZA_ROMFS_DIR})
add_test(NAME personal_test COMMAND personal_test ${ZA_ROMFS_DIR})
add_test(NAME world_test COMMAND world_test ${ZA_ROMFS_DIR})
add_test(NAME battle_test COMMAND battle_test ${ZA_ROMFS_DIR})
add_test(NAME learnset_test COMMAND learnset_test ${ZA_ROMFS_DIR})
add_test(NAME combat_rules_test COMMAND combat_rules_test ${ZA_ROMFS_DIR})
set_tests_properties(trpfs_test assets_test personal_test world_test battle_test learnset_test combat_rules_test PROPERTIES
    ENVIRONMENT "ZA_OODLE_DLL=${CMAKE_CURRENT_SOURCE_DIR}/oo2core_9_win64.dll")
