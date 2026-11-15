// Objetos de valor que representam a telemetria de um veículo.
#pragma once

#include <chrono>
#include <limits>
#include <string>

namespace gcs {

using Instante = std::chrono::steady_clock::time_point;

/// Orientação do veículo, em graus (convenção aeronáutica).
struct Atitude {
    double rolagem = 0.0;  // roll  — positivo com a asa direita abaixada
    double arfagem = 0.0;  // pitch — positivo com o nariz acima do horizonte
    double guinada = 0.0;  // yaw   — 0 = norte, cresce para leste
};

/// Posição global informada pelo GPS.
struct Posicao {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude_absoluta_m = 0.0;  // em relação ao nível do mar (AMSL)
    double altitude_relativa_m = 0.0;  // em relação ao ponto de decolagem
};

/// Estado da bateria principal.
struct Bateria {
    double tensao_v = 0.0;
    double percentual = 0.0;  // 0 a 100
};

/// Velocidades do veículo, em m/s.
struct Velocidade {
    double solo_ms = 0.0;    // horizontal, em relação ao solo
    double subida_ms = 0.0;  // positiva quando o veículo sobe
};

/// Qualidade do enlace com o veículo.
struct Enlace {
    int satelites = 0;
    double sinal_radio_percent = std::numeric_limits<double>::quiet_NaN();  // 0 a 100; NaN = desconhecido
};

/// Ponto de decolagem. Serve de referência para a distância de retorno.
struct Home {
    double latitude = 0.0;
    double longitude = 0.0;
    bool definido = false;  // false enquanto o autopiloto não informou o home

    /// Distância horizontal, em metros, entre a posição informada e o home.
    /// Devolve 0 se o home ainda não foi definido.
    double distancia_m(const Posicao& posicao) const;
};

/// Fotografia completa do veículo em um instante.
struct EstadoVeiculo {
    std::string modo_de_voo = "DESCONHECIDO";
    bool armado = false;
    bool em_voo = false;
    Atitude atitude{};
    Posicao posicao{};
    Bateria bateria{};
    Velocidade velocidade{};
    Enlace enlace{};
    Home home{};
    Instante instante = std::chrono::steady_clock::now();

    /// Uma linha legível, usada pelo comando `status`.
    std::string resumo() const;
};

/// Segundos decorridos entre dois instantes.
double segundos_entre(Instante inicio, Instante fim);

/// Distância horizontal, em metros, entre dois pontos (fórmula de haversine).
double distancia_entre_m(double latitude1, double longitude1,
                         double latitude2, double longitude2);

}  // namespace gcs