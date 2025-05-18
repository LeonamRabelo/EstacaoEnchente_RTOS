# 🌊 Estação de Alerta de Enchente - BitDogLab com RP2040 + FreeRTOS

Este projeto implementa uma estação de monitoramento de cheias que simula os níveis de água e chuva com o joystick da placa BitDogLab. O sistema é baseado no RP2040 rodando FreeRTOS, com comunicação entre tarefas por meio de filas. Possui alertas visuais, sonoros e exibição em tempo real no display OLED.

---

## 🎯 Objetivo

Desenvolver uma estação de monitoramento embarcada com alertas automáticos de enchente, utilizando sensores simulados via joystick, com ênfase em acessibilidade e resposta visual/sonora.

---

## ⚙️ Funcionalidades

- **Simulação de sensores**: 
  - Eixo X → Nível do rio (%)
  - Eixo Y → Volume de chuva (%)
- **Modo Normal**: 
  - Monitoramento contínuo dos dados
  - Feedback visual com LED verde e matriz de LED animada mostrando o nivel do volume de chuva
- **Modo Alerta**:
  - Ativado automaticamente se: 
    - Nível da água ≥ 70% ou Chuva ≥ 80%
  - Feedback visual: LED vermelho + símbolo de alerta (!) na matriz
  - Feedback sonoro: 3 bipes curtos com pausa
- **Modo Manual**:
  - Pode ser ativado/desativado com o botão A
  - Útil em situações excepcionais
- **Display OLED**:
  - Exibe os níveis lidos e o estado atual (Normal/Alerta)
- **Acessibilidade**:
  - Buzzer como alerta sonoro
  - Exibição clara no display
  - Botão físico de emergência

---

## 🧠 Arquitetura FreeRTOS

O sistema utiliza várias tarefas que se comunicam por **filas**:

| Tarefa               | Responsabilidade                                |
|----------------------|-------------------------------------------------|
| `vTaskLeituraJoystick` | Lê o joystick e envia leituras para a fila     |
| `vTaskDisplay`         | Exibe os dados no display OLED                 |
| `vTaskMatrizLeds`      | Atualiza a matriz de LED conforme nível/alerta |
| `vTaskBuzzer`          | Toca sons de alerta ou monitoração             |
| `vTaskLedRGB`          | Controla os LEDs RGB com base no risco         |
| `vTaskBotao`           | Alterna modo manual via botão A                |

---

## 🖥️ Componentes Utilizados (BitDogLab)

- 🕹️ **Joystick** (ADC0, ADC1): simula sensores
- 📺 **Display OLED SSD1306** (I2C)
- 🔊 **Buzzer** (PWM)
- 🔴🟢🔵 **LED RGB** (GPIO)
- 🔲 **Matriz de LEDs WS2812** (PIO)
- 🔘 **Botão A**: alterna modo manual

---

## 🚨 Animação da Matriz de LEDs

- Níveis de água são animados com preenchimento de baixo para cima
- Em modo alerta, exibe uma exclamação `!` vermelha

---

# 🧑‍💻 Desenvolvido por
Leonam S. Rabelo