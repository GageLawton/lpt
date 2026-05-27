cmake_minimum_required(VERSION 3.16)
project(lpt CXX)

set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/vendor)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/vendor)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RTLSDR librtlsdr)
pkg_check_modules(SDL2 sdl2)

# ── lpt: SDL2 native display ──────────────────────────────────────────────────
if(RTLSDR_FOUND AND SDL2_FOUND)
    set(LPT_SOURCES
        src/main.cpp
        src/radio/rtlsdr.cpp
        src/radio/ring_buffer.cpp
        src/dsp/mag.cpp
        src/dsp/preamble.cpp
        src/dsp/demod.cpp
        src/decoder/modes.cpp
        src/decoder/crc.cpp
        src/decoder/cpr.cpp
        src/decoder/altitude.cpp
        src/decoder/velocity.cpp
        src/decoder/callsign.cpp
        src/tracker/aircraft_table.cpp
        src/renderer/map.cpp
        src/renderer/aircraft.cpp
    )
    add_executable(lpt ${LPT_SOURCES})

    target_include_directories(lpt PRIVATE
        src
        ${RTLSDR_INCLUDE_DIRS}
        ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(lpt PRIVATE
        ${RTLSDR_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${CMAKE_DL_LIBS}
    )

    target_compile_options(lpt PRIVATE -Wall -Wextra -O2)
else()
    message(STATUS "librtlsdr or SDL2 not found — skipping lpt target")
endif()

# ── lpt-web: HTTP server mode (no SDL2 required) ─────────────────────────────
if(RTLSDR_FOUND)
    set(LPT_WEB_SOURCES
        src/main-web.cpp
        src/web/server.cpp
        src/radio/rtlsdr.cpp
        src/radio/ring_buffer.cpp
        src/dsp/mag.cpp
        src/dsp/preamble.cpp
        src/dsp/demod.cpp
        src/decoder/modes.cpp
        src/decoder/crc.cpp
        src/decoder/cpr.cpp
        src/decoder/altitude.cpp
        src/decoder/velocity.cpp
        src/decoder/callsign.cpp
        src/tracker/aircraft_table.cpp
    )

    add_executable(lpt-web ${LPT_WEB_SOURCES})

    target_include_directories(lpt-web PRIVATE
        src
        ${VENDOR_DIR}
        ${RTLSDR_INCLUDE_DIRS}
    )

    target_link_libraries(lpt-web PRIVATE
        ${RTLSDR_LIBRARIES}
        ${CMAKE_DL_LIBS}
    )

    target_compile_options(lpt-web PRIVATE -Wall -Wextra -O2)
else()
    message(STATUS "librtlsdr not found — skipping lpt-web target")
endif()

# ── Tests ──────────────────────────────────────
enable_testing()

add_executable(test_callsign tests/test_callsign.cpp src/decoder/callsign.cpp)
target_include_directories(test_callsign PRIVATE src)
target_compile_options(test_callsign PRIVATE -Wall -Wextra -O2)
add_test(NAME test_callsign COMMAND test_callsign)

foreach(test test_crc test_cpr test_demod)
    add_executable(${test} tests/${test}.cpp src/decoder/crc.cpp
                                             src/decoder/cpr.cpp
                                             src/dsp/demod.cpp)
    target_include_directories(${test} PRIVATE src)
    target_compile_options(${test} PRIVATE -Wall -Wextra -O2)
    add_test(NAME ${test} COMMAND ${test})
endforeach()

add_executable(test_altitude tests/test_altitude.cpp src/decoder/altitude.cpp)
target_include_directories(test_altitude PRIVATE src)
target_compile_options(test_altitude PRIVATE -Wall -Wextra -O2)
add_test(NAME test_altitude COMMAND test_altitude)

add_executable(test_tracker tests/test_tracker.cpp src/tracker/aircraft_table.cpp)
target_include_directories(test_tracker PRIVATE src)
target_compile_options(test_tracker PRIVATE -Wall -Wextra -O2)
add_test(NAME test_tracker COMMAND test_tracker)

add_executable(test_velocity tests/test_velocity.cpp src/decoder/velocity.cpp)
target_include_directories(test_velocity PRIVATE src)
target_compile_options(test_velocity PRIVATE -Wall -Wextra -O2)

add_test(NAME test_velocity COMMAND test_velocity)
add_executable(test_preamble tests/test_preamble.cpp src/dsp/preamble.cpp)
target_include_directories(test_preamble PRIVATE src)
target_compile_options(test_preamble PRIVATE -Wall -Wextra -O2)

add_test(NAME test_preamble COMMAND test_preamble)
add_executable(test_modes tests/test_modes.cpp src/decoder/modes.cpp src/decoder/crc.cpp)
target_include_directories(test_modes PRIVATE src)
target_compile_options(test_modes PRIVATE -Wall -Wextra -O2)

add_test(NAME test_modes COMMAND test_modes)
add_executable(test_mag tests/test_mag.cpp src/dsp/mag.cpp)
target_include_directories(test_mag PRIVATE src)
target_compile_options(test_mag PRIVATE -Wall -Wextra -O2)
add_test(NAME test_mag COMMAND test_mag)

add_executable(test_server_utils tests/test_server_utils.cpp)
target_include_directories(test_server_utils PRIVATE src)
target_compile_options(test_server_utils PRIVATE -Wall -Wextra -O2)
add_test(NAME test_server_utils COMMAND test_server_utils)
