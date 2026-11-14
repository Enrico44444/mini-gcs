#include "modelo/Drone.hpp"

#include <mavsdk/mavsdk.hpp>
#include <mavsdk/plugins/action/action.hpp>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.hpp>
#include <mavsdk/plugins/telemetry/telemetry.hpp>

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace gcs {
namespace {

/// Modos do ArduCopter que o MAVSDK não expõe como ação. São enviados com
/// MAV_CMD_DO_SET_MODE; o número é o `custom_mode` do ArduCopter.
/// RTL e LAND ficam de fora porque têm ação própria no MAVSDK.
const std::map<std::string, std::uint32_t> MODOS_CUSTOMIZADOS{
    {"STABILIZE", 0},
    {"ALT_HOLD", 2},
    {"AUTO", 3},
    {"GUIDED", 4},
    {"LOITER", 5},
};

constexpr std::uint16_t MAV_CMD_DO_SET_MODE = 176;
constexpr float MAV_MODE_FLAG_CUSTOM_MODE_ENABLED = 1.0f;
constexpr std::uint8_t COMPONENTE_AUTOPILOTO = 1;

/// O MAVSDK imprime os enums com operator<<; é a forma mais simples de
/// transformar um valor em texto.
template <typename T>
std::string para_texto(const T& valor) {
    std::ostringstream fluxo;
    fluxo << valor;
    return fluxo.str();
}

/// Converte o resultado de uma operação do MAVSDK em exceção quando não deu certo.
template <typename Resultado>
void verificar(Resultado resultado, const std::string& operacao) {
    if (resultado != Resultado::Success) {
        throw ErroDeVeiculo(operacao + ": " + para_texto(resultado));
    }
}

/// Traduz o modo informado pela telemetria para o nome usado no projeto,
/// para que `status` e `modo` falem a mesma língua.
std::string nome_do_modo(mavsdk::Telemetry::FlightMode modo) {
    using Modo = mavsdk::Telemetry::FlightMode;
    switch (modo) {
        case Modo::Stabilized:      return "STABILIZE";
        case Modo::Altctl:          return "ALT_HOLD";
        case Modo::Hold:            return "LOITER";
        case Modo::Offboard:        return "GUIDED";
        case Modo::Mission:         return "AUTO";
        case Modo::ReturnToLaunch:  return "RTL";
        case Modo::Land:            return "LAND";
        default:                    return para_texto(modo);
    }
}

}  // namespace

std::string normalizar_endereco(const std::string& endereco) {
    if (endereco.find("://") != std::string::npos) {
        return endereco;  // já está no formato de URL
    }
    const std::size_t separador = endereco.find(':');
    if (separador == std::string::npos) {
        return endereco;
    }
    std::string esquema = endereco.substr(0, separador);
    const std::string resto = endereco.substr(separador + 1);
    // "tcp" é ambíguo para o MAVSDK; para falar com o SITL queremos "tcpout".
    if (esquema == "tcp") {
        esquema = "tcpout";
    }
    return esquema + "://" + resto;
}

/// Tudo que depende do MAVSDK vive aqui dentro.
class Drone::Impl {
public:
    Impl()
        : nucleo(mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation}) {}

    mavsdk::Mavsdk nucleo;
    std::shared_ptr<mavsdk::System> sistema;
    std::unique_ptr<mavsdk::Telemetry> telemetria;
    std::unique_ptr<mavsdk::Action> acoes;
    std::unique_ptr<mavsdk::MavlinkPassthrough> passthrough;
    // Cada assinatura guarda uma função que a cancela. Elas são chamadas no
    // destrutor de Drone, antes de o MAVSDK ser desligado.
    std::vector<std::function<void()>> cancelamentos;
};

Drone::Drone(std::string nome, std::string endereco)
    : Veiculo(std::move(nome)),
      impl_(std::make_unique<Impl>()),
      endereco_(std::move(endereco)) {}

Drone::~Drone() {
    // Cancela as assinaturas enquanto o telemetria ainda existe, para que
    // nenhum callback use o objeto depois da destruição.
    for (const std::function<void()>& cancelar : impl_->cancelamentos) {
        cancelar();
    }
}

void Drone::exigir_conexao() const {
    if (!impl_->acoes || !impl_->telemetria || !impl_->passthrough) {
        throw ErroDeVeiculo("operação antes de conectar()");
    }
}

