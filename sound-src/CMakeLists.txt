vala_precompile(VALA_C snd.vala OPTIONS -g GENERATE_HEADER snd)

add_executable(cfsndserv
    ${VALA_C}
    cfsndserv.c
    common.h
    main.c
)

target_link_libraries(cfsndserv
    ${GTK2_LIBRARIES}
    ${SDLMIXER_LIBRARY}
    ${SDL_LIBRARY}
)

install(TARGETS cfsndserv DESTINATION ${CMAKE_INSTALL_BINDIR})
