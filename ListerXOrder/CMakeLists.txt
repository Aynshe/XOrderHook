cmake_minimum_required(VERSION 3.10)
project(ListerXOrder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Spécifier le chemin de SDL3
set(SDL3_DIR "${CMAKE_SOURCE_DIR}/../SDL3/cmake" CACHE PATH "Chemin vers SDL3")

# Vérifier que le répertoire existe
if(NOT EXISTS "${SDL3_DIR}/SDL3Config.cmake")
    message(FATAL_ERROR "SDL3 non trouvé dans ${SDL3_DIR}. Veuillez vérifier le chemin.")
endif()

# Trouver SDL3
find_package(SDL3 REQUIRED)

# Ajouter l'exécutable
add_executable(ListerXOrder ListerXOrder.cpp)

# Lier les bibliothèques
target_link_libraries(ListerXOrder 
    PRIVATE 
    dinput8
    dxguid
    setupapi
    Cfgmgr32
    hid
    SDL3::SDL3
    SDL3::SDL3main
)
