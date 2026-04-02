add_subdirectory(src)

install(DIRECTORY themes ui DESTINATION ${CMAKE_INSTALL_DATADIR})

install(FILES crossfire-client.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

foreach(size 16 32 48)
    install(FILES ../pixmaps/${size}x${size}.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${size}x${size}/apps
        RENAME crossfire-client.png)
endforeach()
