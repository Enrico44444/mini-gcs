// Fábrica dos comandos da estação de controle.
#pragma once

#include "cli/Comando.hpp"

namespace gcs {

/// Monta o registro com todos os comandos do console.

/// Cada comando é uma classe pequena em ComandosPadrao.cpp, registrada aqui.
/// Para acrescentar um comando, crie a classe e adicione uma linha de registro.
RegistroDeComandos comandos_padrao();

}  // namespace gcs