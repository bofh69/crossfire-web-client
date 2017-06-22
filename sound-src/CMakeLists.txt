add_executable(cfsndserv
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
