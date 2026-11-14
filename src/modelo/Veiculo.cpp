#include "modelo/Veiculo.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace gcs {
namespace {

constexpr double PI_RAD = 3.14159265358979323846;

}  // namespace

Veiculo::Veiculo(std::string nome) : nome_(std::move(nome)) {}

EstadoVeiculo Veiculo::estado() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estado_;  // cópia
}

std::string Veiculo::modo_de_voo() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estado_.modo_de_voo;
}

void Veiculo::registrar_observador(Observador observador) {
    std::lock_guard<std::mutex> trava(mutex_);
    observadores_.push_back(std::move(observador));
}

void Veiculo::atualizar(const std::function<void(EstadoVeiculo&)>& modificacao) {
    EstadoVeiculo copia;
    std::vector<Observador> observadores;
    {
        std::lock_guard<std::mutex> trava(mutex_);
        modificacao(estado_);
        estado_.instante = std::chrono::steady_clock::now();
        copia = estado_;
        observadores = observadores_;
    }
    // Os observadores são chamados FORA da trava: se um deles pedir estado()
    // de volta, não travamos o programa.
    for (const Observador& observador : observadores) {
        observador(copia);
    }
}

std::string Veiculo::validar_modo(const std::string& modo) const {
    std::string maiusculo;
    for (char caractere : modo) {
        maiusculo += static_cast<char>(std::toupper(static_cast<unsigned char>(caractere)));
    }

    const std::vector<std::string> aceitos = modos_suportados();
    if (std::find(aceitos.begin(), aceitos.end(), maiusculo) != aceitos.end()) {
        return maiusculo;
    }

    std::string disponiveis;
    for (const std::string& aceito : aceitos) {
        disponiveis += (disponiveis.empty() ? "" : ", ") + aceito;
    }
    throw ErroDeModo("modo '" + modo + "' inválido; disponíveis: " + disponiveis);
}

std::ostream& operator<<(std::ostream& fluxo, const Veiculo& veiculo) {
    return fluxo << veiculo.nome() << ": " << veiculo.estado().resumo();
}

const std::vector<std::string>& modos_de_voo_conhecidos() {
    static const std::vector<std::string> modos{
        "STABILIZE", "ALT_HOLD", "LOITER", "GUIDED", "AUTO", "RTL", "LAND"};
    return modos;
}

// ---------------------------------------------------------------------------
// VeiculoSimulado
// ---------------------------------------------------------------------------
VeiculoSimulado::VeiculoSimulado(std::string nome) : Veiculo(std::move(nome)) {}

