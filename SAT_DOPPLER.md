# SAT DOPPLER — Rastreamento de Satélites com Correção Doppler

Módulo de rastreamento de satélites amadores para o Quansheng UV-K5, com correção automática de frequência por efeito Doppler em tempo real.

## Funcionalidades

- Correção Doppler automática para RX e TX durante passagens de satélites
- Ajuste manual de frequência em passos de 1 kHz
- Suporte a FM, USB (SSB) e AM — respeita a modulação configurada no canal
- Monitor (squelch override) com SIDE1
- Transmissão com CTCSS e potência configurados no canal
- Sem necessidade de modificação de hardware ou EEPROM expandida

## Como Configurar os Satélites

Os satélites são canais de memória normais cujo nome começa com **$**. Configure pelo menu do rádio ou pelo CHIRP:

1. Crie um canal de memória
2. Nomeie começando com `$` (ex: `$SO-50`, `$ISS`, `$AO-91`)
3. Configure a **frequência RX** como o **downlink** do satélite
4. Configure o **TX Offset** como a diferença entre uplink e downlink
5. Configure a **direção do offset** (+ ou -)
6. Configure o **CTCSS TX** se necessário (ex: 67.0 Hz para SO-50)
7. Configure a **potência TX** desejada (LOW recomendado para satélites)
8. Configure a **modulação** (FM para satélites FM, USB para lineares)

### Exemplo: SO-50

| Campo | Valor |
|-------|-------|
| Nome | `$SO-50` |
| RX Freq | 436.795 MHz |
| TX Offset | 290.945 MHz |
| Offset Dir | - (negativo: 436.795 - 290.945 = 145.850 uplink) |
| CTCSS TX | 67.0 Hz |
| Potência | LOW |
| Modulação | FM |
| Bandwidth | Wide |

### Exemplo: ISS

| Campo | Valor |
|-------|-------|
| Nome | `$ISS` |
| RX Freq | 437.800 MHz |
| TX Offset | 291.810 MHz |
| Offset Dir | - (437.800 - 291.810 = 145.990 uplink) |
| CTCSS TX | 67.0 Hz |
| Potência | LOW |
| Modulação | FM |

### Exemplo: RS-44 (linear/SSB)

| Campo | Valor |
|-------|-------|
| Nome | `$RS-44` |
| RX Freq | 435.640 MHz |
| TX Offset | 289.675 MHz |
| Offset Dir | - (435.640 - 289.675 = 145.965 uplink) |
| CTCSS TX | nenhum |
| Modulação | USB |

## Como Usar

### Entrar no modo Doppler

Pressione **F + \*** na tela principal.

### Tela Principal

```
┌────────────────────────────┐
│ SAT DOPPLER         [bat]  │
│          SO-50        1/5  │
│                            │
│ RX 436.79500      +10.9K   │
│ TX 145.85000       -3.6K   │
│                            │
│          TRK 285s          │
│ MON                        │
└────────────────────────────┘
```

### Teclas

| Tecla | Função |
|-------|--------|
| **UP / DOWN** | Seleciona satélite (navega entre canais $) |
| **MENU** | Inicia tracking: digite a duração em segundos, depois MENU para confirmar |
| **MENU** (durante tracking) | Para o tracking |
| **PTT** | Transmite na frequência uplink corrigida pelo Doppler |
| **SIDE1** | Liga/desliga monitor (squelch aberto) |
| **SIDE2** | Liga backlight |
| **1** | RX +1 kHz (ajuste manual) |
| **7** | RX -1 kHz (ajuste manual) |
| **3** | TX +1 kHz (ajuste manual) |
| **9** | TX -1 kHz (ajuste manual) |
| **EXIT** | Sai do modo Doppler |

### Fluxo de Operação

1. **F+\*** para entrar no SAT DOPPLER
2. **UP/DOWN** para selecionar o satélite
3. Quando o satélite aparecer no horizonte, pressione **MENU**
4. Digite a duração estimada da passagem em segundos (ex: `480` para 8 minutos)
5. Pressione **MENU** para confirmar e iniciar o tracking automático
6. O rádio ajusta RX e TX automaticamente a cada segundo
7. **PTT** para transmitir (com Doppler corrigido + CTCSS)
8. O tracking para automaticamente ao fim da duração, ou pressione **MENU** para parar

### Ajuste Manual (sem tracking)

Se preferir corrigir o Doppler manualmente:
- Use **1/7** para ajustar RX em passos de 1 kHz
- Use **3/9** para ajustar TX em passos de 1 kHz
- O indicador de shift (ex: `+3.0K`) mostra o desvio atual

## Como Funciona o Cálculo Doppler

O desvio Doppler para satélites LEO segue uma curva em S (sigmoide):

```
shift(t) = max_shift × (t - t_meio) / √((t - t_meio)² + T²)
```

- **max_shift** = frequência × velocidade_orbital / velocidade_da_luz
  - ~3.6 kHz em 145 MHz
  - ~10.9 kHz em 435 MHz
- **T** = constante de tempo (~143 segundos para satélites a 500 km)
- **t_meio** = metade da duração da passagem (TCA)

### Correção RX vs TX

- **RX (downlink)**: satélite se aproxima → frequência recebida sobe → rádio sintoniza acima
- **TX (uplink)**: satélite se aproxima → ele vê nossa freq mais alta → transmitimos abaixo para compensar

Os shifts de RX e TX são diferentes porque as frequências estão em bandas diferentes (VHF vs UHF).

## Compilação

```bash
make clean
make all ENABLE_MESSENGER=0 ENABLE_MESSENGER_UART=0 \
         ENABLE_MESSENGER_NOTIFICATION=0 \
         ENABLE_MESSENGER_DELIVERY_NOTIFICATION=0 \
         ENABLE_FEAT_F4HWN_PMR=0 \
         ENABLE_FEAT_F4HWN_NARROWER=0 \
         ENABLE_FEAT_F4HWN_SLEEP=0
```

Para desabilitar o Doppler: adicione `ENABLE_DOPPLER=0`.

## Requisitos

- Quansheng UV-K5/K6/5R com EEPROM padrão de 8 KB
- Toolchain `arm-none-eabi-gcc`
- Python 3 com `crcmod` (para gerar .packed.bin)
- Nenhuma modificação de hardware necessária

## Créditos

- Conceito de Doppler baseado no trabalho de LOSEHU (uv-k5-firmware-custom)
- Firmware base: Joaquim.org (fork F4HWN/Egzumer)
- Dados de satélites: AMSAT (amsat.org)
