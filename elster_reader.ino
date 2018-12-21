/*
  This code is been tested in an Arduino Nano clone wiring a simple arduino IR line follower / obstacle avoidance sensor to digital 3.
  These sensors are available on ebay for £1.50. Simple cut off the white transmit LED and turn the pot until the the circuit is trigged (LED on)
  Then back off the pot a little bit until the receive LED on the board goes out. Hold DIRECTLY infront (touching) the Elster IR port.
  The green LED shoud flash like mad and you're in business.
  If enclosing this, make sure the receive LED can only see the Elster meter. In other words tape up the LED except the tip.
  Don't forget the back side of the LED as light leaks in there easily (all the LEDs inside your case for example).
  I found blutack great for this.

  It will print to to the serial just when it detects a change
  in Total Imports, Total exports or a change in direction (0=Importing , 1=Exporting)

  It will also flash an LED on digital 4 in time with IR pulse reception.

  Pedro said - I have tried some IR sensors so far the only one working at the moment is RPM7138-R
  Alan said = Give up on the commercial IR sensors, They are designed for remote controls with noise filtering
  and all sorts of stuff to make your life hard. RPM7138-R is discontinued.

  Based on Dave's code to read an elter a100c for more info on that vist:
  http://www.rotwang.co.uk/projects/meter.html
  Thanks Dave.

  Based on Pedros code here :
  https://github.com/priveros/Elster_A1100
  Thanks Pedro.

*/

//The Elster A1100 seems to run at a higher baud rate by default
#define BIT_PERIOD 860 // us

//This is apparently what the A100c uses
//#define BIT_PERIOD 416 // us



#define BUFF_SIZE 64
volatile long data[BUFF_SIZE];

// in & out - in is written to as every bit is read by the ISR
// out is what we are processing / decoding into a bytestream
volatile uint8_t in;
volatile uint8_t out;

//last time a bit was received
volatile unsigned long last_us ;

//debug to STDOUT
uint8_t dbug = 0;

//LED on or off
bool LED = 0 ;

//which pin indicator LED is on
int ledPin = 4 ;


unsigned int statusFlag;
float imports;  //final decoded meter read
float exports;  //meter decoded meter read

float last_data;
uint8_t sFlag;  //meter, 0=importing, 1=exporting
float imps;      //used for constucting import value
float exps;     //used for constructing export value

uint16_t idx = 0;   //where we are in the re-constructed byte stream
uint16_t idx_max = 0; //largest number of bytes constructed so far

uint8_t byt_msg = 0;
uint8_t bit_left = 0;
uint8_t bit_shft = 0;
uint8_t pSum = 0;   //total of all timeperiods added together for this byte
uint16_t BCC = 0;   //binary check digit
uint8_t eom = 1;    //end of message marker



void ledToggle() {
  LED = !LED ;
  digitalWrite(ledPin, LED) ;
}

void ledOff() {
  digitalWrite(ledPin, 0) ;
}

//Interupt handler.
//Here we use IRQ 1 (ping digital 3)
//All this does is record the microsecond time stamp
//of a bit being received. The decoding is done in the loop
ISR(INT1_vect) {
  unsigned long us = micros();
  unsigned long diff = us - last_us;
  if (diff > 20 ) {
    last_us = us;
    int next = in + 1;
    if (next >= BUFF_SIZE) next = 0;
    data[in] = diff;
    in = next;
  }

}

void setup() {

  in = out = 0;
  Serial.begin(57600);

  // Setup interrupts, see here :
  // https://sites.google.com/site/qeewiki/books/avr-guide/external-interrupts-on-the-atmega328

  EICRA = 12;    //falling interrupt on IRQ1
  EIMSK = 2;

  pinMode(ledPin, OUTPUT) ;

  if (dbug) Serial.print("A1100 reader started.");
  last_us = micros();
}


void loop() {
  int rd = decode_buff();

  if (!rd) {
    ledOff() ;
  } else if (rd == 3) {
    rd = 4;
    Serial.println("");
    Serial.print("imports:");    Serial.print(imports);    Serial.print("\t");
    Serial.print("exports:");    Serial.print(exports);    Serial.print("\t");
    Serial.print("statusFlag:"); Serial.println(statusFlag);
  }

  ledOff() ;

}



//Where the work is done decoding each bit
//Best you read the Elster manual on this, appendix A
//It takes a little while to get your head around it, but when you do
//it's not so bad. Really.
// https://www.camax.co.uk/downloads/A1100-Manual.pdf

