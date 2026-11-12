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