void Drone::conectar() {
    const std::string url = normalizar_endereco(endereco_);
    std::cout << "[" << nome() << "] conectando em " << url << " ..." << std::endl;

    const mavsdk::ConnectionResult resultado = impl_->nucleo.add_any_connection(url);
    if (resultado != mavsdk::ConnectionResult::Success) {
        throw ErroDeVeiculo("não foi possível abrir " + url + ": " + para_texto(resultado));
    }

    // O primeiro autopiloto que aparecer na rede MAVLink é o nosso drone.
    const auto sistema = impl_->nucleo.first_autopilot(TEMPO_LIMITE_S);
    if (!sistema) {
        throw ErroDeVeiculo(
            "nenhum autopiloto respondeu em " + url + ". O SITL do ArduPilot está rodando?");
    }

    impl_->sistema = sistema.value();
    impl_->telemetria = std::make_unique<mavsdk::Telemetry>(impl_->sistema);
    impl_->acoes = std::make_unique<mavsdk::Action>(impl_->sistema);
    impl_->passthrough = std::make_unique<mavsdk::MavlinkPassthrough>(impl_->sistema);
    std::cout << "[" << nome() << "] conectado." << std::endl;
}

void Drone::iniciar_telemetria() {
    exigir_conexao();
    mavsdk::Telemetry& telemetria = *impl_->telemetria;

    // Cada assinatura é um callback chamado pela thread do MAVSDK. Traduzimos
    // o dado da biblioteca para o NOSSO modelo, para que o resto do programa
    // não dependa do MAVSDK.

    auto h_modo = telemetria.subscribe_flight_mode([this](mavsdk::Telemetry::FlightMode modo) {
        const std::string texto = nome_do_modo(modo);
        atualizar([&texto](EstadoVeiculo& estado) { estado.modo_de_voo = texto; });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_modo] { telemetria.unsubscribe_flight_mode(h_modo); });

    auto h_atitude = telemetria.subscribe_attitude_euler([this](mavsdk::Telemetry::EulerAngle angulo) {
        atualizar([&angulo](EstadoVeiculo& estado) {
            estado.atitude.rolagem = angulo.roll_deg;
            estado.atitude.arfagem = angulo.pitch_deg;
            estado.atitude.guinada = angulo.yaw_deg;
        });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_atitude] { telemetria.unsubscribe_attitude_euler(h_atitude); });

    auto h_posicao = telemetria.subscribe_position([this](mavsdk::Telemetry::Position posicao) {
        atualizar([&posicao](EstadoVeiculo& estado) {
            estado.posicao.latitude = posicao.latitude_deg;
            estado.posicao.longitude = posicao.longitude_deg;
            estado.posicao.altitude_absoluta_m = posicao.absolute_altitude_m;
            estado.posicao.altitude_relativa_m = posicao.relative_altitude_m;
        });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_posicao] { telemetria.unsubscribe_position(h_posicao); });

    auto h_bateria = telemetria.subscribe_battery([this](mavsdk::Telemetry::Battery bateria) {
        atualizar([&bateria](EstadoVeiculo& estado) {
            estado.bateria.tensao_v = bateria.voltage_v;
            // No MAVSDK 4.x remaining_percent já vem de 0 a 100.
            estado.bateria.percentual = bateria.remaining_percent;
        });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_bateria] { telemetria.unsubscribe_battery(h_bateria); });

    auto h_armado = telemetria.subscribe_armed([this](bool armado) {
        atualizar([armado](EstadoVeiculo& estado) { estado.armado = armado; });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_armado] { telemetria.unsubscribe_armed(h_armado); });

    auto h_em_voo = telemetria.subscribe_in_air([this](bool em_voo) {
        atualizar([em_voo](EstadoVeiculo& estado) { estado.em_voo = em_voo; });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_em_voo] { telemetria.unsubscribe_in_air(h_em_voo); });

    // Velocidade: o MAVSDK entrega as componentes NED (norte, leste, baixo).
    // A velocidade no solo é o módulo do vetor horizontal; a subida é o oposto
    // da componente "para baixo".
    auto h_velocidade = telemetria.subscribe_velocity_ned([this](mavsdk::Telemetry::VelocityNed v) {
        atualizar([v](EstadoVeiculo& estado) {
            estado.velocidade.solo_ms = std::hypot(v.north_m_s, v.east_m_s);
            estado.velocidade.subida_ms = -v.down_m_s;
        });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_velocidade] { telemetria.unsubscribe_velocity_ned(h_velocidade); });

    auto h_gps = telemetria.subscribe_gps_info([this](mavsdk::Telemetry::GpsInfo gps) {
        atualizar([gps](EstadoVeiculo& estado) { estado.enlace.satelites = gps.num_satellites; });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_gps] { telemetria.unsubscribe_gps_info(h_gps); });

    // Sinal de rádio: quando o autopiloto não sabe o valor, ele vem como NaN.
    // Guardamos 0 nesse caso.
    auto h_rc = telemetria.subscribe_rc_status([this](mavsdk::Telemetry::RcStatus rc) {
        atualizar([rc](EstadoVeiculo& estado) {
            estado.enlace.sinal_radio_percent =
                std::isnan(rc.signal_strength_percent) ? 0.0 : rc.signal_strength_percent;
        });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_rc] { telemetria.unsubscribe_rc_status(h_rc); });

    // Home: o ponto de decolagem informado pelo autopiloto.
    auto h_home = telemetria.subscribe_home([this](mavsdk::Telemetry::HomePosition casa) {
        atualizar([casa](EstadoVeiculo& estado) {
            estado.home.latitude = casa.latitude_deg;
            estado.home.longitude = casa.longitude_deg;
            estado.home.definido = true;
        });
    });
    impl_->cancelamentos.push_back(
        [&telemetria, h_home] { telemetria.unsubscribe_home(h_home); });
}

