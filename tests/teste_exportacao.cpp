#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
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

gcs::EstadoVeiculo amostra(double altitude, double segundos,
                           const std::string& modo = "STABILIZE") {
    gcs::EstadoVeiculo estado;
    estado.posicao.altitude_relativa_m = altitude;
    estado.modo_de_voo = modo;
    estado.instante = BASE + std::chrono::milliseconds(static_cast<long long>(segundos * 1000.0));
    return estado;
}

std::vector<std::string> linhas_de(const std::string& texto) {
    std::vector<std::string> linhas;
    std::istringstream fluxo(texto);
    std::string linha;
    while (std::getline(fluxo, linha)) {
        linhas.push_back(linha);
    }
    return linhas;
}

/// Separa por vírgula. Só use em linhas cujos campos não contêm vírgula.
std::vector<std::string> campos_de(const std::string& linha) {
    std::vector<std::string> campos;
    std::string atual;
    for (char caractere : linha) {
        if (caractere == ',') {
            campos.push_back(atual);
            atual.clear();
        } else {
            atual += caractere;
        }
    }
    campos.push_back(atual);
    return campos;
}

}  // namespace

int main() {
    const std::string cabecalho =
        "tempo_s,modo_de_voo,armado,em_voo,latitude,longitude,altitude_abs_m,altitude_rel_m,"
        "rolagem_deg,arfagem_deg,guinada_deg,bateria_pct,tensao_v,vel_solo_ms,vel_subida_ms,"
        "satelites,radio_pct,home_lat,home_lon,home_definido,dist_home_m";
    const std::size_t colunas = campos_de(cabecalho).size();

    // Histórico vazio: só o cabeçalho.
    {
        gcs::Historico historico;
        std::ostringstream saida;
        const std::size_t escritas = historico.escrever_csv(saida);
        verificar(escritas == 0, "histórico vazio não escreve amostras");
        verificar(linhas_de(saida.str()).size() == 1 && linhas_de(saida.str())[0] == cabecalho,
                  "histórico vazio escreve só o cabeçalho");
    }

    // Três amostras: uma linha de cabeçalho e três de dados, todas com o mesmo número de colunas.
    {
        gcs::Historico historico;
        historico.registrar(amostra(0.0, 0.0));
        historico.registrar(amostra(5.0, 1.0));
        historico.registrar(amostra(10.0, 2.0));
        std::ostringstream saida;
        const std::size_t escritas = historico.escrever_csv(saida);
        const std::vector<std::string> linhas = linhas_de(saida.str());
        verificar(escritas == 3 && linhas.size() == 4,
                  "três amostras geram quatro linhas (cabeçalho e dados)");
        bool mesmas_colunas = true;
        for (const std::string& linha : linhas) {
            mesmas_colunas = mesmas_colunas && campos_de(linha).size() == colunas;
        }
        verificar(mesmas_colunas, "todas as linhas têm o mesmo número de colunas do cabeçalho");
        verificar(linhas[0] == cabecalho, "cabeçalho tem os nomes esperados");
        verificar(campos_de(linhas[1])[0] == "0.000000" && campos_de(linhas[2])[0] == "1.000000",
                  "tempo_s começa em zero e cresce a partir da amostra mais antiga");
        verificar(campos_de(linhas[1])[16] == "",
                  "rádio desconhecido (NaN) fica como campo vazio");
    }

    // Modo com vírgula e aspas é escrito entre aspas, sem quebrar as colunas.
    {
        gcs::Historico historico;
        historico.registrar(amostra(0.0, 0.0, "LOITER, \"teste\""));
        std::ostringstream saida;
        historico.escrever_csv(saida);
        verificar(saida.str().find("\"LOITER, \"\"teste\"\"\"") != std::string::npos,
                  "modo com vírgula e aspas é escapado no CSV");
    }

    // Flags e rádio conhecido.
    {
        gcs::Historico historico;
        gcs::EstadoVeiculo estado = amostra(3.0, 0.0);
        estado.armado = true;
        estado.em_voo = false;
        estado.enlace.sinal_radio_percent = 95.0;
        historico.registrar(estado);
        std::ostringstream saida;
        historico.escrever_csv(saida);
        const std::vector<std::string> campos = campos_de(linhas_de(saida.str())[1]);
        verificar(campos[2] == "1" && campos[3] == "0",
                  "armado e em voo são gravados como 1 e 0");
        verificar(campos[16] == "95.000000", "rádio conhecido é gravado com o valor");
    }

    // Exportação para arquivo, lendo o resultado de volta.
    {
        gcs::Historico historico;
        historico.registrar(amostra(1.0, 0.0));
        historico.registrar(amostra(2.0, 0.5));
        const std::string caminho = "teste_exportacao.csv";
        const std::size_t escritas = historico.exportar_csv(caminho);
        std::ifstream entrada(caminho);
        std::string primeira;
        std::getline(entrada, primeira);
        entrada.close();
        std::remove(caminho.c_str());
        verificar(escritas == 2 && primeira == cabecalho,
                  "exportar_csv grava o arquivo com o cabeçalho e as amostras");
    }

    // Caminho inválido gera erro claro, e não falha silenciosa.
    {
        gcs::Historico historico;
        historico.registrar(amostra(1.0, 0.0));
        bool lancou = false;
        try {
            historico.exportar_csv("/diretorio_que_nao_existe/x.csv");
        } catch (const std::runtime_error&) {
            lancou = true;
        }
        verificar(lancou, "caminho que não pode ser criado lança std::runtime_error");
    }

    std::cout << (falhas == 0 ? "\nTodos os testes de exportação passaram.\n"
                              : "\nHá testes de exportação com falha.\n");
    return falhas == 0 ? 0 : 1;
}