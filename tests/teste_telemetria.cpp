#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

#include "modelo/Telemetria.hpp"

namespace {

int falhas = 0;

void verificar(bool ok, const std::string& descricao) {
    std::cout << (ok ? "[ok]    " : "[FALHA] ") << descricao << '\n';
    if (!ok) {
        ++falhas;
    }
}

bool proximo(double valor, double esperado, double tolerancia) {
    return std::fabs(valor - esperado) <= tolerancia;
}

}  // namespace

int main() {
    // Distância: um grau de latitude com raio de 6371 km ≈ 111195 m.
    verificar(proximo(gcs::distancia_entre_m(0, 0, 1, 0), 111194.9, 10.0),
              "um grau de latitude vale ~111195 m");
    verificar(proximo(gcs::distancia_entre_m(-35.36, 149.16, -35.36, 149.16), 0.0, 1e-9),
              "mesmo ponto dá distância zero");

    // Home: sem definição, a distância é zero, mesmo com o drone longe.
    gcs::Home home_indefinido;
    gcs::Posicao longe;
    longe.latitude = 1.0;
    verificar(home_indefinido.distancia_m(longe) == 0.0,
              "home não definido devolve distância zero");

    gcs::Home home_definido;
    home_definido.latitude = 0.0;
    home_definido.longitude = 0.0;
    home_definido.definido = true;
    gcs::Posicao leste;
    leste.latitude = 0.0;
    leste.longitude = 1.0;
    verificar(proximo(home_definido.distancia_m(leste), 111194.9, 10.0),
              "home definido mede a distância até a posição");

    // Segundos entre instantes.
    const gcs::Instante inicio = std::chrono::steady_clock::now();
    const gcs::Instante fim = inicio + std::chrono::milliseconds(1500);
    verificar(proximo(gcs::segundos_entre(inicio, fim), 1.5, 1e-9),
              "segundos_entre converte a diferença de instantes");

    // resumo(): rádio desconhecido aparece como n/d.
    gcs::EstadoVeiculo estado;
    verificar(estado.resumo().find("n/d") != std::string::npos,
              "rádio NaN aparece como n/d no resumo");

    // resumo(): rádio conhecido aparece com o valor.
    estado.enlace.sinal_radio_percent = 95.0;
    verificar(estado.resumo().find("95.0 %") != std::string::npos,
              "rádio conhecido aparece com o valor");

    // resumo(): zero negativo não pode aparecer como -0.0.
    gcs::EstadoVeiculo parado;
    parado.velocidade.subida_ms = -0.0;
    parado.velocidade.solo_ms = -0.0;
    parado.atitude.rolagem = -0.0;
    verificar(parado.resumo().find("-0.0") == std::string::npos,
              "zero negativo é exibido como 0.0");

    std::cout << (falhas == 0 ? "\nTodos os testes de telemetria passaram.\n"
                              : "\nHá testes de telemetria com falha.\n");
    return falhas == 0 ? 0 : 1;
}