static int decode_buff(void) {
  
  if (in == out) return 0;
  int next = out + 1;
  if (next >= BUFF_SIZE) next = 0;
  //p seems to be which 'bit' number we are curently receiving
  //each transmitted frame is 10bits long
  int p = (((data[out]) + (BIT_PERIOD / 2)) / BIT_PERIOD);
  //if (dbug) {
  //Serial.print(data[out]);
  //Serial.print(" ");
  //if (p > 500) Serial.println("<-");
  // }

  //if we received more than 500 bits, consider it a new frame
  if (p > 500) {
    idx = BCC = eom = imps = exps = sFlag = 0;
    out = next;
    return 0;
  }

  //bits remaining in this frame
  bit_left = (4 - (pSum % 5));
  /*
     if (bit_left < p) {
      bit_shft = bit_left ;
      } else {
        bit_shft = p ;
      }

  */
  //if we have less bits left than we have processed
  bit_shft = (bit_left < p) ? bit_left : p;

  //if we have 10bits then we have a frame

  /*
     easier to read logic than pedros' shorthand C.. ugh.
    if (psum ==10) {
      psum = p ;
    } else if (pSum + p > 10) {
    psum = 10 ;
    } else {
    pSum = pSum + p ;
    }
  */
  pSum = (pSum == 10) ? p : ((pSum + p > 10) ? 10 : pSum + p);


  //if we have read more the 7th bit
  if (eom == 2 && pSum >= 7) {
    pSum = pSum == 7 ? 11 : 10;
    eom = 0;

  }

  //From what I can understand, the bytestream defaults to all zeros
  //when we get a 'mark' via IRDA via the INT, then based on time, we know which bit
  //we are in. And we set the previous bit to '1' appropriately as per the Elster coding.
  if (bit_shft > 0) {
    byt_msg >>= bit_shft;
    if (p == 2) byt_msg += 0x40 << (p - bit_shft);
    if (p == 3) byt_msg += 0x60 << (p - bit_shft);
    if (p == 4) byt_msg += 0x70 << (p - bit_shft);
    if (p >= 5) byt_msg += 0xF0;

  }

  if (pSum >= 10) {
    idx++;
    ledToggle() ;

    if (idx > idx_max) {
      idx_max = idx ;

    } else {
      if (dbug) {
        //we should have received 328 bytes if all is good
        Serial.print("bytes received=") ; Serial.print(idx_max) ; Serial.println("/328.");
      }
      idx_max = 0 ;
    }
    if (idx != 328) BCC = (BCC + byt_msg) & 255;
    //if (dbug){Serial.print("[");Serial.print(idx);Serial.print(":");Serial.print(byt_msg,HEX); Serial.print("]");}
    //if (dbug){Serial.print((char)byt_msg) & 0x7f;}

    //now decode what each byte actually is

    if (idx >= 95 && idx <= 101)
      //import kwh first
      imps += ((float)byt_msg - 48) * pow(10 , (101 - idx));
    if (idx == 103)
      imps += ((float)byt_msg - 48) / 10;
    if (idx >= 114 && idx <= 120)
      //export kwh
      exps += ((float)byt_msg - 48) * pow(10 , (120 - idx));
    if (idx == 122)
      exps += ((float)byt_msg - 48) / 10;
    if (idx == 210)
      //status flag
      sFlag = (byt_msg - 48) >> 3; //1=Exporting ; 0=Importing

    if (byt_msg == 3) eom = 2;

    if (idx == 328) {
      //full message has been received now
      if ((byt_msg >> (pSum == 10 ? (((~BCC) & 0b1000000) ? 0 : 1) : 2)) == ((~BCC) & 0x7F)) {
        if (last_data != (imps + exps + sFlag)) {
          imports = imps;
          exports = exps;
          statusFlag = sFlag;
          last_data = imps + exps + sFlag;
          out = next;
          return 3;
        }
      }
      if (dbug) {
        Serial.print("imps:"); Serial.println(imps);
        Serial.print("exps:"); Serial.println(exps);
        Serial.print("sFlag:"); Serial.println(sFlag);
        Serial.print("pSum"); Serial.println(pSum);
        Serial.print("byt_msg:"); Serial.println(byt_msg >> (pSum == 10 ? 1 : 2), BIN);
        //Binary check digit
        Serial.print("BCC:");  Serial.println((~BCC) & 0x7F, BIN); //BCC calculated

      }
    }
  }
  out = next;
  return 0;
}
