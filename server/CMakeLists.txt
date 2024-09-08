cmake_minimum_required(VERSION 3.16)

project(aesdsocket
    LANGUAGES C
    VERSION 1.0.0
    DESCRIPTION "ECEA 5305 Assignment 5 Part 1"
)

set(CMAKE_C_STANDARD 99)

add_executable(${PROJECT_NAME} aesdsocket.c)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Winit-self 
    -Wlogical-op -Wshadow -Wsign-conversion -Wswitch-default -Wundef 
    -Wunused -pedantic
)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_link_libraries(${PROJECT_NAME} rt pthread)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/aesdsocket-start-stop
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/init.d/
    RENAME S99${PROJECT_NAME}
)
