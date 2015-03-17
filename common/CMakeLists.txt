add_custom_command(
    OUTPUT def-keys.h
    COMMAND ${PERL_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/mdk.pl ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS def-keys
)

add_custom_command(
    OUTPUT msgtypes.h
    COMMAND ${PERL_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/msgtypes.pl ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS shared/newclient.h
)

# While 'item-types.h' is still under version control, don't build it.
#add_custom_command(
#    OUTPUT item-types.h
#    COMMAND ${PERL_EXECUTABLE}
#        ${CMAKE_CURRENT_SOURCE_DIR}/items.pl ${CMAKE_CURRENT_SOURCE_DIR}
#    DEPENDS item-types
#)

add_library(cfclient
    client.c
    client.h
    commands.c
    def-keys.h
    external.h
    image.c
    init.c
    item.c
    item.h
    mapdata.c
    mapdata.h
    metaserver.c
    metaserver.h
    misc.c
    msgtypes.h
    newsocket.c
    p_cmd.c
    p_cmd.h
    player.c
    proto.h
    script.c
    script.h
    script_lua.c
    shared/newclient.h
    version.h
)
