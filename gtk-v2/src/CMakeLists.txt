vala_precompile(VALA_C sound-vala.vala OPTIONS -g GENERATE_HEADER client-vala)

add_executable(crossfire-client-gtk2
    ${VALA_C}
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

target_compile_features(crossfire-client-gtk2 PRIVATE c_std_99)

target_include_directories(crossfire-client-gtk2
    PRIVATE ${GTK2_INCLUDE_DIRS}
            ${OPENGL_INCLUDE_DIR}
            ${PNG_INCLUDE_DIRS}
            ${SDLMIXER_INCLUDE_DIRS}
            ${SDL_INCLUDE_DIR}
            ${PROJECT_BINARY_DIR}
            ${PROJECT_BINARY_DIR}/common
            ${PROJECT_SOURCE_DIR}/common
)

target_link_libraries(crossfire-client-gtk2 cfclient
    ${CURL_LDFLAGS}
    ${GTK2_LDFLAGS}
    ${LUA_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${PNG_LIBRARIES}
    ${SDL_LDFLAGS}
    ${X11_LIBRARIES}
)

if(MINGW OR WIN32)
    target_link_libraries(crossfire-client-gtk2 wsock32)
endif()

install(TARGETS crossfire-client-gtk2 DESTINATION ${CMAKE_INSTALL_BINDIR})
