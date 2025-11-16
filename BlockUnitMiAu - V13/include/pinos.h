#ifndef PINAGEM_H
#define PINAGEM_H

// ========================================
// AU ESP32-S3-N16R8 
// ========================================

// Pinos do display OLED (I2C)
#define SLAVE_OLED_SDA       21  // Data do OLED
#define SLAVE_OLED_SCL       22  // Clock do OLED

// Pinos do DAC UDA1334A (I2S)
#define UDA1334A_BCLK        7    // Bit Clock do DAC
#define UDA1334A_WSEL        10   // Word Select do DAC  
#define UDA1334A_DIN         11   // Data In do DAC



// Pinos dos potenciômetros (controle dos geradores)
#define POT3   13  // Potenciômetro 3 (Gerador 3)
#define POT2   17  // Potenciômetro 2 (Gerador 2) 
#define POT1   16  // Potenciômetro 1 (Gerador 1)

// Pinos do encoder master
#define MASTER_ENC1_CLK       47  // Clock do encoder master
#define MASTER_ENC1_DT        5   // Data do encoder master
#define MASTER_ENC1_BT        4   // Botão do encoder master

// Pinos do cartão SD (SPI)
#define SD_MOSI   8   // Master Out Slave In
#define SD_MISO   9   // Master In Slave Out
#define SD_SCK    18  // Serial Clock
#define SD_CS     21  // Chip Select

// Pino MIDI IN
#define AU_MIDI_IN    12  

// UART TX_AU
#define UART_TX_AU    15

// ========================================
// MI ESP32-WROOM-32D-N4 
// ========================================

// Pinos MIDI DIN
#define SLAVE_MIDI_IN        35  // Entrada MIDI
#define SLAVE_MIDI_OUT       33  // Saída MIDI

// Pinos do multiplexador CD74HC4067 (botões)
#define SLAVE_MUX_CD74HC4067_S0      19  // Seletor 0 (Q0-Q7: botões passos 1-8)
#define SLAVE_MUX_CD74HC4067_S1      16  // Seletor 1 (Q8: modo master/slave)
#define SLAVE_MUX_CD74HC4067_S2      23  // Seletor 2 (Q9: botão encoder)
#define SLAVE_MUX_CD74HC4067_S3      25  // Seletor 3 (Q10-Q12: botões extras)
#define SLAVE_MUX_CD74HC4067_SIG     26  // Sinal de entrada (Q13: botão extra 5)

//00 - botão passo 1
//01 - botão passo 2
//02 - botão passo 3
//03 - botão passo 4
//04 - botão passo 5
//05 - botão passo 6
//06 - botão passo 7
//07 - botão passo 8
//08 - botão extra 1 - Adiciona a página 2, passos 9 a 16
//09 - botão encoder
//10 - botão extra 2 - Alterna entre MODO MASTER e MODO SLAVE
//11 - botão extra 3 - Modo Presets Slots
//12 - botão extra 4 - Modo Input 
//13 - botão extra 5
//14 - botão extra 6 - Click no MODO MASTER > TAP; Long Press no MODO MASTER REC ; Click no Modo Slave REC
//15 - botão extra 7 - Alterna a vizualização do AU

// Pinos do shift register 74HC595 (LEDs)
#define SLAVE_MUX_74HC595_LDSCK      14  // Load/Shift Clock
#define SLAVE_MUX_74HC595_LDSTR      13  // Load/Store
#define SLAVE_MUX_74HC595_DATA       18  // Data
//0_0 - LED 1 - passo 1
//0_1 - LED 2 - passo 2
//0_2 - LED 3 - passo 3
//0_3 - LED 4 - passo 4
//0_4 - LED 5 - passo 5
//0_5 - LED 6 - passo 6
//0_6 - LED 7 - passo 7
//0_7 - LED 8 - passo 8
//1_0 - LED led extra 1
//1_1 - LED led extra 2 
//1_2 - LED led extra 3 
//1_3 - LED led extra 4 
//1_4 - LED led extra 5 
//1_5 - LED led extra 6  
//1_6 - LED led extra 7 

// Pinos do encoder slave (KY-040)
#define SLAVE_ENC2_CLK       27  // Clock do encoder slave
#define SLAVE_ENC2_DT        32  // Data do encoder slave
#define SLAVE_ENC2_BT_CANAL  9   // Botão do encoder slave (canal)

// Pinos MIDI duplicados (para compatibilidade)
#define SLAVE_MIDI_IN     35  // Entrada MIDI
#define SLAVE_MIDI_OUT    33  // Saída MIDI

// UART RX_MI
#define UART_RX_MI    17

#endif