std::vector<std::string> Drone::modos_suportados() const {
    return modos_de_voo_conhecidos();
}

void Drone::alterar_modo_de_voo(const std::string& modo) {
    exigir_conexao();
    // validar_modo lança ErroDeModo com a lista de modos aceitos.
    const std::string alvo = validar_modo(modo);

    if (alvo == "RTL") {
        retornar_para_casa();
    } else if (alvo == "LAND") {
        pousar();
    } else {
        enviar_modo_customizado(MODOS_CUSTOMIZADOS.at(alvo));
    }
}

void Drone::enviar_modo_customizado(std::uint32_t numero) {
    mavsdk::MavlinkPassthrough::CommandLong comando{};
    comando.target_sysid = impl_->sistema->get_system_id();
    comando.target_compid = COMPONENTE_AUTOPILOTO;
    comando.command = MAV_CMD_DO_SET_MODE;
    comando.param1 = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;
    comando.param2 = static_cast<float>(numero);
    verificar(impl_->passthrough->send_command_long(comando), "trocar modo");
}

void Drone::armar() {
    exigir_conexao();
    verificar(impl_->acoes->arm(), "armar");
}

void Drone::desarmar() {
    exigir_conexao();
    verificar(impl_->acoes->disarm(), "desarmar");
}

void Drone::decolar(double altura_m) {
    exigir_conexao();
    if (altura_m <= 0.0) {
        throw std::invalid_argument("a altura de decolagem deve ser maior que zero");
    }
    verificar(impl_->acoes->set_takeoff_altitude(static_cast<float>(altura_m)),
              "ajustar altura de decolagem");
    verificar(impl_->acoes->takeoff(), "decolar");
}

void Drone::pousar() {
    exigir_conexao();
    verificar(impl_->acoes->land(), "pousar");
}

void Drone::retornar_para_casa() {
    exigir_conexao();
    verificar(impl_->acoes->return_to_launch(), "retornar para casa");
}

void Drone::ir_para(double latitude, double longitude, double altitude_m) {
    exigir_conexao();
    const EstadoVeiculo atual = estado();

    // goto_location espera altitude absoluta (acima do nível do mar). A altitude
    // pedida é relativa ao ponto de decolagem, então somamos a altitude do solo.
    const double solo_m = atual.posicao.altitude_absoluta_m - atual.posicao.altitude_relativa_m;

    // Mantemos a orientação atual para o drone não girar durante o trajeto.
    verificar(impl_->acoes->goto_location(latitude, longitude,
                                          static_cast<float>(solo_m + altitude_m),
                                          static_cast<float>(atual.atitude.guinada)),
              "ir para a posição");
}

}  // namespace gcs