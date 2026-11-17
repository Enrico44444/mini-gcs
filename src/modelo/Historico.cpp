#include "modelo/Historico.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace gcs {
namespace {

constexpr const char* CABECALHO_CSV =
    "tempo_s,modo_de_voo,armado,em_voo,latitude,longitude,altitude_abs_m,altitude_rel_m,"
    "rolagem_deg,arfagem_deg,guinada_deg,bateria_pct,tensao_v,vel_solo_ms,vel_subida_ms,"
    "satelites,radio_pct,home_lat,home_lon,home_definido,dist_home_m";

/// Campo CSV: entre aspas, com as aspas internas duplicadas. Assim o modo de voo
/// pode conter vírgulas sem quebrar a linha.
std::string campo_csv(const std::string& texto) {
    std::string resultado = "\"";
    for (char caractere : texto) {
        if (caractere == '"') {
            resultado += "\"\"";
        } else {
            resultado += caractere;
        }
    }
    return resultado + "\"";
}

}  // namespace

Historico::Historico(std::size_t capacidade) : capacidade_(capacidade) {}

void Historico::registrar(const EstadoVeiculo& estado) {
    std::lock_guard<std::mutex> trava(mutex_);
    estados_.push_back(estado);
    while (estados_.size() > capacidade_) {
        estados_.pop_front();
    }
}

std::size_t Historico::tamanho() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estados_.size();
}

bool Historico::vazio() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estados_.empty();
}

EstadoVeiculo Historico::ultimo(bool* ok) const {
    std::lock_guard<std::mutex> trava(mutex_);
    if (estados_.empty()) {
        if (ok != nullptr) {
            *ok = false;
        }
        return EstadoVeiculo{};
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return estados_.back();
}

Historico::Serie Historico::serie(const Extrator& extrator) const {
    std::lock_guard<std::mutex> trava(mutex_);
    Serie resultado;
    if (estados_.empty()) {
        return resultado;
    }
    const Instante t0 = estados_.front().instante;
    resultado.segundos.reserve(estados_.size());
    resultado.valores.reserve(estados_.size());
    for (const EstadoVeiculo& estado : estados_) {
        resultado.segundos.push_back(segundos_entre(t0, estado.instante));
        resultado.valores.push_back(extrator(estado));
    }
    return resultado;
}

void Historico::limpar() {
    std::lock_guard<std::mutex> trava(mutex_);
    estados_.clear();
}

std::size_t Historico::escrever_csv(std::ostream& saida) const {
    // Copia as amostras sob a trava e escreve fora dela, para a escrita em disco
    // não atrasar o registro de novas amostras pela telemetria.
    std::deque<EstadoVeiculo> copia;
    {
        std::lock_guard<std::mutex> trava(mutex_);
        copia = estados_;
    }

    saida << CABECALHO_CSV << '\n';
    if (copia.empty()) {
        return 0;
    }
    const Instante t0 = copia.front().instante;
    saida << std::fixed << std::setprecision(6);
    for (const EstadoVeiculo& estado : copia) {
        saida << segundos_entre(t0, estado.instante) << ','
              << campo_csv(estado.modo_de_voo) << ','
              << (estado.armado ? 1 : 0) << ',' << (estado.em_voo ? 1 : 0) << ','
              << estado.posicao.latitude << ',' << estado.posicao.longitude << ','
              << estado.posicao.altitude_absoluta_m << ','
              << estado.posicao.altitude_relativa_m << ','
              << estado.atitude.rolagem << ',' << estado.atitude.arfagem << ','
              << estado.atitude.guinada << ','
              << estado.bateria.percentual << ',' << estado.bateria.tensao_v << ','
              << estado.velocidade.solo_ms << ',' << estado.velocidade.subida_ms << ','
              << estado.enlace.satelites << ',';
        // Rádio desconhecido (NaN) vira campo vazio, e não um número falso.
        if (!std::isnan(estado.enlace.sinal_radio_percent)) {
            saida << estado.enlace.sinal_radio_percent;
        }
        saida << ',' << estado.home.latitude << ',' << estado.home.longitude << ','
              << (estado.home.definido ? 1 : 0) << ','
              << estado.home.distancia_m(estado.posicao) << '\n';
    }
    return copia.size();
}

std::size_t Historico::exportar_csv(const std::string& caminho) const {
    std::ofstream arquivo(caminho);
    if (!arquivo) {
        throw std::runtime_error("não foi possível criar o arquivo '" + caminho + "'");
    }
    const std::size_t amostras = escrever_csv(arquivo);
    arquivo.close();
    if (!arquivo) {
        throw std::runtime_error("erro ao gravar o arquivo '" + caminho + "'");
    }
    return amostras;
}

}  // namespace gcs