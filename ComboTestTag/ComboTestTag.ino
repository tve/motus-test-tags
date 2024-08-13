#include <Arduino.h>
#include "ArduinoLowPower.h"
#include "RTCZero.h"
#include <RadioLib.h> // https://github.com/jgromes/RadioLib
#include <CRC.h> // https://github.com/RobTillaart/CRC

// ===== Configurable settings

#if 1
constexpr uint32_t INITIAL_HOURS = 12; // for how many hours to TX every few seconds
constexpr uint32_t RUN_LENGTH = 7; // after INITIAL_HOURS how many TX in a run
constexpr uint32_t SLEEP_MINUTES = 67; // after INITIAL_HOURS how long to sleep between runs
#else
// values for testing
constexpr uint32_t INITIAL_HOURS = 1; // for how many hours to TX every few seconds
constexpr uint32_t RUN_LENGTH = 7; // after INITIAL_HOURS how many TX in a run
constexpr uint32_t SLEEP_MINUTES = 17; // after INITIAL_HOURS how long to sleep between runs
#endif

// Lotek test tag code
// The array has the three pulse intervals in milliseconds, and the interval between bursts in 1/10th seconds
// "TestTags:4"
// WARNING: do not change this randomly: you are likely generate bad data that compromises scientific
// projects by producing fake detections. A valid use-case would be to change to the code of a test
// tag code you own and have registered with Motus.
static const uint16_t lotekTag[] = {22, 34, 73, 53}; // ms, ms, ms, 1/10th sec

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

void setupLotek() {
  // Generate packet to match desired burst
  for (int i=0; i<MAXPKT; i++) ltkPacket[i] = 0;
  int off = 24; addPulse(off);
  off += lotekTag[0];
  for (int i=0; i<3; i++, off+=lotekTag[i]) ltkPacketLen = addPulse(off);
  radio.fixedPacketLengthMode(ltkPacketLen);

  Serial.print("Lotek tag [");
  Serial.print(lotekTag[0]); Serial.print(" ");
  Serial.print(lotekTag[1]); Serial.print(" ");
  Serial.print(lotekTag[2]); Serial.print("] ");
  Serial.print(lotekTag[3]/10); Serial.print(".");
  Serial.print(lotekTag[3]%10); Serial.print("ms @");
  Serial.print(LOTEK_FREQ); Serial.println("Mhz");

  // print OOK packet for debugging
  Serial.print(ltkPacketLen); Serial.print(":");
  for (int i=0; i<ltkPacketLen; i++) {
    Serial.print(" ");
    Serial.print(ltkPacket[i], 16);
  }
  Serial.println();
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
    radio.setSyncWord(nullptr, 0);
    radio.setCurrentLimit(100);
    radio.setDataShapingOOK(1);
    radio.setCRC(0);
    radio.fixedPacketLengthMode(ltkPacketLen);
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
    radio.fixedPacketLengthMode(5);
    radio.setSyncWord((uint8_t*)sync, sizeof(sync));
    // radio.setWhitening(false);
    radio.setDataShaping(RADIOLIB_SHAPING_NONE);
    radio.setCRC(0);
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

  if (count > oneDay && count % RUN_LENGTH == 0) {
    // after the first 24 hours sleep for 67 minutes every 7 transmissions
    deepSleep(SLEEP_MINUTES * 60 * 1000);
  } else {
    // sleep for the interval between bursts
    // this is a bit complicated 'cause deepSleep operates at 1 second granularity and we need better
    uint32_t dt = millis() - t0;
    uint32_t d = ltkInterval - dt;
    Serial.println(d);
    if (d > 1100 && count > 2) deepSleep(d);
    else delay(d);
  }
  t0 = millis();

  digitalWrite(LED_BUILTIN, 1);

  int status1 = configLotek();
  if (status1 != 0) blink();
  else radio.transmit(ltkPacket, ltkPacketLen);

  int status2 = configCTT();
  if (status2 != 0) blink();
  else radio.transmit(cttPacket, 5);
  radio.sleep();

  bool statusOK = status1 == 0 && status2 == 0;
  if (count <= 200) {
    if (statusOK) {
      Serial.print("TX @");
      Serial.print(POW);
      Serial.println("dBm");
    } else {
      Serial.print("Err: ");
      Serial.print(status1);
      Serial.print(" ");
      Serial.println(status2);
    }
  }
  count++;
}

    // Serial.end();
    // USBDevice.detach();
    // USBDevice.attach();
    // Serial.begin(115200);
