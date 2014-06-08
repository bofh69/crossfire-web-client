add_executable(crossfire-client-gtk2
    about.c
    account.c
    config.c
    create_char.c
    gtk2proto.h
    image.c
    image.h
    info.c
    info.h
    inventory.c
    keys.c
    magicmap.c
    main.c
    main.h
    map.c
    menubar.c
    metaserver.c
    opengl.c
    pickup.c
    png.c
    sdl.c
    skills.c
    sound.c
    spells.c
    stats.c
)

target_link_libraries(crossfire-client-gtk2 cfclient
    ${CURL_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${LUA_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${PNG_LIBRARIES}
    ${SDL_LIBRARY}
    ${X11_LIBRARIES}
)

if(WIN32)
    target_link_libraries(crossfire-client-gtk2
        ${WSOCK32_LIBRARY} ${WINMM_LIBRARY})
endif()

install(TARGETS crossfire-client-gtk2 DESTINATION ${CMAKE_INSTALL_BINDIR})
