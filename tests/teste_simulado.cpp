#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include "modelo/Veiculo.hpp"

namespace {

constexpr double LAT_BASE = -35.363262;
constexpr double LON_BASE = 149.165237;

using Condicao = std::function<bool(const gcs::EstadoVeiculo&)>;

bool aguardar(const gcs::Veiculo& veiculo, const Condicao& condicao, double limite_s) {
    const auto inicio = std::chrono::steady_clock::now();
    while (gcs::segundos_entre(inicio, std::chrono::steady_clock::now()) < limite_s) {
        if (condicao(veiculo.estado())) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return condicao(veiculo.estado());
}

}  // namespace

int main() {
    gcs::VeiculoSimulado veiculo;
    veiculo.conectar();
    veiculo.iniciar_telemetria();

    int falhas = 0;
    auto verificar = [&falhas](bool ok, const std::string& descricao) {
        std::cout << (ok ? "[ok]    " : "[FALHA] ") << descricao << '\n';
        if (!ok) {
            ++falhas;
        }
    };
    auto mostrar = [&veiculo](const std::string& rotulo) {
        std::cout << "        " << rotulo << ": " << veiculo.estado().resumo() << '\n';
    };

    // Comandos fora de ordem devem ser recusados.
    bool recusou = false;
    try { veiculo.decolar(10); } catch (const gcs::ErroDeVeiculo&) { recusou = true; }
    verificar(recusou, "decolar sem armar é recusado");

    recusou = false;
    try { veiculo.pousar(); } catch (const gcs::ErroDeVeiculo&) { recusou = true; }
    verificar(recusou, "pousar no solo é recusado");

    bool modo_recusado = false;
    try { veiculo.alterar_modo_de_voo("HOLD"); } catch (const gcs::ErroDeModo&) { modo_recusado = true; }
    verificar(modo_recusado, "modo HOLD é recusado");

    // Sequência principal: armar, decolar, deslocar, RTL.
    veiculo.armar();
    verificar(aguardar(veiculo, [](const gcs::EstadoVeiculo& e) { return e.armado; }, 2),
              "armar liga o estado armado");

    veiculo.decolar(10);
    verificar(aguardar(veiculo,
                       [](const gcs::EstadoVeiculo& e) {
                           return e.em_voo && e.posicao.altitude_relativa_m > 9.9;
                       },
                       15),
              "decolagem atinge 10 m");
    mostrar("em voo");

    const double destino = LAT_BASE + 0.00045;  // ~50 m ao norte do home
    veiculo.ir_para(destino, LON_BASE, 10.0);
    verificar(aguardar(veiculo,
                       [](const gcs::EstadoVeiculo& e) {
                           return e.posicao.latitude >= LAT_BASE + 0.00044;
                       },
                       30),
              "ir_para leva o veículo ao destino");
    const gcs::EstadoVeiculo no_destino = veiculo.estado();
    const double distancia = no_destino.home.distancia_m(no_destino.posicao);
    verificar(distancia > 45.0 && distancia < 55.0, "distância ao home ~50 m no destino");
    mostrar("no destino");

    veiculo.alterar_modo_de_voo("rtl");
    verificar(aguardar(veiculo,
                       [](const gcs::EstadoVeiculo& e) { return !e.em_voo && !e.armado; },
                       90),
              "RTL retorna, pousa e desarma");

    const gcs::EstadoVeiculo fim = veiculo.estado();
    verificar(fim.home.distancia_m(fim.posicao) < 2.0, "pouso a menos de 2 m do home");
    verificar(fim.bateria.percentual < 100.0 && fim.bateria.percentual > 90.0,
              "bateria consumida durante o voo");
    mostrar("pousado");

    std::cout << (falhas == 0 ? "\nTodos os testes passaram.\n"
                              : "\nHá testes com falha.\n");
    return falhas == 0 ? 0 : 1;
}