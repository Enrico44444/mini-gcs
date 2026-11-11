#include "modelo/Telemetria.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace gcs {
namespace {

constexpr double RAIO_TERRA_M = 6371000.0;
constexpr double PI = 3.14159265358979323846;

double radianos(double graus) {
    return graus * PI / 180.0;
}

}  // namespace

std::string EstadoVeiculo::resumo() const {
    std::ostringstream fluxo;
    fluxo << std::fixed << std::setprecision(1)
          << "modo=" << std::left << std::setw(18) << modo_de_voo << std::right
          << " armado=" << (armado ? "sim" : "nao")
          << " em_voo=" << (em_voo ? "sim" : "nao")
          << " alt_rel=" << std::setw(7) << posicao.altitude_relativa_m << " m"
          << " vel_solo=" << std::setw(5) << velocidade.solo_ms << " m/s"
          << " vel_subida=" << std::setw(5) << velocidade.subida_ms << " m/s"
          << " dist_home=" << std::setw(7) << home.distancia_m(posicao) << " m"
          << " sat=" << enlace.satelites
          << " radio=" << std::setw(5) << enlace.sinal_radio_percent << " %"
          << " rolagem=" << std::setw(6) << atitude.rolagem << " deg"
          << " arfagem=" << std::setw(6) << atitude.arfagem << " deg"
          << " bat=" << std::setw(5) << bateria.percentual << " %";
    return fluxo.str();
}

double segundos_entre(Instante inicio, Instante fim) {
    return std::chrono::duration<double>(fim - inicio).count();
}

double distancia_entre_m(double latitude1, double longitude1,
                         double latitude2, double longitude2) {
    const double delta_lat = radianos(latitude2 - latitude1);
    const double delta_lon = radianos(longitude2 - longitude1);
    const double a = std::sin(delta_lat / 2) * std::sin(delta_lat / 2) +
                     std::cos(radianos(latitude1)) * std::cos(radianos(latitude2)) *
                         std::sin(delta_lon / 2) * std::sin(delta_lon / 2);
    return 2.0 * RAIO_TERRA_M * std::asin(std::sqrt(a));
}

double Home::distancia_m(const Posicao& posicao) const {
    if (!definido) {
        return 0.0;
    }
    return distancia_entre_m(latitude, longitude, posicao.latitude, posicao.longitude);
}

}  // namespace gcs