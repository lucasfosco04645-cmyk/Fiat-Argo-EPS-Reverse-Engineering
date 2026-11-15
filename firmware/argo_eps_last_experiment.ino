#include <mcp_can.h>
#include <SPI.h>

#define SPI_CS_PIN 10
#define RX_BUFFER_SIZE 256

#define UDS_TX_ID 0x18DA30F1
#define UDS_RX_ID 0x18DAF130

MCP_CAN CAN(SPI_CS_PIN);

// Timers de C-CAN (Background)
unsigned long prevBCMCmd = 0;
unsigned long prevBSM1 = 0;
unsigned long prevMOT1 = 0;
unsigned long prevMOT3 = 0;
unsigned long prevABS = 0;
unsigned long prevBCM2 = 0;
unsigned long prevIPC = 0;
unsigned long prevSAS = 0;
unsigned long prevClutch = 0;
unsigned long prevHandbrake = 0;
unsigned long prevPROXI = 0;

// Máquina de Estados UDS Sequencial
int udsState = 0; 
unsigned long nextActionAt = 0;
unsigned long bootTime = 0;
bool operationalStateAchieved = false;

// Contadores de Nibble
uint8_t counterBSM1 = 0;
uint8_t counterSAS = 0;
uint8_t counterABS = 0;

// Buffer ISO-TP
byte isoBuffer[RX_BUFFER_SIZE];
uint16_t isoExpected = 0;
uint16_t isoReceived = 0;
byte expectedSequence = 1;
bool isoActive = false;
unsigned long lastIsoTpFrameAt = 0;
#define ISOTP_TIMEOUT 4000UL

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println(F("\n=== EPS NEXTEER: ISO-TP RAW TRACE & EMULAÇÃO CRONOS ==="));

  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println(F("MCP2515 Inicializado com sucesso a 500 kbps!"));
  } else {
    Serial.println(F("Erro crítico ao inicializar o MCP2515."));
    while (1);
  }

  CAN.setMode(MCP_NORMAL);
  bootTime = millis();
  nextActionAt = bootTime + 2000;
}

