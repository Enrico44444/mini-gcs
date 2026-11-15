# Execução: mini GCS e ArduPilot

## Pré-requisitos

- MAVSDK 4.0.4 instalado
- ArduPilot na branch `Copter-4.5`, com o venv ativado (`~/ardupilot/venv`)
- GCC 15 (`gcc-15` e `g++-15`) para compilar o SITL
- Ajuste os caminhos abaixo se o projeto estiver em outro lugar

## Iniciar o ArduPilot (SITL)

```bash
cd ~/ardupilot/ArduCopter
source ~/ardupilot/venv/bin/activate
CC=gcc-15 CXX=g++-15 python3 ../Tools/autotest/sim_vehicle.py -v ArduCopter --console --map --out=udpout:127.0.0.1:14552
```

## Iniciar a GCS

```bash
cd ~/Documentos/PUGCS
./build/mini_gcs --endereco udpin://0.0.0.0:14552
```

## Compilar o projeto

```bash
cd ~/Documentos/PUGCS
cmake --build build -j$(nproc)
```