VeiculoSimulado::~VeiculoSimulado() {
    parar_ = true;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void VeiculoSimulado::conectar() {
    EstadoVeiculo estado;
    {
        std::lock_guard<std::mutex> trava(sim_mutex_);
        modo_ = "STABILIZE";
        estado = montar_estado_locked();
    }
    publicar(estado);
}

void VeiculoSimulado::iniciar_telemetria() {
    thread_ = std::thread(&VeiculoSimulado::laco_de_telemetria, this);
}

void VeiculoSimulado::laco_de_telemetria() {
    Instante anterior = std::chrono::steady_clock::now();
    while (!parar_) {
        const Instante agora = std::chrono::steady_clock::now();
        // Limita o passo para não dar saltos se a thread atrasar.
        const double dt = std::min(segundos_entre(anterior, agora), 0.2);
        anterior = agora;

        EstadoVeiculo estado;
        {
            std::lock_guard<std::mutex> trava(sim_mutex_);
            avancar_locked(dt);
            estado = montar_estado_locked();
        }
        publicar(estado);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void VeiculoSimulado::publicar(const EstadoVeiculo& estado) {
    atualizar([&estado](EstadoVeiculo& atual) { atual = estado; });
}

EstadoVeiculo VeiculoSimulado::montar_estado_locked() const {
    EstadoVeiculo estado;
    estado.modo_de_voo = modo_;
    estado.armado = armado_;
    estado.em_voo = em_voo_;

    // Atitude: parada no solo. Em voo, balança levemente e inclina o nariz
    // para baixo quando está se deslocando.
    if (em_voo_) {
        estado.atitude.rolagem = 2.0 * std::sin(tempo_s_ / 2.0);
        estado.atitude.arfagem = (tem_destino_ ? -5.0 : 0.0) + std::sin(tempo_s_ / 3.0);
    }
    estado.atitude.guinada = guinada_;

    estado.posicao.latitude = latitude_;
    estado.posicao.longitude = longitude_;
    estado.posicao.altitude_absoluta_m = ALTITUDE_SOLO_AMSL_M + altitude_m_;
    estado.posicao.altitude_relativa_m = altitude_m_;

    estado.bateria.percentual = bateria_pct_;
    estado.bateria.tensao_v = 11.0 + 1.4 * (bateria_pct_ / 100.0);

    estado.velocidade.solo_ms = vel_solo_ms_;
    estado.velocidade.subida_ms = vel_subida_ms_;

    estado.enlace.satelites = 14;
    estado.enlace.sinal_radio_percent = 95.0;

    estado.home.latitude = latitude_home_;
    estado.home.longitude = longitude_home_;
    estado.home.definido = true;
    return estado;
}

void VeiculoSimulado::avancar_locked(double dt) {
    if (dt <= 0.0) {
        return;
    }
    tempo_s_ += dt;
    vel_solo_ms_ = 0.0;
    vel_subida_ms_ = 0.0;

    // Desarmado no solo: nada se move.
    if (!armado_) {
        return;
    }

    // 1. Vertical: sobe ou desce em velocidade constante até a altitude alvo.
    if (altitude_m_ < altitude_alvo_m_) {
        const double passo = std::min(VELOCIDADE_SUBIDA_MS * dt, altitude_alvo_m_ - altitude_m_);
        altitude_m_ += passo;
        vel_subida_ms_ = passo / dt;
    } else if (altitude_m_ > altitude_alvo_m_) {
        const double passo = std::min(VELOCIDADE_DESCIDA_MS * dt, altitude_m_ - altitude_alvo_m_);
        altitude_m_ -= passo;
        vel_subida_ms_ = -passo / dt;
    }
    if (altitude_m_ > 0.5) {
        em_voo_ = true;
    }

    // 2. Pouso concluído: no chão, o veículo desarma.
    if (pousando_ && altitude_m_ <= 0.0) {
        altitude_m_ = 0.0;
        em_voo_ = false;
        armado_ = false;
        pousando_ = false;
        tem_destino_ = false;
        return;
    }

    // 3. Horizontal: avança em direção ao destino, se houver.
    if (em_voo_ && tem_destino_) {
        const double distancia =
            distancia_entre_m(latitude_, longitude_, latitude_alvo_, longitude_alvo_);
        if (distancia <= 0.5) {
            latitude_ = latitude_alvo_;
            longitude_ = longitude_alvo_;
            tem_destino_ = false;
        } else {
            // Rumo calculado antes de mover, em graus a partir do norte.
            const double dn = (latitude_alvo_ - latitude_) * METROS_POR_GRAU;
            const double de = (longitude_alvo_ - longitude_) * METROS_POR_GRAU *
                              std::cos(latitude_ * PI_RAD / 180.0);
            guinada_ = std::fmod(std::atan2(de, dn) * 180.0 / PI_RAD + 360.0, 360.0);

            const double passo = std::min(VELOCIDADE_HORIZONTAL_MS * dt, distancia);
            const double fracao = passo / distancia;
            latitude_ += (latitude_alvo_ - latitude_) * fracao;
            longitude_ += (longitude_alvo_ - longitude_) * fracao;
            vel_solo_ms_ = passo / dt;
        }
    }

    // 4. RTL: ao chegar ao home, começa o pouso.
    if (em_voo_ && modo_ == "RTL" && !tem_destino_ && !pousando_) {
        pousando_ = true;
        altitude_alvo_m_ = 0.0;
    }

    // 5. Bateria: consome só enquanto está voando.
    if (em_voo_) {
        bateria_pct_ = std::max(0.0, bateria_pct_ - CONSUMO_BATERIA_POR_S * dt);
    }
}

void VeiculoSimulado::pousar_locked() {
    if (!em_voo_) {
        throw ErroDeVeiculo("o veículo não está em voo");
    }
    modo_ = "LAND";
    tem_destino_ = false;
    pousando_ = true;
    altitude_alvo_m_ = 0.0;
}

void VeiculoSimulado::retornar_locked() {
    if (!em_voo_) {
        throw ErroDeVeiculo("o veículo não está em voo");
    }
    modo_ = "RTL";
    pousando_ = false;
    tem_destino_ = true;
    latitude_alvo_ = latitude_home_;
    longitude_alvo_ = longitude_home_;
    // Sobe até a altura de segurança do retorno, se estiver mais baixo.
    altitude_alvo_m_ = std::max(altitude_m_, ALTURA_RTL_M);
}

std::vector<std::string> VeiculoSimulado::modos_suportados() const {
    return modos_de_voo_conhecidos();
}

void VeiculoSimulado::alterar_modo_de_voo(const std::string& modo) {
    const std::string alvo = validar_modo(modo);  // lança ErroDeModo se inválido

    EstadoVeiculo estado;
    {
        std::lock_guard<std::mutex> trava(sim_mutex_);
        if (alvo == "RTL") {
            retornar_locked();
        } else if (alvo == "LAND") {
            pousar_locked();
        } else {
            modo_ = alvo;
            // Modos que mantêm a posição cancelam o destino anterior.
            if (alvo == "STABILIZE" || alvo == "ALT_HOLD" || alvo == "LOITER") {
                tem_destino_ = false;
            }
        }
        estado = montar_estado_locked();
    }
    publicar(estado);
}

void VeiculoSimulado::armar() {
    EstadoVeiculo estado;
    {
        std::lock_guard<std::mutex> trava(sim_mutex_);
        if (em_voo_) {
            throw ErroDeVeiculo("não é possível armar com o veículo em voo");
        }
        armado_ = true;
        estado = montar_estado_locked();
    }
    publicar(estado);
}

void VeiculoSimulado::desarmar() {
    EstadoVeiculo estado;
    {
        std::lock_guard<std::mutex> trava(sim_mutex_);
        if (em_voo_) {
            throw ErroDeVeiculo("não é possível desarmar com o veículo em voo");
        }
        armado_ = false;
        estado = montar_estado_locked();
    }
    publicar(estado);
}

void VeiculoSimulado::decolar(double altura_m) {
    if (altura_m <= 0.0) {
        throw std::invalid_argument("a altura de decolagem deve ser maior que zero");
    }
    EstadoVeiculo estado;
    {
        std::lock_guard<std::mutex> trava(sim_mutex_);
        if (!armado_) {
            throw ErroDeVeiculo("é preciso armar antes de decolar");
        }
        if (em_voo_) {
            throw ErroDeVeiculo("o veículo já está em voo");
        }
        // O ponto de decolagem vira o home, como no ArduPilot.
        latitude_home_ = latitude_;
        longitude_home_ = longitude_;
        modo_ = "GUIDED";
        pousando_ = false;
        tem_destino_ = false;
        altitude_alvo_m_ = altura_m;
        estado = montar_estado_locked();
    }
    publicar(estado);
}

void VeiculoSimulado::pousar() {
    EstadoVeiculo estado;
    {
        std::lock_guard<std::mutex> trava(sim_mutex_);
        pousar_locked();
        estado = montar_estado_locked();
    }
    publicar(estado);
}

void VeiculoSimulado::retornar_para_casa() {
    EstadoVeiculo estado;
    {
        std::lock_guard<std::mutex> trava(sim_mutex_);
        retornar_locked();
        estado = montar_estado_locked();
    }
    publicar(estado);
}

void VeiculoSimulado::ir_para(double latitude, double longitude, double altitude_m) {
    if (altitude_m <= 0.0) {
        throw std::invalid_argument("a altitude deve ser maior que zero");
    }
    EstadoVeiculo estado;
    {
        std::lock_guard<std::mutex> trava(sim_mutex_);
        if (!em_voo_) {
            throw ErroDeVeiculo("o veículo não está em voo");
        }
        modo_ = "GUIDED";
        pousando_ = false;
        tem_destino_ = true;
        latitude_alvo_ = latitude;
        longitude_alvo_ = longitude;
        altitude_alvo_m_ = altitude_m;
        estado = montar_estado_locked();
    }
    publicar(estado);
}

}  // namespace gcs