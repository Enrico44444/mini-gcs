#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>

#include "modelo/Historico.hpp"
#include "visual/GraficoTempo.hpp"
#include "visual/HorizonteArtificial.hpp"

namespace {

int falhas = 0;

void verificar(bool ok, const std::string& descricao) {
    std::cout << (ok ? "[ok]    " : "[FALHA] ") << descricao << '\n';
    if (!ok) {
        ++falhas;
    }
}

const gcs::Instante BASE = std::chrono::steady_clock::now();

gcs::EstadoVeiculo amostra(double altitude, double segundos,
                           const std::string& modo = "STABILIZE") {
    gcs::EstadoVeiculo estado;
    estado.posicao.altitude_relativa_m = altitude;
    estado.modo_de_voo = modo;
    estado.instante = BASE + std::chrono::milliseconds(static_cast<long long>(segundos * 1000.0));
    return estado;
}

std::size_t contar(const std::string& texto, char alvo) {
    return static_cast<std::size_t>(std::count(texto.begin(), texto.end(), alvo));
}

bool contem(const std::string& texto, const std::string& trecho) {
    return texto.find(trecho) != std::string::npos;
}

gcs::GraficoTempo grafico_altitude(double janela_s) {
    return gcs::GraficoTempo(
        gcs::GraficoTempo::Campo{"Altitude relativa", "m",
                                 [](const gcs::EstadoVeiculo& e) {
                                     return e.posicao.altitude_relativa_m;
                                 }},
        70, 16, janela_s);
}

}  // namespace

int main() {
    // --- GraficoTempo ----------------------------------------------------
    {
        gcs::Historico historico;
        verificar(contem(grafico_altitude(60.0).quadro(historico), "aguardando telemetria"),
                  "gráfico sem dados mostra a mensagem de espera");
    }
    {
        gcs::Historico historico;
        historico.registrar(amostra(0.0, 0.0));
        historico.registrar(amostra(5.0, 1.0));
        historico.registrar(amostra(10.0, 2.0));
        const std::string quadro = grafico_altitude(60.0).quadro(historico);
        verificar(contem(quadro, "Altitude relativa (m)"),
                  "gráfico mostra o título com a unidade");
        verificar(contem(quadro, "atual:") && contem(quadro, "10.0 m"),
                  "gráfico mostra o último valor");
        verificar(contar(quadro, '*') == 3,
                  "três amostras em colunas e linhas distintas geram três marcas");
    }
    {
        // Valor constante: a escala ainda existe e os pontos aparecem.
        gcs::Historico historico;
        historico.registrar(amostra(5.0, 0.0));
        historico.registrar(amostra(5.0, 1.0));
        historico.registrar(amostra(5.0, 2.0));
        const std::string quadro = grafico_altitude(60.0).quadro(historico);
        verificar(contar(quadro, '*') == 3,
                  "valor constante ainda desenha as três marcas");
    }
    {
        // Amostras fora da janela de tempo não entram na escala.
        gcs::Historico historico;
        historico.registrar(amostra(100.0, 0.0));
        historico.registrar(amostra(0.0, 100.0));
        const std::string quadro = grafico_altitude(60.0).quadro(historico);
        // O rótulo de cima da escala deve ser ~0.5 (folga sobre o 0), e não 100.
        verificar(contem(quadro, "0.5 |") && !contem(quadro, "100.0 |"),
                  "amostra fora da janela não entra na escala vertical");
        verificar(contar(quadro, '*') == 1,
                  "apenas a amostra dentro da janela é desenhada");
    }

    // --- HorizonteArtificial --------------------------------------------
    {
        gcs::Historico historico;
        const gcs::HorizonteArtificial horizonte;
        verificar(contem(horizonte.quadro(historico), "aguardando telemetria"),
                  "horizonte sem dados mostra a mensagem de espera");
    }
    {
        gcs::Historico historico;
        historico.registrar(amostra(10.0, 0.0, "GUIDED"));
        const gcs::HorizonteArtificial horizonte(72, 21);
        const std::string quadro = horizonte.quadro(historico);
        verificar(contar(quadro, '\n') == 24,
                  "horizonte tem 21 linhas de desenho, título, espaço e rodapé");
        verificar(contem(quadro, "GUIDED"), "horizonte mostra o modo de voo");
        verificar(contem(quadro, "rolagem") && contem(quadro, "arfagem"),
                  "horizonte mostra rolagem e arfagem");
    }
    {
        // Arfagem diferente deve mudar o desenho: é o que move o horizonte.
        gcs::Historico plano;
        gcs::Historico subindo;
        plano.registrar(amostra(0.0, 0.0));
        gcs::EstadoVeiculo nariz_para_cima = amostra(0.0, 0.0);
        nariz_para_cima.atitude.arfagem = 20.0;
        subindo.registrar(nariz_para_cima);
        const gcs::HorizonteArtificial horizonte;
        verificar(horizonte.quadro(plano) != horizonte.quadro(subindo),
                  "arfagem muda o desenho do horizonte");
    }

    std::cout << (falhas == 0 ? "\nTodos os testes dos painéis passaram.\n"
                              : "\nHá testes dos painéis com falha.\n");
    return falhas == 0 ? 0 : 1;
}