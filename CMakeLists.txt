cmake_minimum_required(VERSION 3.10)
project(SweetSocket C)

# Define a versão mínima do CMake
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Diretórios
set(OUT_DIR ${CMAKE_BINARY_DIR}/build)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(SWEETTHREAD_DIR ${SRC_DIR}/SweetThread)
set(LIB_NAME SweetSocket)

# Cria o diretório de build se não existir
file(MAKE_DIRECTORY ${OUT_DIR})

# Adiciona os diretórios de fontes
file(GLOB SRC_FILES ${SRC_DIR}/*.c ${SWEETTHREAD_DIR}/*.c)

# Adiciona a biblioteca
add_library(${LIB_NAME} STATIC ${SRC_FILES})

# Adiciona diretórios de include
include_directories(${SRC_DIR} ${SWEETTHREAD_DIR})

# Define as flags de compilação para o Windows e Linux
if(WIN32)
    target_link_libraries(${LIB_NAME} ws2_32)
elseif(UNIX)
    target_link_libraries(${LIB_NAME} pthread)
endif()
