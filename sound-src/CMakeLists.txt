vala_precompile(VALA_C snd.vala OPTIONS -g GENERATE_HEADER snd)

add_executable(cfsndserv
    ${VALA_C}
    cfsndserv.c
    common.h
    main.c
)

target_include_directories(cfsndserv
    PRIVATE ${GTK2_INCLUDE_DIRS}
            ${SDLMIXER_INCLUDE_DIRS}
            ${PROJECT_BINARY_DIR}
            ${PROJECT_SOURCE_DIR}/common
)

target_link_libraries(cfsndserv
    ${GTK2_LDFLAGS}
    ${SDLMIXER_LDFLAGS}
)

install(TARGETS cfsndserv DESTINATION ${CMAKE_INSTALL_BINDIR})
