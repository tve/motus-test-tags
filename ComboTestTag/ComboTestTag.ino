#include <Arduino.h>
#include "ArduinoLowPower.h"
#include "RTCZero.h"
#include <RadioLib.h> // https://github.com/jgromes/RadioLib
#include <CRC.h> // https://github.com/RobTillaart/CRC

// ===== Configurable settings

#if 1
constexpr uint32_t INITIAL_HOURS = 12; // for how many hours to TX every few seconds
constexpr bool SWITCH_8X = true; // after INITIAL_HOURS switch to 8x the interval in lotekTag[3]
constexpr uint32_t RUN_LENGTH = 15; // after INITIAL_HOURS how many TX in a run
constexpr uint32_t SLEEP_MINUTES = 67; // after INITIAL_HOURS how long to sleep between runs
#else
// values for testing
constexpr uint32_t INITIAL_HOURS = 1;
constexpr bool SWITCH_8X = true;
constexpr uint32_t RUN_LENGTH = 7;
constexpr uint32_t SLEEP_MINUTES = 17;
#endif

// Lotek test tag code
// WARNING:
// Do not change this randomly: you are likely generate bad data that compromises scientific
// projects by producing fake detections. A valid use-case would be to change to the code of a test
// tag code you own and have registered with Motus.
// The values set correspond to Lotek ID#732, which has been dedicated for beacon tags, and the
// the 40s interval in particular is used by Motus for that purpose, allowing special processing of
// such tag detections.
// The interval is set to 5 seconds here, which is more convenient at start-up to quickly verify
// reception, it will be detected as tag by the back-end every 8 intervals, and it switches to 8x
// the interval after the INITIAL_HOURS (assuming SWITCH_8X is true above).
// The array has the three pulse intervals in milliseconds, and the interval between bursts in 1/10th seconds
static const uint16_t lotekTag[] = {159, 156, 34, 50}; // ms, ms, ms, 1/10th sec

// CTT test tag code
// The interval is the same as the Lotek test tag interval
// WARNING: do not change this randomly: you are likely generate bad data that compromises scientific
// projects by producing fake detections. A valid use-case would be to change to the code of a test
// tag code you own and have registered with Motus.
constexpr uint32_t cttTag = 0x78554c33; // TvE's test tag

// Transmit power in the range 2..17 (dBm), 2dBm should be enough for a tag placed at a station
constexpr float POW = 2;

// Lotek frequency (is passed into the compilation when using automated builds, hence the #ifndef)
#ifndef LTK_FREQ
#define LTK_FREQ 166.38
#endif
constexpr float LOTEK_FREQ = LTK_FREQ;

// ===== End of configurable settings

SX1278 radio = new Module(8, 3, 4); // D8:CS, D3:IRQ, D4:RST
void blink(); // forward declaration

void checkBlink(int16_t status) {
  if (status == RADIOLIB_ERR_NONE) return;
  Serial.print("Radiolib error ");
  Serial.println(status);
  blink();
}

// ===== Lotek radio configuration

#define MAXPKT 200
static uint8_t ltkPacket[MAXPKT];
static uint8_t ltkPacketLen = 0;
constexpr int L_BR = 2; // bit rate in kbps
static uint32_t ltkInterval = lotekTag[3] * 100; // milliseconds

uint8_t addPulse(uint16_t millis) {
  uint8_t byteOff = (millis*L_BR)>>3;
  uint8_t bitOff = (millis*L_BR) & 7;
  for (int i=0; i<5; i++) {
    ltkPacket[byteOff] |= 0x80 >> bitOff;
    bitOff++;
    if (bitOff == 8) {
      bitOff = 0;
      byteOff++;
    }
  }
  return byteOff+1;
}

// volatile bool ltkMore;

// void fifoAdd(void) {
//   ltkMore = true;
// }

