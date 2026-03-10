cmake_minimum_required(VERSION 3.10)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSES REQUIRED ncurses)

add_executable(remoteidv1 main.c)

target_include_directories(remoteidv1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libopendroneid
    ${NCURSES_INCLUDE_DIRS}
)

target_link_directories(remoteidv1 PRIVATE ${NCURSES_LIBRARY_DIRS})

target_link_libraries(remoteidv1 PRIVATE opendroneid ${NCURSES_LIBRARIES})

target_compile_options(remoteidv1 PRIVATE ${NCURSES_CFLAGS_OTHER})
