
cmake_minimum_required(VERSION 3.25)

project(CacheARV C)

set(CMAKE_C_FLAGS "\
  -march=rv64gc \
  -mabi=lp64d \
  -std=gnu99 \
  -D_ISOC11_SOURCE=1 \
  -O0 \
")

SET(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib")

include_directories(${PROJECT_SOURCE_DIR}/include)

set(aesCore
    ${PROJECT_SOURCE_DIR}/src/aes_core.c
)
set(mastikSRC
    ${PROJECT_SOURCE_DIR}/src/l1.c
    ${PROJECT_SOURCE_DIR}/src/mm.c
    ${PROJECT_SOURCE_DIR}/src/prime_probe.c
    ${PROJECT_SOURCE_DIR}/src/util.c
    ${PROJECT_SOURCE_DIR}/src/vlist.c
)

set(ppAES
    ${PROJECT_SOURCE_DIR}/pp_AES.c
    ${PROJECT_SOURCE_DIR}/src/prime_probe.c
)

set(frAES
    ${PROJECT_SOURCE_DIR}/fr_AES.c
    ${PROJECT_SOURCE_DIR}/src/flush_reload.c
)

set(frLastAES
    ${PROJECT_SOURCE_DIR}/fr_last_AES.c
    ${PROJECT_SOURCE_DIR}/src/flush_reload.c
)

set(erAES
    ${PROJECT_SOURCE_DIR}/er_AES.c
    ${PROJECT_SOURCE_DIR}/src/evict.c
)

ADD_LIBRARY(mastik STATIC ${mastikSRC})

add_executable(ppAES ${ppAES} ${aesCore})
target_link_libraries(ppAES mastik)

add_executable(frAES ${frAES} ${aesCore})
target_link_libraries(frAES mastik)

add_executable(frLastAES ${frLastAES} ${aesCore})
target_link_libraries(frLastAES mastik)

add_executable(erAES ${erAES} ${aesCore})
target_link_libraries(erAES mastik)