cmake_minimum_required(VERSION 3.0)
project(SweetSocket)

# Define o diretório de saída
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Adiciona opções de compilação
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -g")

# Adiciona a compilação para bibliotecas compartilhadas e estáticas
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Define os diretórios de origem
set(SOURCE_DIR src)
set(SWEETTHREAD_DIR ${SOURCE_DIR}/SweetThread)

# Encontra todos os arquivos fonte
file(GLOB_RECURSE SRC_FILES ${SOURCE_DIR}/*.c)
file(GLOB_RECURSE SWEETTHREAD_FILES ${SWEETTHREAD_DIR}/*.c)

# Cria a biblioteca estática
add_library(SweetSocket_static STATIC ${SRC_FILES} ${SWEETTHREAD_FILES})

# Cria a biblioteca compartilhada
if(WIN32)
    set(SO_EXTENSION dll)
    set(LIBRARY_NAME "SweetSocket.dll")
    target_link_libraries(SweetSocket_static ws2_32)
elseif(UNIX)
    set(SO_EXTENSION so)
    set(LIBRARY_NAME "SweetSocket.so")
    target_link_libraries(SweetSocket_static pthread)
endif()

add_library(SweetSocket SHARED ${SRC_FILES} ${SWEETTHREAD_FILES})

# Define o diretório de cabeçalhos (se necessário)
include_directories(${SOURCE_DIR})
include_directories(${SWEETTHREAD_DIR})

# Define o alvo da biblioteca compartilhada
set_target_properties(SweetSocket PROPERTIES
    OUTPUT_NAME "SweetSocket"
    SUFFIX ${SO_EXTENSION}
)

# Adiciona o executável se necessário
# add_executable(my_executable main.c)
# target_link_libraries(my_executable SweetSocket)

# Alvo de limpeza
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
)

# Comando para limpar
set(CLEAN_COMMAND "rm -rf ${CMAKE_BINARY_DIR}")