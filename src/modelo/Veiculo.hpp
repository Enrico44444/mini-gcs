// Classe base de todos os veículos conhecidos pela estação de controle.
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "modelo/Telemetria.hpp"

namespace gcs {

/// Erro ao falar com o veículo (conexão, comando rejeitado...).
class ErroDeVeiculo : public std::runtime_error {
public:
    explicit ErroDeVeiculo(const std::string& mensagem) : std::runtime_error(mensagem) {}
};

/// Modo de voo que o usuário pediu e que esta implementação não aceita.
/// É um erro de uso, por isso é diferente de NaoImplementado.
class ErroDeModo : public std::invalid_argument {
public:
    explicit ErroDeModo(const std::string& mensagem) : std::invalid_argument(mensagem) {}
};

/// Operação que ainda é exercício da disciplina. O console trata esta exceção
/// de forma amigável, então dá para registrar o esqueleto de um recurso antes
/// de implementá-lo.
class NaoImplementado : public std::logic_error {
public:
    explicit NaoImplementado(const std::string& o_que)
        : std::logic_error(o_que) {}
};

/// Contrato comum a qualquer veículo: multirrotor, asa fixa, barco, simulador.
///
/// A classe base resolve três responsabilidades genéricas:
///
///  1. guardar o último EstadoVeiculo conhecido, com acesso seguro entre threads;
///  2. avisar os interessados a cada atualização (padrão *Observer*) — é assim
///     que o histórico e os painéis recebem dados sem conhecer o MAVSDK;
///  3. declarar as operações que toda subclasse precisa oferecer.
///
/// O que é específico de um protocolo fica nas subclasses: veja gcs::Drone
/// (MAVSDK + ArduPilot) e gcs::VeiculoSimulado (dados falsos para testes).
class Veiculo {
public:
    /// Quem quer ser avisado quando o estado muda.
    /// Atenção: é chamado na thread da telemetria, não na do console.
    using Observador = std::function<void(const EstadoVeiculo&)>;

    explicit Veiculo(std::string nome);
    virtual ~Veiculo() = default;

    Veiculo(const Veiculo&) = delete;
    Veiculo& operator=(const Veiculo&) = delete;

    // --- leitura de estado -------------------------------------------------
    const std::string& nome() const { return nome_; }
    /// Cópia do último estado conhecido (segura para usar em qualquer thread).
    EstadoVeiculo estado() const;
    /// Modo de voo atual, p.ex. "STABILIZE", "GUIDED", "RTL".
    std::string modo_de_voo() const;

    // --- observer ----------------------------------------------------------
    void registrar_observador(Observador observador);

    // --- conexão e telemetria ----------------------------------------------
    /// Estabelece a conexão; só retorna quando o veículo responder.
    /// Lança ErroDeVeiculo em caso de falha.
    virtual void conectar() = 0;
    /// Assina os fluxos de telemetria (não bloqueia: os dados chegam por callback).
    virtual void iniciar_telemetria() = 0;

    // --- modos de voo ------------------------------------------------------
    /// Modos que esta implementação aceita, em maiúsculas.
    virtual std::vector<std::string> modos_suportados() const = 0;
    /// Pede ao autopiloto a troca do modo de voo. Lança ErroDeModo se o modo
    /// não estiver em modos_suportados().
    virtual void alterar_modo_de_voo(const std::string& modo) = 0;

    // --- comandos de voo ---------------------------------------------------
    virtual void armar() = 0;
    virtual void desarmar() = 0;
    /// Decola até a altura relativa pedida, em metros.
    virtual void decolar(double altura_m) = 0;
    virtual void pousar() = 0;
    virtual void retornar_para_casa() = 0;
    /// Voa até a posição global dada. A altitude é relativa ao ponto de decolagem, em metros.
    virtual void ir_para(double latitude, double longitude, double altitude_m) = 0;

protected:
    /// Modifica o estado e notifica os observadores.
    ///
    /// Usada pelas subclasses quando a telemetria chega:
    ///
    ///     atualizar([&](EstadoVeiculo& e) { e.modo_de_voo = "GUIDED"; });
    void atualizar(const std::function<void(EstadoVeiculo&)>& modificacao);

