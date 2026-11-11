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
