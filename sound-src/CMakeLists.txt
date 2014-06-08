add_executable(cfsndserv
    cfsndserv.c
    common.h
    init.c
    main.c
    parser.c
    sndproto.h
)

target_link_libraries(cfsndserv cfclient
    ${GTK2_LIBRARIES}
    ${SDLMIXER_LIBRARY}
    ${SDL_LIBRARY}
)

install(TARGETS cfsndserv DESTINATION ${CMAKE_INSTALL_BINDIR})
