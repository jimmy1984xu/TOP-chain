cmake_minimum_required(VERSION 3.8)

aux_source_directory(./src src)
add_library(xchaininit ${src})

#compile definitions
add_definitions(
    -DUSE_STANDALONE_ASIO
    -DASIO_STANDALONE
    # -D__STRICT_ANSI__
    -DRPC_BUILD
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_options(-Wno-deprecated-declarations)
    add_compile_options(-Wno-write-strings)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_options(-Wno-writable-strings)
    add_compile_options(-Wno-deprecated-declarations)
    add_compile_options(-Wno-write-strings)
    add_compile_options(-Wno-unused-variable)
    add_compile_options(-Wno-unused-function)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_definitions(-D_WIN32_WINNT=0x0501)
    add_definitions(-DNOMINMAX)  # enable std::min/std::max
    add_definitions(-D_CRT_SECURE_NO_WARNINGS) # disable strncpy -> strncpy_s warnings
    add_compile_options(/Zc:__cplusplus) # using correct __cplusplus value
    add_compile_options(/utf-8) # input/output as utf-8
endif()

set(LINK_ARGS secp256k1 trezor-crypto ssl crypto)
if(NOT MSVC)
    set(LINK_ARGS ${LINK_ARGS} hiredis pthread)
endif()
add_dependencies(xchaininit xapplication xdata xconfig xvnode xelect xstore xvnetwork xrpc xrouter xelect_common xtopcl xxbase)

#if (BUILD_VIRTUAL_NODE)
#    add_dependencies(xchaininit xservices)
#    target_link_libraries(xchaininit PRIVATE xservices)
#endif()

target_link_libraries(xchaininit PRIVATE ${LINK_ARGS}
    xapplication
    xconfig
    xloader
    xdata
    xvnode
    xstore
    xrpc
    xelect
    xtopcl
    xrouter
    xvnetwork
    xelect_common
    db_tool
    xxbase
    jsoncpp
    -lpthread -ldl)
