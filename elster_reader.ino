/*
  This code is been tested in an Arduino Nano clone wiring a simple arduino IR line follower / obstacle avoidance sensor to digital 3.
  These sensors are available on ebay for £1.50. Simple cut off the white transmit LED and turn the pot until the the circuit is trigged (LED on)
  Then back off the pot a little bit until the receive LED on the board goes out. Hold DIRECTLY infront (touching) the Elster IR port.
  The green LED shoud flash like mad and you're in business.
  If enclosing this, make sure the receive LED can only see the Elster meter. In other words tape up the LED except the tip.
  Don't forget the back side of the LED as light leaks in there easily (all the LEDs inside your case for example).
  I found blutack great for this.

  It will print Total Imports, Total exports & direction (0=Importing , 1=Exporting) to the serial port on every read if the BCC checksum is correct.
  It will also flash an LED on digital 4 in time with IR pulse reception.

  Pedro said - I have tried some IR sensors so far the only one working at the moment is RPM7138-R
  Alan said = Give up on the commercial IR sensors, They are designed for remote controls with noise filtering
  and all sorts of stuff to make your life hard. RPM7138-R is discontinued. See the README.MD file

  Based on Dave's code to read an elter a100c for more info on that vist:
  http://www.rotwang.co.uk/projects/meter.html
  Thanks Dave.

  Based on Pedros code here :
  https://github.com/priveros/Elster_A1100
  Thanks Pedro.

  For the curious, here is what a real received IRDA packet looks like when printed as ASCII char type (with &07f to remove startbit) 
  OB96.1.1(Elster A1100)0.2.0(2-01166-K)96.1.0(000000)0.0.0(--------11278044)0.2.1(0000)1.8.1(0021325.5kWh)2.8.1(0000000.0kWh)1.8.2(0000000.0kWh)2.8.2(0000000.0kWh)1.8.0(0021325.5kWh)2.8.0(0000000.0kWh)96.5.0(01)96.4*0(01)97.97.0(00)96.8.1(025775Hrs)96.8.2(000000Hrs)96.7.0(000040)96.52.0(000008)96.53.0(000000)96.54.0(000000)<BCC>

*/


//debug to STDOUT - you'll need this. I know you will. But it will make your eyes bleed
//lookout for the commented (ifdef DEBUG) statements too.
#define DEBUG

// This will print the uSecs each 'mark' was received on IR by the interrupt handler
// handy to see if you are getting a somewhat consistent datastream coming in
// timings should be almost the same from msg to msg or else you have interference or poor reception
// don't use this at the same time as #define DEBUG or it will give non-sensical output.
#undef DEBUG_BITRX

//The Elster A1100 seems to run at a higher baud rate by default, the A100C used 416us, but this code won't work with a A100C
#define BIT_PERIOD 860 // us

#define BUFF_SIZE 64
volatile long data[BUFF_SIZE];

// in & out - in is written to as every bit is read by the ISR
// out is what we are processing / decoding into a bytestream
volatile uint8_t in;
volatile uint8_t out;

//last time a bit was received
volatile unsigned long last_us ;

//LED toggle on or off
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
//Here we use IRQ 1 (pin digital 3)
//All this does is record the microsecond time stamp
//of a '1' bit being received. The decoding of the full byte is done in decode_buff
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

  EICRA = 8;    //falling interrupt on IRQ1
  EIMSK = 2;    //only care about IRQ1

  pinMode(ledPin, OUTPUT) ;

  Serial.println("#A1100 reader started");
  Serial.println("#import_reading : export_reading : status_flag");

  last_us = micros();
}


void loop() {
  int rd = decode_buff();

  if (!rd) {
    //ledOff() ;
  } else if (rd == 3) {
    //Good decode !
    ledOff() ;
    rd = 4;
    Serial.println() ; Serial.print(imports);    Serial.print(":"); Serial.print(exports); Serial.print(":"); Serial.print(statusFlag);
  }


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



#ifdef DEBUG_BITRX
    //debug bit reception timings
    Serial.print(data[out]);
     Serial.print(" ");
    if (p > 500) Serial.println("<-");
#endif


  //if we received more than 500 bits, consider it a new frame, it's garbage
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

#ifdef DEBUG
      //we should have received 328 bytes if all is good
      Serial.print("bytes received=") ; Serial.print(idx_max) ; Serial.println("/328.");
#endif


      idx_max = 0 ;
    }
    if (idx != 328) {
      //Keep calculating what the BCC should be
      BCC = (BCC + byt_msg) & 255;
    }

#ifdef DEBUG
    //Serial.print("[");Serial.print(idx);Serial.print(":");Serial.print(byt_msg,HEX); Serial.print("]");
    if (idx != 328) {
      Serial.print((char)byt_msg) & 0x7f;
    } else {
      Serial.println(byt_msg >> (pSum == 10 ? 1 : 2), HEX);
    }

#endif

    //now decode what each byte actually is
    //Umm, yeah. I think there may be an easier way, but this appears to work.
    //I think the better approach to this whole problem is to receive the entire bytestring
    //then run it through a parser. But hey, this works and I'm grateful for it.

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
      //We are now receiving binary check digit (BCC)
      //begin if-then-else sanity.
      bool bccGood = false ;

      if (pSum == 10) {
        //All bits of BCC have been received. Now check it is correct.
#ifdef DEBUG
        Serial.println("");
#endif

        if ((~BCC) & 0b1000000) {
          if (byt_msg >> 0 == ((~BCC) & 0x7F) ) {
            bccGood = true ;
          }
        } else {
          if (byt_msg >> 1 == ((~BCC) & 0x7F) ) {
            bccGood = true ;
          }
        }

      }

      if (bccGood) {
        imports = imps;
        exports = exps;
        statusFlag = sFlag;
        last_data = imps + exps + sFlag;
        out = next;
        return 3;

      } else {
        //binary check failed
#ifdef DEBUG
        Serial.println("BCC checksum does not match.");
        Serial.print("imps:"); Serial.println(imps);
        Serial.print("exps:"); Serial.println(exps);
        Serial.print("sFlag:"); Serial.println(sFlag);
        Serial.print("pSum"); Serial.println(pSum);
        Serial.print("received BCC:"); Serial.println(byt_msg >> (pSum == 10 ? 1 : 2), BIN);
        //Binary check digit
        Serial.print("calculated BCC:");  Serial.println((~BCC) & 0x7F, BIN); //BCC calculated
#endif
      }
      /*
         OMG - this condensed (& double embedded if:then:else below has been santisied into above ^^
         end if-then-else insanity ye who enter here.
        if ((byt_msg >> (pSum == 10 ? (((~BCC) & 0b1000000) ? 0 : 1) : 2)) == ((~BCC) & 0x7F)) {
        // if (last_data != (imps + exps + sFlag)) {
          imports = imps;
          exports = exps;
          statusFlag = sFlag;
          last_data = imps + exps + sFlag;
          out = next;
          return 3;
        // }
        }
      */


    } // if(idx==328)
  }
  out = next;
  return 0;
}
