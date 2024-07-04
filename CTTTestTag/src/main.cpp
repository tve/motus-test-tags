// Simple Motus/CTT test tag

#include <Arduino.h>
#include <RadioLib.h> // https://github.com/jgromes/RadioLib
#include <CRC.h> // https://github.com/RobTillaart/CRC

#define TAG_ID 0x78554c33 // TvE's test tag: use freely ;-)
//#define TAG_ID 0x613455FF // Another test tag
#define POW 5 // dBm output power, range -xx..17dBm
#define INTERVAL 2000 // TX interval in milliseconds

#if defined(__AVR_ATmega32U4__)
  #define RFM69_CS    8
  #define RFM69_INT   7
  #define RFM69_RST   4
  #define LED_BUILTIN 13
  RF69 radio = new Module(RFM69_CS, RFM69_INT, RFM69_RST);
#endif

// Encode 20-bit value into 32 bits
uint8_t code[] = {
  0x00, 0x07, 0x19, 0x1E, 0x2A, 0x2D, 0x33, 0x34, 0x4B, 0x4C, 0x52, 0x55,
  0x61, 0x66, 0x78, 0x7F, 0x80, 0x87, 0x99, 0x9E, 0xAA, 0xAD, 0xB3, 0xB4,
  0xCB, 0xCC, 0xD2, 0xD5, 0xE1, 0xE6, 0xF8, 0xFF };

uint32_t encode(uint32_t val20) {
  uint32_t val32 = 0;
  for (int i=0; i<4; i++) {
    val32 <<= 8;
    val32 |= code[val20 & 0x1F];
    val20 >>= 5;
  }
  return val32;
}

bool transmittedFlag = false;  // flag that a packet was transmitted
uint8_t packet[5];             // buffer for the packet
uint8_t sync[] = {0xD3, 0x91}; // sync word

// ******************************************************************************************* setup
void setup() {
  // Pin initialization
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);

  // prep packet
#ifdef TAG_ID
  packet[0] = (TAG_ID>>24) & 0xff;
  packet[1] = (TAG_ID>>16) & 0xff;
  packet[2] = (TAG_ID>>8) & 0xff;
  packet[3] = TAG_ID & 0xFF;
#else
  const id = 0x12345; // 20 bits
  uint32_t pkt = encode(uid);
  packet[0] = (pkt >> 24) & 0xFF;
  packet[1] = (pkt >> 16) & 0xFF;
  packet[2] = (pkt >> 8) & 0xFF;
  packet[3] = pkt & 0xFF;
#endif
  packet[4] = calcCRC8(packet, 4);

  while(!Serial);
  delay(10);
  Serial.print("\n\nCTT test tag ");
  Serial.print(packet[0], 16); Serial.print(" ");
  Serial.print(packet[1], 16); Serial.print(" ");
  Serial.print(packet[2], 16); Serial.print(" ");
  Serial.print(packet[3], 16); Serial.print(" (");
  Serial.print(packet[4], 16);
  Serial.println(")");

  SPI.begin();
  radio.reset();
  #define FREQ 434
  #define BR 25
  #define FREQDEV 25
  #define RXBW 50
  #define PRELEN 24 // longer than real tags use, but can only help...
  int state = radio.begin(FREQ, BR, FREQDEV, RXBW, POW, PRELEN);
  radio.fixedPacketLengthMode(5);
  radio.setSyncWord(sync, sizeof(sync));
  radio.setDataShaping(RADIOLIB_SHAPING_NONE);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("radio initialized");
  } else {
    Serial.print("radio init failed, code ");
    Serial.println(state);
    while (true) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
    }
  }
}

void rapidBlink() {
  for (int i=0; i<10; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
  }
}

// ***********************************************************************************************************
void loop() {
  transmittedFlag = false;
  int16_t state = radio.transmit(packet, sizeof(packet));
  if (state == RADIOLIB_ERR_NONE) {
    // packet was successfully sent
    Serial.println("TX done");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    // some other error occurred
    Serial.print("TX failed, code ");
    Serial.println(state);
    rapidBlink();
  }
  // delay(INTERVAL-100);
}
