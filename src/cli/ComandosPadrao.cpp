#include "cli/ComandosPadrao.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "EstacaoDeControle.hpp"
#include "modelo/Veiculo.hpp"
#include "visual/GraficoTempo.hpp"
#include "visual/HorizonteArtificial.hpp"

namespace gcs {
namespace {

/// Converte um argumento em número, ou lança ErroDeComando com o nome do argumento.
/// O throw fica fora do try para não ser confundido com a exceção de std::stod.
double numero(const std::string& texto, const std::string& argumento) {
    double valor = 0.0;
    std::size_t usados = 0;
    bool convertido = true;
    try {
        valor = std::stod(texto, &usados);
    } catch (const std::exception&) {
        convertido = false;
    }
    if (!convertido || usados != texto.size()) {
        throw ErroDeComando(argumento + " deve ser um número, recebi '" + texto + "'");
    }
    return valor;
}

class Ajuda : public Comando {
public:
    std::string nome() const override { return "ajuda"; }
    std::string ajuda() const override { return "lista os comandos disponíveis"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        std::cout << "Comandos:\n";
        for (const Comando* comando : estacao.comandos().todos()) {
            std::cout << "  " << comando->linha_de_ajuda() << '\n';
        }
    }
};

class Status : public Comando {
public:
    std::string nome() const override { return "status"; }
    std::string ajuda() const override { return "mostra o modo de voo e a telemetria atual"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        std::cout << estacao.veiculo() << '\n';
    }
};

class Modo : public Comando {
public:
    std::string nome() const override { return "modo"; }
    std::string uso() const override { return "modo <NOME>"; }
    std::string ajuda() const override { return "troca o modo de voo (ex.: modo RTL)"; }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        estacao.veiculo().alterar_modo_de_voo(argumentos.front());
        std::cout << "pedido de troca de modo enviado\n";
    }
};

class Plotar : public Comando {
public:
    std::string nome() const override { return "plotar"; }
    std::string uso() const override { return "plotar <campo>"; }
    std::string ajuda() const override {
        return "abre o gráfico de um campo: " + nomes_dos_campos();
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        const auto& catalogo = campos_disponiveis();
        const auto encontrado = catalogo.find(argumentos.front());
        if (encontrado == catalogo.end()) {
            throw ErroDeComando("campo '" + argumentos.front() +
                                "' não existe. Disponíveis: " + nomes_dos_campos());
        }
        const GraficoTempo grafico{encontrado->second};
        estacao.exibir_painel(grafico);
    }
};

class Horizonte : public Comando {
public:
    std::string nome() const override { return "horizonte"; }
    std::string ajuda() const override { return "abre o horizonte artificial"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        const HorizonteArtificial horizonte;
        estacao.exibir_painel(horizonte);
    }
};

class Armar : public Comando {
public:
    std::string nome() const override { return "armar"; }
    std::string ajuda() const override { return "arma os motores (antes de decolar)"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().armar();
        std::cout << "veículo armado\n";
    }
};

class Desarmar : public Comando {
public:
    std::string nome() const override { return "desarmar"; }
    std::string ajuda() const override { return "desarma os motores (só no solo)"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().desarmar();
        std::cout << "veículo desarmado\n";
    }
};

class Decolar : public Comando {
public:
    std::string nome() const override { return "decolar"; }
    std::string uso() const override { return "decolar <altura_m>"; }
    std::string ajuda() const override {
        return "decola até a altura pedida, em metros (é preciso estar armado)";
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        const double altura = numero(argumentos.front(), "altura");
        if (altura <= 0.0) {
            throw ErroDeComando("a altura deve ser maior que zero");
        }
        estacao.veiculo().decolar(altura);
        std::cout << "decolagem pedida até " << altura << " m\n";
    }
};

class Pousar : public Comando {
public:
    std::string nome() const override { return "pousar"; }
    std::string ajuda() const override { return "pousa no ponto atual"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().pousar();
        std::cout << "pouso pedido\n";
    }
};

class Rtl : public Comando {
public:
    std::string nome() const override { return "rtl"; }
    std::string ajuda() const override { return "volta ao ponto de decolagem e pousa"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().retornar_para_casa();
        std::cout << "retorno para casa pedido\n";
    }
};

class IrPara : public Comando {
public:
    std::string nome() const override { return "ir_para"; }
    std::string uso() const override { return "ir_para <latitude> <longitude> <altitude_m>"; }
    std::string ajuda() const override {
        return "voa até a posição dada, com a altitude em metros acima da decolagem";
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 3) {
            throw ErroDeComando("uso: " + uso());
        }
        const double latitude = numero(argumentos[0], "latitude");
        const double longitude = numero(argumentos[1], "longitude");
        const double altitude = numero(argumentos[2], "altitude");

        if (latitude < -90.0 || latitude > 90.0) {
            throw ErroDeComando("latitude fora do intervalo -90 a 90");
        }
        if (longitude < -180.0 || longitude > 180.0) {
            throw ErroDeComando("longitude fora do intervalo -180 a 180");
        }
        if (altitude <= 0.0) {
            throw ErroDeComando("a altitude deve ser maior que zero");
        }
        estacao.veiculo().ir_para(latitude, longitude, altitude);
        std::cout << "destino enviado\n";
    }
};

class Gravar : public Comando {
public:
    std::string nome() const override { return "gravar"; }
    std::string uso() const override { return "gravar <arquivo.csv>"; }
    std::string ajuda() const override {
        return "grava a telemetria recebida em um arquivo CSV (ex.: gravar voo1.csv)";
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        if (estacao.historico().vazio()) {
            throw ErroDeComando("ainda não há telemetria para gravar");
        }
        const std::size_t amostras = estacao.historico().exportar_csv(argumentos.front());
        std::cout << amostras << " amostras gravadas em " << argumentos.front() << '\n';
    }
};

class Sair : public Comando {
public:
    std::string nome() const override { return "sair"; }
    std::string ajuda() const override { return "encerra a estação de controle"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.encerrar();
    }
};

}  // namespace

RegistroDeComandos comandos_padrao() {
    RegistroDeComandos registro;
    registro.registrar(std::make_unique<Ajuda>());
    registro.registrar(std::make_unique<Status>());
    registro.registrar(std::make_unique<Modo>());
    registro.registrar(std::make_unique<Plotar>());
    registro.registrar(std::make_unique<Horizonte>());
    registro.registrar(std::make_unique<Armar>());
    registro.registrar(std::make_unique<Desarmar>());
    registro.registrar(std::make_unique<Decolar>());
    registro.registrar(std::make_unique<Pousar>());
    registro.registrar(std::make_unique<Rtl>());
    registro.registrar(std::make_unique<IrPara>());
    registro.registrar(std::make_unique<Gravar>());
    registro.registrar(std::make_unique<Sair>());
    return registro;
}

}  // namespace gcs