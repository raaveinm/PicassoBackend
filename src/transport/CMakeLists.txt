# http and ws are separate targets rather than one "transport" blob: they have
# genuinely different dependencies (only ws needs oatpp-websocket), and keeping them
# apart makes that visible in the build rather than buried in includes.
add_subdirectory(http)
add_subdirectory(ws)
