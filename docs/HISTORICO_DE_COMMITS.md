# Histórico de commits: mini GCS

## Commit 1: Etapa 1, amplia o modelo de dados

Branch: `etapa-1-modelo`

Adicionado:

- `Velocidade` (velocidade no solo e taxa de subida), `Enlace` (satélites e sinal de rádio) e `Home` (ponto de decolagem), em `src/modelo/Telemetria.hpp`
- Campos `velocidade`, `enlace` e `home` em `EstadoVeiculo`
- `Home::distancia_m()`, que calcula a distância até o ponto de decolagem
- `distancia_entre_m()`, que calcula a distância entre duas coordenadas GPS (fórmula de haversine)
- `resumo()` mostra os novos campos no comando `status`
- Este arquivo de histórico

## Commit 2: Etapa 2, contrato do veículo

Branch: `etapa-2-contrato`

Adicionado:

- Operações `armar`, `desarmar`, `decolar`, `pousar`, `retornar_para_casa` e `ir_para` como virtuais puras em `Veiculo`
- `modos_suportados()` para listar os modos aceitos por cada implementação
- `ErroDeModo`, exceção própria para modo inválido, separada de `NaoImplementado`
- `validar_modo()` na classe base, usada por `Drone` e `VeiculoSimulado`
- Stubs em `Drone` e `VeiculoSimulado` que lançam `NaoImplementado` até as Etapas 3 e 4
- Correção de duplicação de declarações em `Drone.hpp` e remoção de `para_maiusculas`, que deixou de ser usada

Pendente:

- `Drone` ainda aceita só RTL, HOLD e LAND, enquanto o simulado aceita 7 modos. A lista será unificada na Etapa 3.

## Commit 3: Etapa 3, implementação real com MAVSDK

Branch: `etapa-3-drone`

Adicionado:

- `armar`, `desarmar`, `decolar`, `pousar`, `retornar_para_casa` e `ir_para` implementados em `Drone`, usando `mavsdk::Action`
- Modos de voo por `MAV_CMD_DO_SET_MODE` (`MavlinkPassthrough`) para STABILIZE, ALT_HOLD, LOITER, GUIDED e AUTO. RTL e LAND usam ações do MAVSDK
- Lista única de modos em `modos_de_voo_conhecidos()`, usada por `Drone` e `VeiculoSimulado`. Removido HOLD; incluídos ALT_HOLD e LOITER
- Telemetria estendida: velocidade no solo e de subida, número de satélites, sinal de rádio e home
- `nome_do_modo()` traduz os modos do MAVSDK para os nomes do projeto
- Assinaturas do MAVSDK canceladas no destrutor de `Drone`
- `ir_para` converte a altitude relativa ao ponto de decolagem para a altitude absoluta que o MAVSDK espera
- `CMakeLists.txt` recebeu a macro `MAVSDK_ENABLE_MAVLINK_C_API`, necessária para compilar com `MavlinkPassthrough`

Pendente:

- Validação com o SITL.
- Comandos `armar`, `desarmar`, `pousar` e `ir_para` ainda não estão no console (Etapa 5)
- `VeiculoSimulado` ainda não reage aos comandos de voo (Etapa 4)

## Commit 4: Etapa 4, VeiculoSimulado reage aos comandos de voo

Branch: `etapa-4-simulado`

Adicionado:

- Estado interno da simulação em `VeiculoSimulado`, protegido por `sim_mutex_`: armado, em voo, pousando, destino, altitude alvo, posição, home, bateria e velocidades
- `armar` e `desarmar` com recusa em voo
- `decolar(altura)`: exige armado, registra o ponto atual como home e sobe em velocidade constante até a altura pedida
- `pousar`: desce em velocidade constante e desarma ao tocar o solo
- `retornar_para_casa`: sobe até 15 m se estiver mais baixo, volta ao home em linha reta e pousa ao chegar
- `ir_para(lat, lon, altitude)`: desloca em linha reta até o destino, com a guinada apontando para o alvo
- `alterar_modo_de_voo`: RTL e LAND acionam os mesmos comportamentos dos comandos correspondentes, e os demais modos só trocam o nome
- Velocidade no solo e de subida calculadas a partir do deslocamento real, e distância ao home pela função da Etapa 1
- Bateria cai só com o veículo em voo, a uma taxa constante
- Passo de simulação limitado a 0,2 s, para evitar saltos se a thread atrasar

Decisões de projeto:

- Versão realista da simulação, com física simplificada e velocidades constantes, em vez de altitude linear. Assim a telemetria de velocidade e de distância ao home é exercitada.
- Lógica de voo concentrada em `avancar_locked`, chamada pela thread de telemetria, e os comandos apenas alteram o estado de alvo. Os comandos não mexem diretamente na posição.
- Erros de sequência (decolar sem armar, pousar no solo) usam `ErroDeVeiculo`, com a mesma exceção do `Drone`, para que o console trate os dois casos igual.
- Sem ruído nos sensores nem atraso de comando, para que os testes sejam determinísticos.

Verificação realizada:

- [x] Compila com `-Wall -Wextra` (o aviso de `send_command_long` depreciado é da Etapa 3)
- [x] Teste da sequência armar → decolar 10 → ir_para ~50 m → RTL → pouso, com 12 verificações. Todas passaram
- [x] Recusas: decolar sem armar, pousar no solo e modo HOLD
- [x] Distância ao home de ~49 m no destino, e pouso a menos de 2 m do home
- [x] Bateria consumida durante o voo (99,2 % ao pousar)

Pendente:

- O teste está em `/tmp/teste_simulado.cpp`, fora do projeto. Na Etapa 6, ele deve ir para uma pasta `tests/`, integrado ao CMake com `ctest`
- No simulado, após o pouso o modo continua `RTL`. O ArduPilot real trocaria para `LAND` ou desarmaria. Vale decidir se o simulado deve refletir isso
- `decolar` no console ainda mostra "ainda não implementado" (Etapa 5)
- Validação do `Drone` com o SITL continua pendente, como registrado na Etapa 3