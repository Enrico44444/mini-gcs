#include "visual/HorizonteArtificial.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "modelo/Telemetria.hpp"
#include "visual/Terminal.hpp"

namespace gcs {
namespace {

/// Quantos graus cada caractere vale. Como um caractere de terminal é mais ou
/// menos duas vezes mais alto do que largo, uma coluna vale metade de uma linha
/// — é isso que mantém o horizonte com a inclinação correta.
constexpr double GRAUS_POR_LINHA = 4.0;
constexpr double GRAUS_POR_COLUNA = 2.0;

constexpr double PI = 3.14159265358979323846;

/// Limites que disparam alerta no painel.
constexpr double ROLAGEM_ALERTA_GRAUS = 45.0;
constexpr double BATERIA_ALERTA_PCT = 20.0;

/// Raio do arco de rolagem, em linhas de texto.
constexpr double RAIO_ARCO_LINHAS = 10.0;

/// O que há atrás de cada caractere.
enum class Fundo { Ceu, Chao, Horizonte, Caixa };
/// A cor do caractere desenhado em cima do fundo.
enum class Tinta { Padrao, Simbolo, Texto, Escala, Alerta };

struct Celula {
    Fundo fundo = Fundo::Ceu;
    Tinta tinta = Tinta::Padrao;
    char glifo = ' ';
};

const std::string FUNDO_CEU = terminal::fundo(0x2A, 0x78, 0xD6);
const std::string FUNDO_CHAO = terminal::fundo(0x8C, 0x5A, 0x2B);
const std::string FUNDO_HORIZONTE = terminal::fundo(0xFC, 0xFC, 0xFB);
const std::string FUNDO_CAIXA = terminal::fundo(0x1A, 0x1A, 0x19);
const std::string FRENTE_PADRAO = terminal::frente(0xFC, 0xFC, 0xFB);
const std::string FRENTE_SIMBOLO = terminal::frente(0xED, 0xA1, 0x00);
const std::string FRENTE_TEXTO = terminal::frente(0xFC, 0xFC, 0xFB);
const std::string FRENTE_ESCALA = terminal::frente(0x1A, 0x1A, 0x19);
const std::string FRENTE_ALERTA = terminal::frente(0xFF, 0x4D, 0x4D);

const std::string& codigo(Fundo fundo) {
    switch (fundo) {
        case Fundo::Chao: return FUNDO_CHAO;
        case Fundo::Horizonte: return FUNDO_HORIZONTE;
        case Fundo::Caixa: return FUNDO_CAIXA;
        case Fundo::Ceu: break;
    }
    return FUNDO_CEU;
}

const std::string& codigo(Tinta tinta) {
    switch (tinta) {
        case Tinta::Simbolo: return FRENTE_SIMBOLO;
        case Tinta::Texto: return FRENTE_TEXTO;
        case Tinta::Escala: return FRENTE_ESCALA;
        case Tinta::Alerta: return FRENTE_ALERTA;
        case Tinta::Padrao: break;
    }
    return FRENTE_PADRAO;
}

std::string graus(double valor) {
    std::ostringstream fluxo;
    fluxo << std::fixed << std::setprecision(1) << std::showpos << std::setw(7) << valor;
    return fluxo.str();
}

/// Rumo em graus no intervalo [0, 360).
double normalizar_rumo(double graus_valor) {
    double resto = std::fmod(graus_valor, 360.0);
    if (resto < 0.0) {
        resto += 360.0;
    }
    return resto;
}

}  // namespace

HorizonteArtificial::HorizonteArtificial(int largura, int altura)
    : Painel("Horizonte artificial"),
      largura_(std::max(40, largura)),
      altura_(std::max(11, altura | 1)) {}  // altura ímpar: existe uma linha central

std::string HorizonteArtificial::quadro(const Historico& historico) const {
    bool recebeu = false;
    const EstadoVeiculo estado = historico.ultimo(&recebeu);
    std::ostringstream saida;
    if (!recebeu) {
        saida << titulo() << terminal::LIMPAR_LINHA << "\n\n"
              << "aguardando telemetria..." << terminal::LIMPAR_LINHA << "\n";
        return saida.str();
    }

    const double rolagem = estado.atitude.rolagem;
    const double arfagem = estado.atitude.arfagem;
    const double guinada = estado.atitude.guinada;
    const double seno = std::sin(rolagem * PI / 180.0);
    const double cosseno = std::cos(rolagem * PI / 180.0);
    const double centro_linha = (altura_ - 1) / 2.0;
    const double centro_coluna = (largura_ - 1) / 2.0;
    const int linha_central = altura_ / 2;
    const int coluna_central = largura_ / 2;

    std::vector<std::vector<Celula>> tela(
        static_cast<std::size_t>(altura_),
        std::vector<Celula>(static_cast<std::size_t>(largura_)));

    // Escreve um caractere por cima do desenho. `fundo` vazio mantém o céu/chão
    // que já estava ali — é o que faz os traços parecerem parte do horizonte.
    const auto escrever = [&](int linha, int coluna, char glifo, Tinta tinta,
                              std::optional<Fundo> fundo = std::nullopt) {
        if (linha < 0 || linha >= altura_ || coluna < 0 || coluna >= largura_) {
            return;
        }
        Celula& celula = tela[static_cast<std::size_t>(linha)][static_cast<std::size_t>(coluna)];
        celula.glifo = glifo;
        celula.tinta = tinta;
        if (fundo.has_value()) {
            celula.fundo = *fundo;
        }
    };

    // Coordenadas de uma célula: y em graus acima do centro, x em graus à direita.
    const auto y_de = [&](int linha) { return (centro_linha - linha) * GRAUS_POR_LINHA; };
    const auto x_de = [&](int coluna) { return (coluna - centro_coluna) * GRAUS_POR_COLUNA; };
    // Altura, em graus, de um ponto acima da linha do horizonte.
    const auto altura_relativa = [&](double x, double y) {
        return -x * seno + y * cosseno + arfagem;
    };

    // 1. Decide, caractere por caractere, se ali é céu, chão ou horizonte.
    for (int linha = 0; linha < altura_; ++linha) {
        for (int coluna = 0; coluna < largura_; ++coluna) {
            const double u = altura_relativa(x_de(coluna), y_de(linha));
            Celula& celula = tela[static_cast<std::size_t>(linha)][static_cast<std::size_t>(coluna)];
            if (std::abs(u) <= GRAUS_POR_LINHA * 0.6) {
                celula.fundo = Fundo::Horizonte;
            } else {
                celula.fundo = (u > 0.0) ? Fundo::Ceu : Fundo::Chao;
            }
        }
    }

    // 2. Escada de arfagem: traços a cada 10 graus, que acompanham a inclinação
    //    porque são desenhados onde a altura relativa vale o degrau.
    for (int grau = -30; grau <= 30; grau += 10) {
        if (grau == 0) {
            continue;
        }
        for (int linha = 0; linha < altura_; ++linha) {
            for (int coluna = 0; coluna < largura_; ++coluna) {
                const double x = x_de(coluna);
                if (std::abs(x) <= 24.0 &&
                    std::abs(altura_relativa(x, y_de(linha)) - grau) <= GRAUS_POR_LINHA * 0.5) {
                    escrever(linha, coluna, '-', Tinta::Escala);
                }
            }
        }
        // Número ao lado da ponta direita do traço: a linha em que o traço cruza x = 24 graus.
        if (std::abs(cosseno) > 1e-3) {
            const double y = (grau - arfagem + 24.0 * seno) / cosseno;
            const int linha_rotulo =
                static_cast<int>(std::lround(centro_linha - y / GRAUS_POR_LINHA));
            const std::string rotulo = std::to_string(grau);
            for (std::size_t j = 0; j < rotulo.size(); ++j) {
                escrever(linha_rotulo, coluna_central + 14 + static_cast<int>(j), rotulo[j],
                         Tinta::Escala);
            }
        }
    }

    // 3. Arco de rolagem no topo, com marcas e um ponteiro na rolagem atual.
    const auto posicao_no_arco = [&](double angulo_graus, double raio) {
        const double radianos = angulo_graus * PI / 180.0;
        const int linha = linha_central - static_cast<int>(std::lround(raio * std::cos(radianos)));
        // Uma coluna vale metade de uma linha: por isso o deslocamento horizontal é dobrado.
        const int coluna =
            coluna_central + static_cast<int>(std::lround(2.0 * raio * std::sin(radianos)));
        return std::pair<int, int>(linha, coluna);
    };
    for (const double marca : {-60.0, -45.0, -30.0, -20.0, -10.0, 0.0, 10.0, 20.0, 30.0, 45.0, 60.0}) {
        const auto [linha, coluna] = posicao_no_arco(marca, RAIO_ARCO_LINHAS);
        escrever(linha, coluna, '|', Tinta::Escala);
    }
    const auto [linha_ponteiro, coluna_ponteiro] =
        posicao_no_arco(std::clamp(rolagem, -60.0, 60.0), RAIO_ARCO_LINHAS - 1.0);
    escrever(linha_ponteiro, coluna_ponteiro, 'v', Tinta::Simbolo);

    // 4. Fita de rumo na última linha: um traço a cada 30 graus e a letra nos
    //    pontos cardeais (N, L, S, O). A coluna central mostra o rumo atual.
    const char cardeais[] = {'N', 'L', 'S', 'O'};
    const int linha_fita = altura_ - 1;
    for (int coluna = 0; coluna < largura_; ++coluna) {
        const double rumo = normalizar_rumo(guinada + x_de(coluna));
        const double desvio_30 = std::abs(rumo - 30.0 * std::round(rumo / 30.0));
        if (desvio_30 > 1.0) {
            continue;
        }
        const double quarto = std::round(rumo / 90.0);
        const double desvio_90 = std::abs(rumo - 90.0 * quarto);
        if (desvio_90 <= 1.0) {
            escrever(linha_fita, coluna, cardeais[static_cast<int>(quarto) % 4], Tinta::Texto);
        } else {
            escrever(linha_fita, coluna, '|', Tinta::Escala);
        }
    }

    // 5. O símbolo da aeronave, sempre fixo no centro: é a referência do piloto.
    for (int distancia = 5; distancia <= 11; ++distancia) {
        escrever(linha_central, coluna_central - distancia, '=', Tinta::Simbolo);
        escrever(linha_central, coluna_central + distancia, '=', Tinta::Simbolo);
    }
    escrever(linha_central, coluna_central, 'o', Tinta::Simbolo);

    // 6. As leituras numéricas e os alertas, em uma caixa escura para ficarem
    //    legíveis tanto sobre o céu quanto sobre o chão.
    std::vector<std::string> leituras{
        " modo    " + estado.modo_de_voo,
        " rolagem " + graus(rolagem) + " deg",
        " arfagem " + graus(arfagem) + " deg",
        " rumo    " + graus(guinada) + " deg",
    };
    std::vector<Tinta> tintas(leituras.size(), Tinta::Texto);
    if (std::abs(rolagem) > ROLAGEM_ALERTA_GRAUS) {
        leituras.push_back(" ALERTA: rolagem excessiva ");
        tintas.push_back(Tinta::Alerta);
    }
    // Bateria 0 significa "ainda sem leitura", e não bateria descarregada.
    if (estado.bateria.percentual > 0.0 && estado.bateria.percentual < BATERIA_ALERTA_PCT) {
        leituras.push_back(" ALERTA: bateria baixa ");
        tintas.push_back(Tinta::Alerta);
    }
    std::size_t largura_caixa = 0;
    for (const std::string& leitura : leituras) {
        largura_caixa = std::max(largura_caixa, leitura.size() + 1);
    }
    for (std::size_t i = 0; i < leituras.size(); ++i) {
        leituras[i].resize(largura_caixa, ' ');  // deixa a caixa retangular
        for (std::size_t j = 0; j < leituras[i].size(); ++j) {
            escrever(static_cast<int>(i) + 1, static_cast<int>(j) + 2, leituras[i][j], tintas[i],
                     Fundo::Caixa);
        }
    }

    // 7. Emite a tela, trocando as cores só quando elas mudam de verdade.
    saida << terminal::NEGRITO << titulo() << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    for (const std::vector<Celula>& linha : tela) {
        const Fundo* fundo_atual = nullptr;
        const Tinta* tinta_atual = nullptr;
        for (const Celula& celula : linha) {
            if (fundo_atual == nullptr || celula.fundo != *fundo_atual) {
                saida << codigo(celula.fundo);
                fundo_atual = &celula.fundo;
            }
            if (tinta_atual == nullptr || celula.tinta != *tinta_atual) {
                saida << codigo(celula.tinta);
                tinta_atual = &celula.tinta;
            }
            saida << celula.glifo;
        }
        saida << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    }
    saida << "\n"
          << terminal::frente(0x82, 0x81, 0x7C) << "[Enter] volta para o console"
          << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    return saida.str();
}

// Ideias ainda não feitas para este painel:
//  * pintar de vermelho a própria linha do horizonte quando a rolagem for crítica;
//  * indicador de bateria e de modo de voo com destaque quando houver falha.

}  // namespace gcs