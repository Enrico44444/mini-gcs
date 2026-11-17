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


## Commit 5: Etapa 5, comandos de voo no console e correções

Branch: `etapa-5-console`

Adicionado:

- Comandos `armar`, `desarmar`, `decolar <altura>`, `pousar`, `rtl` e `ir_para <lat> <lon> <altitude>` em `ComandosPadrao.cpp`, cada um como classe própria, chamando diretamente o `Veiculo`
- Função auxiliar `numero()`, que converte argumentos e lança `ErroDeComando` com o nome do argumento inválido
- Validação de intervalo em `ir_para` (latitude de -90 a 90, longitude de -180 a 180, altitude positiva) e de altura em `decolar`
- `tests/teste_simulado.cpp`: teste automatizado com 12 verificações da sequência armar, decolar, deslocar, RTL e pouso, rodado pelo `VeiculoSimulado`
- `docs/EXECUCAO.md`: comandos que funcionaram para subir o SITL, a GCS e compilar o projeto

Corrigido:

- `Drone::decolar` e `Drone::ir_para` trocam para GUIDED antes de enviar o comando. Sem isso, a decolagem falhava em STABILIZE com `NAV_TAKEOFF: FAILED`
- `radio` deixou de ser convertido de NaN para zero. O modelo guarda NaN para "sem informação", e o `status` mostra `n/d`
- `vel_subida` e os demais campos do `status` são arredondados para uma casa decimal, e `-0.0` é exibido como `0.0`

Decisões de projeto:

- O NaN é mantido de propósito no modelo. Um zero seria interpretado como "sinal de rádio nulo", e isso parece uma falha de enlace quando na verdade o autopiloto não reporta o valor
- A troca para GUIDED acontece dentro de `Drone`, e não no console, para que o comportamento seja o mesmo para quem usa a GCS e para quem usa outra interface

Verificação realizada:

- [x] Teste automatizado: 12 verificações passaram após as mudanças em `Telemetria.cpp`
- [x] Simulado: `decolar 0` e `decolar abc` recusados com mensagem de uso; `ir_para 1 2` recusado com o uso correto; `radio=95.0 %` e `vel_subida` sem `-0.0`
- [x] SITL, pela GCS: `armar`, `decolar 10` e `pousar` aceitos; `modo=GUIDED` após o `decolar` sem troca manual; `radio=n/d`; pouso completo com `Hit ground` e `Disarming motors`

Pendente:

- `decolar` sem estar armado retorna `decolar: Failed` do autopiloto. Uma verificação de armamento em `Drone::decolar` daria uma mensagem específica, como o `VeiculoSimulado` já faz
- No simulado, o modo continua `RTL` após o pouso, em vez de `LAND`
- Validação do SITL com `Floating point exception` resolvida com GCC 15, mas a causa exata com clang 22 e GCC 16 não foi investigada


## Commit 6: Etapa 6, testes automatizados com ctest

Branch: `etapa-6-testes`

Adicionado:

- `tests/teste_telemetria.cpp`: distância de haversine, `Home::distancia_m`, `segundos_entre` e a formatação do `resumo()` (rádio `n/d`, zero negativo como `0.0`)
- `tests/teste_historico.cpp`: histórico vazio, ordem de chegada, tempo relativo à amostra mais antiga, capacidade com descarte das mais antigas, `limpar()` e leitura simultânea ao registro em outra thread
- `tests/teste_paineis.cpp`: `GraficoTempo` (mensagem de espera, título, último valor, marcas nos pontos, valor constante, janela de tempo na escala) e `HorizonteArtificial` (mensagem de espera, número de linhas, modo de voo, efeito da arfagem)
- `CMakeLists.txt`: bloco `ctest` com quatro testes, cada um com a lista de fontes de que precisa. O teste do simulado tem limite de 150 s e rótulo `lento`

Corrigido:

- Nada no código de produção. Os testes foram escritos sobre o comportamento atual, e nenhum falhou por defeito real

Verificação realizada:

- [x] Os quatro testes passam com `ctest` em uma cópia do projeto sem MAVSDK, usando CMake 4.4
- [x] Teste do simulado: 12 verificações, como na Etapa 4
- [x] Testes rápidos rodam em menos de 1 segundo, e o simulado leva cerca de 40 segundos

Decisões de projeto:

- Os testes de `Historico` e dos painéis não usam o MAVSDK, porque recebem `EstadoVeiculo` montado à mão. Isso permite testá-los sem o SITL
- O painel devolve o quadro como texto, por isso os testes comparam a saída em vez de olhar a tela
- A checagem da janela de tempo verifica o rótulo da escala vertical, e não o valor 100 em qualquer lugar, porque o eixo de tempo também mostra o instante final

Pendente:

- `numero(valores.back(), 0)` em `GraficoTempo` mostra o último valor com uma casa decimal (`10.0 m`), porque o segundo argumento é a largura, e não as casas decimais. O teste documenta esse comportamento. Se a intenção era outra, a correção é pequena

## Commit 7: Etapa 7, visualização

Branch: `etapa-7-visualizacao`

Adicionado:

- Campos `rumo`, `satelites`, `subida`, `tensao` e `velocidade` no catálogo do `plotar`
- Gráfico com linha: amostras vizinhas são ligadas com `.`, e cada amostra continua marcada com `*`
- Horizonte com escada de arfagem (traços a cada 10 graus, com números) que acompanham a inclinação da rolagem
- Arco de rolagem no topo, com marcas e um ponteiro na rolagem atual
- Fita de rumo na base, com traço a cada 30 graus e as letras N, L, S e O
- Alertas em vermelho na caixa de leituras: rolagem acima de 45 graus e bateria abaixo de 20 %

Decisões de projeto:

- Bateria igual a zero significa "sem leitura", e não bateria descarregada. Por isso o alerta de bateria só dispara com um valor positivo
- Os traços da escada são desenhados onde a altura relativa vale o degrau, e não em linhas fixas. Assim eles acompanham a inclinação sem cálculo extra

Verificação realizada:

- [x] Testes automatizados de painéis: 12 verificações, incluindo linha ligando amostras, escada, arco, fita de rumo e alertas
- [x] `ctest` com os quatro testes passando

Pendente:

- Fita lateral de altitude e de velocidade, como sugerido no `TODO` original do horizonte
- Marcar o instante da decolagem no gráfico


## Commit 8: Etapa 8, exportação de dados

Branch: `etapa-8-exportacao`

Adicionado:

- `Historico::escrever_csv` e `Historico::exportar_csv`: uma linha de cabeçalho e uma linha por amostra, com tempo, modo, flags, posição, atitude, bateria, velocidades, satélites, rádio, home e distância ao home
- Comando `gravar <arquivo.csv>` no console, com mensagem de erro se não houver telemetria
- `tests/teste_exportacao.cpp`: cinco verificações de cabeçalho, número de colunas, escape de vírgulas e aspas, rádio desconhecido como campo vazio, e erro para caminho inválido

Decisões de projeto:

- A escrita copia o histórico sob a trava e grava fora dela, para o disco não atrasar a telemetria
- Rádio desconhecido vira campo vazio, e não zero, pelo mesmo motivo que o `status` mostra `n/d`

Verificação realizada:

- [x] Teste automatizado de exportação: 12 verificações passando
- [x] `ComandosPadrao.cpp` compila sem avisos com `-Wall -Wextra -Wpedantic`
- [x] `gravar` testado na GCS com o simulado
- [x] `gravar` testado na GCS com o SITL

Pendente:

- O histórico guarda só as últimas 2000 amostras. Um voo longo é exportado só a partir da janela final
- O comando sobrescreve o arquivo se ele já existir, sem perguntar