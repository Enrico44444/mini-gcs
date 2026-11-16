#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "modelo/Historico.hpp"

namespace {

int falhas = 0;

void verificar(bool ok, const std::string& descricao) {
    std::cout << (ok ? "[ok]    " : "[FALHA] ") << descricao << '\n';
    if (!ok) {
        ++falhas;
    }
}

const gcs::Instante BASE = std::chrono::steady_clock::now();

/// Estado com altitude conhecida, recebido `segundos` depois de BASE.
gcs::EstadoVeiculo amostra(double altitude, double segundos) {
    gcs::EstadoVeiculo estado;
    estado.posicao.altitude_relativa_m = altitude;
    estado.instante = BASE + std::chrono::milliseconds(static_cast<long long>(segundos * 1000.0));
    return estado;
}

gcs::Extrator altitude() {
    return [](const gcs::EstadoVeiculo& e) { return e.posicao.altitude_relativa_m; };
}

bool proximos(const std::vector<double>& obtido, const std::vector<double>& esperado) {
    if (obtido.size() != esperado.size()) {
        return false;
    }
    for (std::size_t i = 0; i < obtido.size(); ++i) {
        if (std::fabs(obtido[i] - esperado[i]) > 1e-6) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    // Histórico recém-criado.
    {
        gcs::Historico historico;
        verificar(historico.vazio() && historico.tamanho() == 0,
                  "histórico novo começa vazio");
        bool ok = true;
        historico.ultimo(&ok);
        verificar(!ok, "ultimo() sinaliza que ainda não chegou estado");
        verificar(historico.serie(altitude()).vazia(),
                  "serie() de histórico vazio é vazia");
    }

    // Ordem de chegada e último estado.
    {
        gcs::Historico historico;
        historico.registrar(amostra(1.0, 0.0));
        historico.registrar(amostra(2.0, 1.0));
        historico.registrar(amostra(3.0, 2.0));
        bool ok = false;
        const gcs::EstadoVeiculo ultimo = historico.ultimo(&ok);
        verificar(ok && ultimo.posicao.altitude_relativa_m == 3.0,
                  "ultimo() devolve a amostra mais recente");
        verificar(proximos(historico.serie(altitude()).valores, {1.0, 2.0, 3.0}),
                  "serie() mantém a ordem de chegada");
    }

    // Tempo medido a partir da amostra mais antiga.
    {
        gcs::Historico historico;
        historico.registrar(amostra(0.0, 0.0));
        historico.registrar(amostra(0.0, 0.5));
        historico.registrar(amostra(0.0, 2.0));
        verificar(proximos(historico.serie(altitude()).segundos, {0.0, 0.5, 2.0}),
                  "segundos são medidos a partir da amostra mais antiga");
    }

    // Capacidade: as amostras mais antigas saem primeiro.
    {
        gcs::Historico historico(5);
        for (int i = 0; i < 8; ++i) {
            historico.registrar(amostra(i, i));
        }
        const gcs::Historico::Serie serie = historico.serie(altitude());
        verificar(historico.tamanho() == 5, "capacidade limita o número de amostras");
        verificar(proximos(serie.valores, {3.0, 4.0, 5.0, 6.0, 7.0}),
                  "as amostras mais antigas são descartadas");
        verificar(!serie.segundos.empty() && std::fabs(serie.segundos.front()) < 1e-6,
                  "o tempo começa em zero na amostra mais antiga que restou");
    }

    // limpar() esvazia tudo.
    {
        gcs::Historico historico;
        historico.registrar(amostra(1.0, 0.0));
        historico.limpar();
        verificar(historico.vazio() && historico.serie(altitude()).vazia(),
                  "limpar() esvazia o histórico");
    }

    // Registro e leitura em threads diferentes, como acontece com a telemetria.
    {
        gcs::Historico historico(100);
        std::thread produtor([&historico] {
            for (int i = 0; i < 5000; ++i) {
                historico.registrar(amostra(i, i * 0.001));
            }
        });
        bool consistente = true;
        for (int i = 0; i < 1000; ++i) {
            const gcs::Historico::Serie serie = historico.serie(altitude());
            if (serie.segundos.size() != serie.valores.size() || serie.valores.size() > 100) {
                consistente = false;
            }
        }
        produtor.join();
        verificar(consistente, "leitura durante o registro mantém a série consistente");
        verificar(historico.tamanho() == 100,
                  "após o registro, o histórico fica com exatamente a capacidade");
    }

    std::cout << (falhas == 0 ? "\nTodos os testes do histórico passaram.\n"
                              : "\nHá testes do histórico com falha.\n");
    return falhas == 0 ? 0 : 1;
}