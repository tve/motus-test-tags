Motus Combo Test Tag
====================

The Motus Combo Test Tag acts as a Lotek and a CTT test tag for Motus stations.
The intended use is to carry in the pocket when servicing a station and be able to ensure that
everything works ends-to-end before leaving, as well as leaving a tag in the vicinity of a
station in order to get 24x7 monitoring of the station performance.

The test tag is built on the Adafruit
[Feather M0 with LoRa Radio Module](https://www.adafruit.com/product/3179)
board, 433Mhz version, product id 3179.
Also available at [Digi-Key](https://www.digikey.com/en/products/detail/adafruit-industries-llc/3179/6098604).

Also recommended: [1200mAh LiPO battery](https://www.adafruit.com/product/258) which should power
the tag for about 6 months. (This model has a protection circuit and the right connector to "just
plug in"). The tag can also be powered by plugging into USB.

![Adafruit product 3179](Adafruit_3179.png)

Functionality
-------------

- after power-on or reset the board transmits a Lotek ID and a CTT ID every 5 seconds for 12 hours
- after the first 12 hours the board transmits 15 times at 40 second interval every ~67 minutes
- the tag ID used for Lotek is #732, which is defined in Motus for beacon tags, the 40 second
  interval being designated for "permanent station beacons"
- the tag ID used for CTT is 78554c33, which is a solar tag owned by the author

Programming HOW-TO
------------------

- download the firmware from this repo's [releases](https://github.com/tve/motus-test-tags/releases),
  for the frequency in your region, the file name should end in `.uf2`, e.g. `ComboTestTag-166_38.uf2`
- plug the board into your computer (windows, mac, linux)
- double press the reset button on the board, the LED should slowly pulse
- drag'n'drop the downloaded file onto the drive
- watch the LED: it should stay fully lit for ~3 seconds and then turn off, then blink
  every 5 seconds (for each transmission)

Note: MacOS 13.0 (Ventura) [appears to have issues](https://blog.adafruit.com/2022/10/31/uploading-uf2-files-with-macos-13-0-ventura-apple-microbit_edu-raspberry_pi-circuitpython/)
  with this process, fixed in 13.1

Board specific notes
--------------------

- power via micro-USB connector or using a LiPO attached to the JST-PH connector on the board
- an attached LiPO is recharged when the board is plugged into USB
- try without antenna, it ought to work when at the station
- to add an antenna solder to the single hole on the short side of the board
- when using a LiPO to turn the battery on/off plug/unplug the LiPO, this is not super convenient
  but Adafruit claims that it works OK with the plug on its LiPO's (others use cheap versions that
  can be very difficult to get back out)

Power consumption
-----------------

The power consumption is disappointingly high. Not on a scale that would be noticeable when
plugged in, but the battery life is far shorter than it needs to be.
At the moment the bottom line is:

- the tag needs 160mAh per month, so a 1000mAh LiPO should last a bit over 6 months (untested!)

The specifics are that the tag consumes 165-170uA when idle and with a
15 bursts per hour schedule the TX consumption doesn't matter much.
It should be possible to bring the idle consumption down to around 60uA but it's unclear what
the problem is...
This is one downside of using an off-the-shelf board...

### Details

- TX: 20mA for ~500ms for tag #732 -> 10mJ
- Idle: 180uA
- 1000mAh LiPO: 3.6kJ
- ( 1J = 1mAs = 1 milli-Ampere-second )

interval  | 1hr idle | 1hr TX | days/1000mAh
---       | ---      | ---    | ---
5s        | 648mJ    | 7200mJ |  19
40s       | 648mJ    |  900mJ |  96
15brst/hr | 648mJ    |  150mJ | 188

Updating receivers
------------------

Detecting the CTT tag does not require any change to the receiver (Sensorgnome / SensorStation).
It will show up as 78554c33 or more likely as 78554c3358 where the added 58 is a CRC.

Detecting the Lotek tag at the receiver benefits from adding its specs to the local tag database.
Sensorgnome software V2.0 and later has it already. Earlier ones need it added. One option
is to SSH into the SG and run (all one long line):
```
sudo sqlite3 /etc/sensorgnome/SG_tag_database.sqlite "INSERT INTO tags VALUES('Beacon','732',166.38,166.376,159,156,34,40,0,0.0,0.0,0.0,0.0,0.0,'arduino','Lotek6M')"
```
You then need to restart the service: `sudo systemctl restart sg-control`.

Place the tag nearby and in the SG web interface on the radio tab in the tags&pulses pane you
should see something like:
```
PLS p5 16:28:48: 3.095kHz snr:9.7dB (-41.4/-51.5dB) 9622.2ms
PLS p5 16:28:48: 3.101kHz snr:10.2dB (-41.5/-52.1dB) 159.0ms
PLS p5 16:28:48: 3.097kHz snr:10.1dB (-41.5/-52.0dB) 156.0ms
PLS p5 16:28:49: 3.099kHz snr:12.1dB (-41.6/-54.0dB) 34.0ms
TAG L5 16:28:48: Beacon#732@166.38:40 3.098kHz snr:10.8dB (-41.5/-52.3dB)
```

On Sensorstations the sqlite3 command is the same except that the file location changes to
`/data/sg_files/SG_tag_database.sqlite` and restarting the service needs
`sudo systemctl restart sensorgnome`.
