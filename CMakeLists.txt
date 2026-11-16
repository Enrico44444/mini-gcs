cmake_minimum_required(VERSION 3.16)
project(mini_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # ajuda o clangd/VS Code a entender o projeto

# MAVSDK (C++) — instalação explicada em INSTALACAO.md.
# Com o pacote .deb instalado no sistema, o find_package acha sozinho em
# /usr/lib/cmake/MAVSDK. Também procuramos em ~/.local, para quem instalou sem
# sudo; e dá para apontar outro prefixo com -DCMAKE_PREFIX_PATH=<caminho>.
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local")
find_package(MAVSDK REQUIRED)
find_package(Threads REQUIRED)
message(STATUS "MAVSDK ${MAVSDK_VERSION} encontrado em ${MAVSDK_DIR}")

add_executable(mini_gcs
    src/main.cpp
    src/EstacaoDeControle.cpp
    src/modelo/Telemetria.cpp
    src/modelo/Veiculo.cpp
    src/modelo/Drone.cpp
    src/modelo/Historico.cpp
    src/cli/Comando.cpp
    src/cli/ComandosPadrao.cpp
    src/cli/Console.cpp
    src/visual/Painel.cpp
    src/visual/GraficoTempo.cpp
    src/visual/HorizonteArtificial.cpp
)

# Os #include do projeto são relativos a src/ (ex.: "modelo/Veiculo.hpp").
target_include_directories(mini_gcs PRIVATE src)

target_link_libraries(mini_gcs PRIVATE MAVSDK::mavsdk Threads::Threads)
target_compile_definitions(mini_gcs PRIVATE MAVSDK_ENABLE_MAVLINK_C_API)

target_compile_options(mini_gcs PRIVATE -Wall -Wextra -Wpedantic)

# Se o MAVSDK estiver fora dos diretórios padrão do sistema (p.ex. em ~/.local),
# gravamos o caminho da biblioteca no executável para que ele rode sem precisar
# de LD_LIBRARY_PATH.
get_filename_component(MAVSDK_LIB_DIR "${MAVSDK_DIR}/../.." ABSOLUTE)
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${MAVSDK_LIB_DIR}" _indice_sistema)
if(_indice_sistema EQUAL -1)
    set_target_properties(mini_gcs PROPERTIES BUILD_RPATH "${MAVSDK_LIB_DIR}")
endif()


# ---------------------------------------------------------------------------
# Testes automatizados (ctest). Não dependem do MAVSDK: rodam offline.
# ---------------------------------------------------------------------------
enable_testing()

# Cada teste é um executável pequeno que lista só os arquivos de que precisa.
function(adicionar_teste nome)
    add_executable(${nome} ${ARGN})
    target_include_directories(${nome} PRIVATE src)
    target_compile_features(${nome} PRIVATE cxx_std_17)
    target_compile_options(${nome} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${nome} PRIVATE Threads::Threads)
endfunction()

set(FONTES_TELEMETRIA src/modelo/Telemetria.cpp)
set(FONTES_HISTORICO src/modelo/Historico.cpp src/modelo/Telemetria.cpp)
set(FONTES_VEICULO src/modelo/Veiculo.cpp src/modelo/Telemetria.cpp)
set(FONTES_PAINEIS
    src/visual/Painel.cpp
    src/visual/GraficoTempo.cpp
    src/visual/HorizonteArtificial.cpp
    src/modelo/Historico.cpp
    src/modelo/Telemetria.cpp)

adicionar_teste(teste_telemetria tests/teste_telemetria.cpp ${FONTES_TELEMETRIA})
adicionar_teste(teste_historico tests/teste_historico.cpp ${FONTES_HISTORICO})
adicionar_teste(teste_paineis tests/teste_paineis.cpp ${FONTES_PAINEIS})
adicionar_teste(teste_simulado tests/teste_simulado.cpp ${FONTES_VEICULO})

add_test(NAME telemetria COMMAND teste_telemetria)
add_test(NAME historico COMMAND teste_historico)
add_test(NAME paineis COMMAND teste_paineis)
add_test(NAME simulado COMMAND teste_simulado)
set_tests_properties(simulado PROPERTIES TIMEOUT 150 LABELS "lento")