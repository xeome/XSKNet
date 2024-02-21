cmake_minimum_required(VERSION 3.28)
project(XSKNet)

set(CMAKE_C_COMPILER "clang")

set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(OBJ_PATH ${CMAKE_CURRENT_SOURCE_DIR}/obj)
set(BIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(INC_PATH ${SRC_PATH}/lib)
set(XDP_SRC_PATH ${SRC_PATH}/kern)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -I${INC_PATH} -g -lxdp -lbpf")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0") 
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2") 

set(CMAKE_C_FLAGS_XDP "-O2 -g -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -target bpf -D __BPF_TRACING__ -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -c")

set(SANITIZE "-fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=nullability -fsanitize=integer -fsanitize=shift -fsanitize=unreachable -fsanitize=vla-bound -fsanitize=vptr")

file(GLOB SRC_FILES ${SRC_PATH}/*.c)
file(GLOB LIB_SRC_FILES ${SRC_PATH}/lib/*.c)
file(GLOB XDP_SRC_FILES ${SRC_PATH}/kern/*.c)

add_executable(daemon ${SRC_PATH}/daemon.c ${LIB_SRC_FILES})
add_executable(client ${SRC_PATH}/client.c ${LIB_SRC_FILES})
set_target_properties(daemon client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_PATH})

function(add_xdp_object TARGET FILENAME)
    add_library(${TARGET} OBJECT ${XDP_SRC_PATH}/${FILENAME}.c)
    set_target_properties(${TARGET} PROPERTIES 
        COMPILE_FLAGS ${CMAKE_C_FLAGS_XDP}
    )
endfunction()

add_xdp_object(phy_xdp phy_xdp)
add_xdp_object(inner_xdp inner_xdp)
add_xdp_object(outer_xdp outer_xdp)

add_dependencies(daemon phy_xdp inner_xdp outer_xdp)
add_dependencies(client phy_xdp inner_xdp outer_xdp)

add_custom_command(
    TARGET daemon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:phy_xdp> ${OBJ_PATH}/phy_xdp.o
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:inner_xdp> ${OBJ_PATH}/inner_xdp.o
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:outer_xdp> ${OBJ_PATH}/outer_xdp.o

)

# Clean target
add_custom_target(clean-all COMMAND rm -rf ${OBJ_PATH}/*.o ${BIN_PATH}/*)