void setupLotek() {
  // Generate packet to match desired burst
  for (int i=0; i<MAXPKT; i++) ltkPacket[i] = 0;
  int off = 4; addPulse(off);
  off += lotekTag[0];
  for (int i=0; i<3; i++, off+=lotekTag[i]) ltkPacketLen = addPulse(off);
  // for (int i=0; i<ltkPacketLen; i++) if (ltkPacket[i] == 0) ltkPacket[i] = 0x04;
  // ltkPacket[ltkPacketLen+4] = 0xff;
  // ltkPacket[ltkPacketLen+5] = 0xff;
  // ltkPacketLen+=6;
  // for (int i=0; i<ltkPacketLen; i++) ltkPacket[i] = 0xff;

  Serial.print("Lotek tag [");
  Serial.print(lotekTag[0]); Serial.print(" ");
  Serial.print(lotekTag[1]); Serial.print(" ");
  Serial.print(lotekTag[2]); Serial.print("] ");
  Serial.print(lotekTag[3]/10); Serial.print(".");
  Serial.print(lotekTag[3]%10); Serial.print("s @");
  Serial.print(LOTEK_FREQ); Serial.println("Mhz");

  // print OOK packet for debugging
  Serial.print(ltkPacketLen); Serial.print(":");
  for (int i=0; i<ltkPacketLen; i++) {
    Serial.print(" ");
    Serial.print(ltkPacket[i], 16);
  }
  Serial.println();
  if (ltkPacketLen > MAXPKT) {
    Serial.println("Lotek tag burst is too long!");
    blink();
  }
}

int configLotek() {
  constexpr float FREQDEV = 0;
  constexpr float RXBW = 9.7;
  constexpr float PRELEN = 0;
  constexpr bool ENABLEOOK = true;

  int16_t state = radio.beginFSK(LOTEK_FREQ, L_BR, FREQDEV, RXBW, POW, PRELEN, ENABLEOOK);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("Unable to init OOK, code "));
    Serial.println(state);
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    checkBlink(radio.setSyncWord(nullptr, 0));
    checkBlink(radio.setCurrentLimit(100));
    checkBlink(radio.setDataShapingOOK(1));
    checkBlink(radio.setCRC(0));
    // checkBlink(radio.fixedPacketLengthMode(ltkPacketLen));
    // radio.setFifoEmptyAction(fifoAdd); // DIO1 not connected, so no point calling this...
    checkBlink(radio.fixedPacketLengthMode(0));
  }

  return state;
}

// ===== CTT radio configuration

static uint8_t cttPacket[5];

void setupCTT() {
  cttPacket[0] = (cttTag >> 24) & 0xFF;
  cttPacket[1] = (cttTag >> 16) & 0xFF;
  cttPacket[2] = (cttTag >> 8) & 0xFF;
  cttPacket[3] = cttTag & 0xFF;
  cttPacket[4] = calcCRC8(cttPacket, 4);

  Serial.print("CTT tag 0x");
  Serial.print(cttTag, 16);
  Serial.print(" (");
  Serial.print(cttPacket[4], 16);
  Serial.println(")");
}

int configCTT() {
  constexpr float FREQ = 434.0;
  constexpr float BR = 25;
  constexpr float FREQDEV = 25;
  constexpr float RXBW = 58.6;
  constexpr int PRELEN = 24;
  constexpr bool ENABLEOOK = false;
  static const uint8_t sync[2] = {0xd3, 0x91};
  int state = radio.beginFSK(FREQ, BR, FREQDEV, RXBW, POW, PRELEN, ENABLEOOK);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("Unable to init FSK, code "));
    Serial.println(state);
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    radio.setFifoEmptyAction(nullptr);
    checkBlink(radio.fixedPacketLengthMode(5));
    checkBlink(radio.setSyncWord((uint8_t*)sync, sizeof(sync)));
    // checkBlink(radio.setWhitening(false));
    checkBlink(radio.setDataShaping(RADIOLIB_SHAPING_NONE));
    checkBlink(radio.setCRC(0));
  }
  return state;
}

// ===== Utilities

void blink() {
  for (int i=0; i<10; i++) {
    digitalWrite(LED_BUILTIN, 1);
    delay(50);
    digitalWrite(LED_BUILTIN, 0);
    delay(50);
  }
}

