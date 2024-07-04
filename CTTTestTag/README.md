RF test for Mostag
==================

This code is a proof of concept to test Mostag as a Motus tag on 434Mhz.
It uses the Arduino RadioLib to manage the radio, which is inefficient but easy to use.
The main purpose is to be able to validate that the radio can be configured as a Motus tag.

## RadioLib settings

```
  #define FREQ 434
  #define BR 25
  #define FREQDEV 25
  #define RXBW 58.6
  #define POW 10
  #define PRELEN 32
  #define TCXOV 0 // no TCXO, using xtal
  #define USELDO false
  int state = radio.beginFSK(FREQ, BR, FREQDEV, RXBW, POW, PRELEN, TCXOV, USELDO);
```

Results with output power levels (dBm) `{14, 10, 5, 0, -5, -9}`:
20mA, 15, 10, 8, 6.8, 6.1, @3.3V

![Scope screen shot](2024-03-01_09:13_TX_6x.png)

## First battery measurements (2024/3/1)

- MS621FE battery, charged from factory around 2.6V
- 220uF tantalum cap, not low-leakage
- RadioLib, TX @-9dBm
- STM32LowPower library deepSleep()
- uCurrent using 1 Ohm shunt
- scope #1:PA1, #2:Vss, #4:uCurrent

Battery voltage and current for one transmission:

![Low power TX on battery](2024-03-01_11:23_TX_bat_1.png)

Same but coarser time resolution showing extent of battery droop:

![Low power TX on battery](2024-03-01_11:23_TX_bat_2.png)

Same but higher resolution on the current:

![Low power TX on battery](2024-03-01_11:23_TX_bat_3.png)

Same with on-screen measurements:

![Low power TX on battery](2024-03-01_11:57_TX_bat_4.png)

## Serial programming connections

- PA2: TX (pin 5/RX on ftdi)
- PA3: RX (pin 4/TX on ftdi)
- BOOT0: RTS (pin 2 on ftdi)
- nRST: DTR (pin 6 on ftdi)

### Observations:

- TX power consumption duration is almost exactly 4ms
- Battery & cap droop from 2.46V to 2.18V during TX, should try smaller cap to see battery behavior
- Battery takes ~60-80ms to recover to within 50mV of max voltage
- Start-up duration to come out of deep sleep before sketch runs again is 12.2ms
- Sketch duration before TX is 14ms
- TX-to-sleep duration is 800us and has some artificial delay due to RadioLib bug
  (can't get end-of-TX interrupt)
- Deep sleep current is 600uA @2.46V (yikes!)
- Wake-up current is 1.2mA @2.46V
- Extra power consumption during wake time is (assuming 2.4V):
  - 0.6mA * 33ms + 0.3mA * 17ms + 5.5mA * 4ms = 0.047mJ + 0.012mJ + 0.053mJ = 0.11mJ
  - calculating for 5dBm TX power: 0.16mJ

### Voltage sag

Observed:
- TX @-9dBm for 4ms using 22uF+5uF results in drop from 2.66V to 2V -> 24uJ