    /// Converte o modo para maiúsculas e confere se ele está em
    /// modos_suportados(). Devolve o nome normalizado ou lança ErroDeModo.
    std::string validar_modo(const std::string& modo) const;

private:
    std::string nome_;
    mutable std::mutex mutex_;                // protege estado_ e observadores_
    EstadoVeiculo estado_;
    std::vector<Observador> observadores_;
};

/// Permite `std::cout << veiculo` (usado pelo comando `status`).
std::ostream& operator<<(std::ostream& fluxo, const Veiculo& veiculo);

/// Lista única de modos de voo do projeto. Todas as implementações aceitam
/// exatamente estes, para que o console se comporte igual com o drone real e
/// com o simulado.
const std::vector<std::string>& modos_de_voo_conhecidos();

/// Veículo falso: simula física simples de um multirrotor, sem precisar do SITL.
///
/// Reage aos comandos de voo como um drone real: só arma no solo, decola
/// até a altura pedida, se desloca até os destinos, volta ao home no RTL e
/// pousa e desarma ao tocar o chão. A bateria cai enquanto o veículo voa.
///
/// Use `./mini_gcs --simulado` para desenvolver a interface sem o simulador.
class VeiculoSimulado : public Veiculo {
public:
    explicit VeiculoSimulado(std::string nome = "simulado");
    ~VeiculoSimulado() override;

    void conectar() override;
    void iniciar_telemetria() override;
    std::vector<std::string> modos_suportados() const override;
    void alterar_modo_de_voo(const std::string& modo) override;
    void armar() override;
    void desarmar() override;
    void decolar(double altura_m) override;
    void pousar() override;
    void retornar_para_casa() override;
    void ir_para(double latitude, double longitude, double altitude_m) override;

private:
    // Parâmetros da física simplificada.
    static constexpr double VELOCIDADE_SUBIDA_MS = 2.5;
    static constexpr double VELOCIDADE_DESCIDA_MS = 1.0;
    static constexpr double VELOCIDADE_HORIZONTAL_MS = 5.0;
    static constexpr double ALTURA_RTL_M = 15.0;
    static constexpr double CONSUMO_BATERIA_POR_S = 0.02;  // percentual por segundo em voo
    static constexpr double ALTITUDE_SOLO_AMSL_M = 584.0;
    static constexpr double LATITUDE_BASE = -35.363262;    // mesmo ponto do SITL
    static constexpr double LONGITUDE_BASE = 149.165237;
    static constexpr double METROS_POR_GRAU = 111320.0;

    void laco_de_telemetria();
    /// Avança a simulação em `dt` segundos. Exige sim_mutex_ travado.
    void avancar_locked(double dt);
    /// Monta o estado público a partir da simulação. Exige sim_mutex_ travado.
    EstadoVeiculo montar_estado_locked() const;
    /// Publica um estado já montado para os observadores.
    void publicar(const EstadoVeiculo& estado);
    /// Lógica de pouso e de RTL, chamadas tanto pelos comandos quanto por
    /// alterar_modo_de_voo. Exigem sim_mutex_ travado.
    void pousar_locked();
    void retornar_locked();

    std::atomic<bool> parar_{false};
    std::thread thread_;

    // Estado da simulação, protegido por sim_mutex_.
    mutable std::mutex sim_mutex_;
    bool armado_ = false;
    bool em_voo_ = false;
    bool pousando_ = false;
    bool tem_destino_ = false;
    std::string modo_ = "STABILIZE";
    double altitude_m_ = 0.0;
    double altitude_alvo_m_ = 0.0;
    double latitude_ = LATITUDE_BASE;
    double longitude_ = LONGITUDE_BASE;
    double latitude_alvo_ = LATITUDE_BASE;
    double longitude_alvo_ = LONGITUDE_BASE;
    double latitude_home_ = LATITUDE_BASE;
    double longitude_home_ = LONGITUDE_BASE;
    double bateria_pct_ = 100.0;
    double vel_solo_ms_ = 0.0;
    double vel_subida_ms_ = 0.0;
    double guinada_ = 0.0;
    double tempo_s_ = 0.0;
};

}  // namespace gcs