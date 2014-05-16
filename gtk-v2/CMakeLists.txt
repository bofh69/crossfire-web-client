add_subdirectory(src)
add_subdirectory(themes)
add_subdirectory(ui)

install(FILES crossfire-client.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

install(FILES crossfire-client-gtk2.man DESTINATION ${CMAKE_INSTALL_MANDIR})