void loop() {
  unsigned long now = millis();

  // 1. Transição do OperationalModeSts (0x70 -> 0x80 após ~1.2s)
  byte operationalModeByte = 0x70; 
  if (now - bootTime > 1200) {
    operationalModeByte = 0x80;    
    if (!operationalStateAchieved) {
      Serial.println(F("-> Transição de OperationalModeSts aplicada (0x80)."));
      operationalStateAchieved = true;
    }
  }

  // --- MÁQUINA DE ESTADOS UDS ---
  if (udsState == 0 && now >= nextActionAt) {
    byte msg[8] = {0x02, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
    if (CAN.sendMsgBuf(UDS_TX_ID, 1, 8, msg) == CAN_OK) {
      Serial.println(F("[UDS] Solicitando Sessão Estendida (10 03)..."));
      udsState = 1;
      nextActionAt = now + 1000;
    }
  }
  else if (udsState == 1 && now >= nextActionAt) {
    byte msg[8] = {0x04, 0x14, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00};
    if (CAN.sendMsgBuf(UDS_TX_ID, 1, 8, msg) == CAN_OK) {
      Serial.println(F("[UDS] Enviando Limpeza de DTCs (14 FF FF FF)..."));
      udsState = 2;
      nextActionAt = now + 1000;
    }
  }
  else if (udsState == 2 && now >= nextActionAt) {
    byte msg[8] = {0x03, 0x19, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00};
    if (CAN.sendMsgBuf(UDS_TX_ID, 1, 8, msg) == CAN_OK) {
      Serial.println(F("[UDS] Consultando quantidade de DTCs (19 01)..."));
      udsState = 3;
      nextActionAt = now + 1200;
    }
  }
  else if (udsState == 3 && now >= nextActionAt) {
    byte msg[8] = {0x03, 0x19, 0x02, 0xFF, 0x00, 0x00, 0x00, 0x00};
    if (CAN.sendMsgBuf(UDS_TX_ID, 1, 8, msg) == CAN_OK) {
      Serial.println(F("[UDS] Consultando lista detalhada (19 02)..."));
      udsState = 4;
    }
  }

  // --- BACKGROUND: Ciclo C-CAN Completo (Emulação do Cronos 1.3) ---

  if (now - prevBCMCmd >= 20) {
    prevBCMCmd = now;
    byte data[5] = {operationalModeByte, 0x00, 0x00, 0x00, 0x00}; 
    CAN.sendMsgBuf(0x0FA, 0, 5, data);
  }

  if (now - prevBSM1 >= 10) {
    prevBSM1 = now;
    byte data[8];
    uint16_t vehSpeed = 0; 
    data[0] = (vehSpeed >> 5) & 0xFF;
    data[1] = ((vehSpeed & 0x1F) << 3); 
    data[2] = 0x00; data[3] = 0x00; data[4] = 0x00; data[5] = 0x00;
    data[6] = counterBSM1 & 0x0F;
    data[7] = calcularCRCSAEJ1850(data, 7);
    if (CAN.sendMsgBuf(0x0F2, 0, 8, data) == CAN_OK) {
      counterBSM1 = (counterBSM1 + 1) & 0x0F;
    }
  }

  if (now - prevMOT1 >= 10) {
    prevMOT1 = now;
    byte data[8] = {0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
    CAN.sendMsgBuf(0x0E6, 0, 8, data);
  }

  if (now - prevMOT3 >= 20) {
    prevMOT3 = now;
    byte data[8] = {0x10, 0x0E, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00};
    CAN.sendMsgBuf(0x2EE, 0, 8, data);
  }

  if (now - prevABS >= 20) {
    prevABS = now;
    byte absData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, (byte)(counterABS & 0x0F), 0x00};
    if (CAN.sendMsgBuf(0x212, 0, 8, absData) == CAN_OK) {
      counterABS = (counterABS + 1) & 0x0F;
    }
  }

  if (now - prevClutch >= 20) {
    prevClutch = now;
    byte clutchData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CAN.sendMsgBuf(0x1F0, 0, 8, clutchData);
  }

  if (now - prevHandbrake >= 50) {
    prevHandbrake = now;
    byte hbData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CAN.sendMsgBuf(0x256, 0, 8, hbData);
  }

  if (now - prevBCM2 >= 20) {
    prevBCM2 = now;
    byte data[5] = {0x00, 0x80, 0x00, 0x00, 0x00}; 
    CAN.sendMsgBuf(0x4B4, 0, 5, data);
  }

  if (now - prevIPC >= 100) {
    prevIPC = now;
    byte data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CAN.sendMsgBuf(0x419, 0, 8, data);
  }

  if (now - prevSAS >= 10) {
    prevSAS = now;
    byte sasData[8] = {0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, (byte)(counterSAS & 0x0F), 0x00};
    sasData[7] = calcularCRCSAEJ1850(sasData, 7);
    if (CAN.sendMsgBuf(0x232, 0, 8, sasData) == CAN_OK) {
      counterSAS = (counterSAS + 1) & 0x0F;
    }
  }

  if (now - prevPROXI >= 100) {
    prevPROXI = now;
    byte proxiBytes[6] = {0x33, 0x35, 0x80, 0x10, 0x18, 0x50}; 
    CAN.sendMsgBuf(0x1E114000, 1, 6, proxiBytes);
  }

  // --- ESCUTA REATIVA UDS E ISO-TP ---
  unsigned long rxId;
  byte ext;
  byte len;
  byte buf[8];

  while (CAN.checkReceive() == CAN_MSGAVAIL) {
    if (CAN.readMsgBuf(&rxId, &ext, &len, buf) == CAN_OK) {
      processFrame(rxId, ext, len, buf);
    }
  }

  if (isoActive && now - lastIsoTpFrameAt > ISOTP_TIMEOUT) {
    Serial.println(F("-> [ISO-TP] Timeout por falta de frames consecutivos."));
    isoActive = false;
  }
}

// -----------------------------------------------------
// ROTINAS ISO-TP COM RASTREIO BRUTO
// -----------------------------------------------------

void processUDS(const byte *data, uint16_t length) {
  if (length >= 2 && data[0] == 0x50) {
    Serial.println(F("-> [UDS] Sessão Estendida confirmada (0x50)."));
  }
  else if (length >= 2 && data[0] == 0x54) {
    Serial.println(F("-> [UDS] Limpeza de DTCs confirmada (0x54)."));
  }
  else if (length >= 2 && data[0] == 0x59) {
    if (data[1] == 0x01) {
      uint16_t count = ((uint16_t)data[4] << 8) | data[5];
      Serial.print(F("-> [UDS 19 01] Total de falhas ativas: "));
      Serial.println(count);
    }
    else if (data[1] == 0x02) {
      Serial.println(F("\n======================================================="));
      Serial.println(F("          RELATÓRIO COMPLETO DE FALHAS (19 02)         "));
      Serial.println(F("======================================================="));
      
      uint16_t recordBytes = length - 3;
      if (recordBytes == 0) {
        Serial.println(F("Nenhum DTC retornado! Sistema limpo."));
      } else {
        uint8_t index = 1;
        for (uint16_t i = 3; i + 3 < length; i += 4) {
          Serial.print(F(" [#"));
          if (index < 10) Serial.print("0");
          Serial.print(index++);
          Serial.print(F("] DTC: 0x"));
          printHexChar(data[i]); printHexChar(data[i+1]); printHexChar(data[i+2]);
          Serial.print(F(" | Status Byte: 0x"));
          printHexChar(data[i+3]);
          Serial.println();
        }
      }
      Serial.println(F("=======================================================\n"));
    }
  }
}

void processFrame(unsigned long id, byte ext, byte len, byte *buf) {
  if (ext != 1 || id != UDS_RX_ID || len == 0) return;

  // Mostra absolutamente tudo que entra no canal de diagnóstico da EPS
  Serial.print(F("RAW RX [0x")); Serial.print(id, HEX); Serial.print(F("]: "));
  for (int i = 0; i < len; i++) {
    printHexChar(buf[i]); Serial.print(' ');
  }
  Serial.println();

  byte pci = (buf[0] >> 4) & 0x0F;

  if (pci == 0x00) { // Single Frame
    uint16_t size = buf[0] & 0x0F;
    isoActive = false;
    for (uint16_t i = 0; i < size; i++) isoBuffer[i] = buf[i + 1];
    processUDS(isoBuffer, size);
  }
  else if (pci == 0x01) { // First Frame
    isoExpected = ((uint16_t)(buf[0] & 0x0F) << 8) | buf[1];
    isoReceived = len - 2;
    for (uint16_t i = 0; i < isoReceived; i++) isoBuffer[i] = buf[i + 2];
    expectedSequence = 1;
    isoActive = true;
    lastIsoTpFrameAt = millis();
    
    Serial.println(F("-> First Frame capturado. Enviando Flow Control com STmin = 10ms..."));
    sendFlowControl();
    
    if (isoReceived >= isoExpected) {
      isoActive = false;
      processUDS(isoBuffer, isoExpected);
    }
  }
  else if (pci == 0x02 && isoActive) { // Consecutive Frame
    byte sequence = buf[0] & 0x0F;
    if (sequence != expectedSequence) {
      Serial.println(F("-> [ISO-TP] Erro de sequência em Consecutive Frame."));
      isoActive = false;
      return;
    }
    expectedSequence = (expectedSequence + 1) & 0x0F;
    uint16_t remaining = isoExpected - isoReceived;
    uint16_t available = len - 1;
    uint16_t amount = (available < remaining) ? available : remaining;
    for (uint16_t i = 0; i < amount; i++) {
      isoBuffer[isoReceived + i] = buf[i + 1];
    }
    isoReceived += amount;
    lastIsoTpFrameAt = millis();

    if (isoReceived >= isoExpected) {
      isoActive = false;
      Serial.println(F("-> [ISO-TP] Todos os quadros recebidos! Montando UDS..."));
      processUDS(isoBuffer, isoExpected);
    }
  }
}

void sendFlowControl() {
  // FC: Frame Type = 3, Status = 0 (Clear to Send), BS = 0 (Sem limite), STmin = 0x0A (10ms)
  byte frame[8] = {0x30, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};
  CAN.sendMsgBuf(UDS_TX_ID, 1, 8, frame);
}

void printHexChar(byte b) {
  if (b < 0x10) Serial.print('0');
  Serial.print(b, HEX);
}

byte calcularCRCSAEJ1850(byte *data, byte length) {
  byte crc = 0xFF;
  for (byte i = 0; i < length; i++) {
    crc ^= data[i];
    for (byte j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x1D;
      } else {
        crc = crc << 1;
      }
    }
  }
  return ~crc;
}