# Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

add_custom_target(lauf_test_qbe ALL)

find_program(QBE_EXE NAMES qbe)

unset(ASSEMBLER CACHE)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
elseif(MSVC)
    find_program(ASSEMBLER NAMES yasm)
endif()

if(QBE_EXE)
    file(GLOB test_files CONFIGURE_DEPENDS "*.lauf")
    foreach(file ${test_files})
        get_filename_component(name ${file} NAME)
        add_test(NAME ${name} COMMAND lauf_tool_interpreter ${file})

        # MSVC crashes when running a generated QBE file for some alloc calls in heap and memory
        if(ASSEMBLER AND name MATCHES "heap.lauf|memory.lauf")
            continue()
        endif()

        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.qbe COMMAND lauf_tool_qbe ${file} > ${name}.qbe DEPENDS ${file} lauf_tool_qbe)
        add_custom_command(OUTPUT ${name}.s   COMMAND ${QBE_EXE} -o ${name}.s ${CMAKE_CURRENT_BINARY_DIR}/${name}.qbe  DEPENDS ${name}.qbe)
        if(ASSEMBLER)
            add_custom_command(OUTPUT ${name}.obj COMMAND ${ASSEMBLER} -f win64 -w -p gas ${name}.s  -o ${name}.obj
                                                DEPENDS ${name}.s)
            add_custom_command(OUTPUT ${name}.exe COMMAND ${CMAKE_C_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c ${name}.obj /nologo /MT  /Fe:${name}.exe /link /FORCE:UNRESOLVED /DYNAMICBASE:NO
                                                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c ${name}.obj)
        else()
            add_custom_command(OUTPUT ${name}.exe COMMAND ${CMAKE_C_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c ${name}.s  -o ${name}.exe
                                                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c ${name}.s)
        endif()

        add_custom_target(lauf_test_qbe_${name} DEPENDS ${name}.exe)
        add_dependencies(lauf_test_qbe lauf_test_qbe_${name})

        add_test(NAME ${name}.qbe COMMAND ${name}.exe)
    endforeach()
endif()
