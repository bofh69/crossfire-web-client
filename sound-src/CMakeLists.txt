add_executable(cfsndserv
    cfsndserv.c
    common.h
    init.c
    main.c
    parser.c
    sndproto.h
)

target_link_libraries(cfsndserv cfclient ${SDL_LIBRARY} ${SDLMIXER_LIBRARY})

install(TARGETS cfsndserv DESTINATION ${CMAKE_INSTALL_BINDIR})
