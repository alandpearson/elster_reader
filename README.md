# elster_reader
Elster A1100 meter reading arduino

There are several projects around for this, but none of them seem complete or well documented.

In particular 3 individuals seem to have laid the foundations - Richard Gregory, Dave Berkley and Pedro Riveros.

I've taken their work and made it a little bit clearer.

I've also found the sensors recommended are (a) not available anymore (b) in australia! (c) expensive and (d) not 100% reliable.

This code has been tested in an Arduino Nano clone wiring a simple arduino IR line follower / obstacle avoidance sensor to digital 3.

These sensors are available on ebay for £1.50 - see the picture in pics directory.. 
They have a IR send LED an IR receive LED and a nice opamp to turn it into digital format.
Simply cut off the white transmit LED and turn the pot until the the circuit is triggeredd (LED on)
Then back off the pot a little bit until the receive LED on the board goes out. Hold DIRECTLY infront (touching) the Elster IR port.
The green LED shoud flash like mad and you're in business.

If enclosing this, make sure the receive LED can only see the Elster meter. In other words tape up the LED except the tip.
Don't forget the back side of the LED as light leaks in there easily (all the LEDs inside your case for example).
I found blutack great for this.

The code itself will print to to the serial just when it successfully decodes a reading.

Setting this up for the first time, then I recommend enabling DEBUG.
You are looking to see that you receive 328 bytes of information. The code will tell you how many it has received.
You are looking to see that if there are a lot of errors (or garbage) align your IR LED better and maybe reduce / increase the gain.
Avoid light leakage and try and seal the IR LED to the case to avoid this (including reflections).

The code will also flash an LED on digital 4 in time with IR pulse reception, helping you align the receiver to the IRDA port.

  Pedro said - I have tried some IR sensors so far the only one working at the moment is RPM7138-R.  Alan said = Give up on the commercial IR sensors, They are designed for remote controls with noise filtering, automatic gain control  and all sorts of stuff to make your life hard. RPM7138-R is discontinued. Trust me, I know. I order 8 from mouser, each different. None worked reliably. For those determined to use these, I found that ones optimised for 'short bursts' work at least somewhat, long burst ones not at all. YMMV.

  Based on Dave's code to read an elster A100C 
  http://www.rotwang.co.uk/projects/meter.html
  Thanks Dave.

  Based on Pedros code to read an Elster A1100 
  https://github.com/priveros/Elster_A1100
  Thanks Pedro.

Additionally this repo also contains a companion 'logger' program. This runs on a linux box, connects to serial port and logs the received data to a file as well as posting it via MQTT. 
I use the MQTT post to log to my OpenEnergyMonitor 'emonpi/emonbase'.  Of course, you can configure how you would like.


enjoy