uint32_t rtcOffset = 0;

void deepSleep(uint32_t ms) {
  uint32_t frac = ms % 1000; 
  uint32_t secs = ms - frac; // really whole
  uint32_t left = 1000 - (millis()-rtcOffset) % 1000; // millis to go in the current RTC second
  // Serial.print(secs); Serial.print(" ");
  // Serial.print(frac); Serial.print(" ");
  // Serial.print(left); Serial.print(" ");
  // Serial.println(frac < left);
  if (frac < left) {
    LowPower.deepSleep(secs-1000); // sleeps left + secs-1000
    rtcOffset = millis() % 1000;
    delay(1000+frac-left);
  } else {
    LowPower.deepSleep(secs); // sleeps left + secs
    rtcOffset = millis() % 1000;
    delay(frac-left);
  }
}

// ===== Common setup & loop

uint32_t next;
uint32_t count = 0;
uint32_t oneDay = INITIAL_HOURS * 3600 *10/lotekTag[3]; // number of intervals after which to switch mode

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 1);

  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis()-t0 < 2000) { delay(1); } // blocks 'til USB is opened
  delay(1000); // get a nice long blink...

  Serial.println("\n\nMotus Lotek/CTT combo test tag");
  setupLotek();
  setupCTT();
  digitalWrite(LED_BUILTIN, 0);

  // Serial.print("Port A.DIR ");
  // Serial.println(PORT->Group[0].DIR.reg, 16);
  // Serial.print("Port B.DIR ");
  // Serial.println(PORT->Group[1].DIR.reg, 16);

  next = millis() + 500;
}

uint32_t t0 = 0;

void loop() {
  digitalWrite(LED_BUILTIN, 0);

  // sleep in-between bursts

  if (count > oneDay && count % RUN_LENGTH == 0) {
    // after the first 24 hours sleep for SLEEP_MINUTES minutes every RUN_LENGTH transmissions
    deepSleep(SLEEP_MINUTES * 60 * 1000);
  } else {
    // sleep for the interval between bursts
    // this is a bit complicated 'cause deepSleep operates at 1 second granularity and we need better
    uint32_t dt = millis() - t0;
    uint32_t intv = SWITCH_8X && count > oneDay ? ltkInterval * 8 : ltkInterval;
    uint32_t d = intv - dt;
    // Serial.println(d);
    if (d > 1100 && count > 2) deepSleep(d);
    else delay(d);
  }
  t0 = millis();

  digitalWrite(LED_BUILTIN, 1);

  // transmit Lotek burst

  int status1 = configLotek();
  if (status1 != 0) blink();
  else {
    const uint32_t dly = (RADIOLIB_SX127X_FIFO_THRESH-1) * 4; // 4ms per byte
    const uint32_t dly1 = (ltkPacketLen % (RADIOLIB_SX127X_FIFO_THRESH-1)) * 4 + 10;
    status1 = radio.startTransmit(ltkPacket, ltkPacketLen);
    if (status1 == 0) {
      int remainder = ltkPacketLen;
      // fifoAdd returns true when there was no more data to be sent
      while (!radio.fifoAdd(ltkPacket, ltkPacketLen, &remainder)) delay(dly);
      delay(dly1);
    }
  }

  // transmit CTT packet

  int status2 = configCTT();
  if (status2 != 0) blink();
  else status2 = radio.transmit(cttPacket, 5);

  radio.sleep();

  // debug printing

  bool statusOK = status1 == 0 && status2 == 0;
  if (count <= 200) {
    if (statusOK) {
      Serial.print("TX @"); Serial.print(POW); Serial.print("dBm in ");
      Serial.print(millis()-t0); Serial.println("ms");
    } else {
      Serial.print("Err: "); Serial.print(status1);
      Serial.print(" "); Serial.println(status2);
    }
    // Serial.println("=====");
    if (count == 200) Serial.end();
  }
  count++;
}

    // Serial.end();
    // USBDevice.detach();
    // USBDevice.attach();
    // Serial.begin(115200);
