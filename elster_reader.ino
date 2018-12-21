/*
  This code is been tested in an Arduino Uno clone wiring a comercial infrared
  sensor in pin 2. it will print to to the serial just when it detects a change
  in Total Imports, Total exports or a change in direction (0=Importing , 1=Exporting)

  I have tried some IR sensors so far the only one working at the moment is RPM7138-R

  Based on Dave's code to read an elter a100c for more info on that vist:
  http://www.rotwang.co.uk/projects/meter.html
  Thanks Dave.
*/
const uint8_t intPin = 5;
#define BIT_PERIOD 860 // us
//#define BIT_PERIOD 416 // us
//#define BIT_PERIOD 840 // us


//#define BIT_PERIOD 416 // us

#define BUFF_SIZE 64
volatile long data[BUFF_SIZE];
volatile uint8_t in;
volatile uint8_t out;
volatile unsigned long last_us;
uint8_t dbug = 1;

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
  //pinMode(intPin, INPUT);
  in = out = 0;
  Serial.begin(57600);
  //EICRA |= 3;    //RISING interrupt
  EICRA = 12;    //falling interrupt

  EIMSK = 2;

  if (dbug) Serial.print("Start ........\r\n");
  last_us = micros();
}

unsigned int statusFlag;
float imports;
float exports;

void loop() {
  //    decode_buff();
  int rd = decode_buff();
  if (!rd) return;
  if (rd == 3) {
    rd = 4;
    Serial.println("");
    Serial.print("imports");    Serial.print(imports);    Serial.print("\t");
    Serial.print("exports:");    Serial.print(exports) ; Serial.print("\t");
    Serial.print("statusFlag:");    Serial.print(statusFlag); Serial.println("");
  }
  //   unsigned long end_time = millis() + 6000;
  //   while (end_time >= millis()) ;
}

float last_data;
uint8_t sFlag;
float imps;
float exps;
uint16_t idx = 0;
uint16_t idx_max = 0;

uint8_t byt_msg = 0;
uint8_t bit_left = 0;
uint8_t bit_shft = 0;
uint8_t pSum = 0;
uint16_t BCC = 0;
uint8_t eom = 1;

static int decode_buff(void) {
  if (in == out) return 0;
  int next = out + 1;
  if (next >= BUFF_SIZE) next = 0;
  //p seems to be which 'bit' number we are curently receiving
  //each transmitted frame is 10bits
  int p = (((data[out]) + (BIT_PERIOD / 2)) / BIT_PERIOD);
  //if (dbug) {
    //Serial.print(data[out]);
    //Serial.print(" ");
    //if (p > 500) Serial.println("<-");
 // }
  if (p > 500) {
    idx = BCC = eom = imps = exps = sFlag = 0;
    out = next;
    return 0;
  }

  //bits remaining in this frame
  bit_left = (4 - (pSum % 5));
  /*
   * if (bit_left < p) {
   *  bit_shft = bit_left ;
   *  } else {
   *    bit_shft = p ;
   *  }
   * 
   */
   //if we have less bits left than we have processed
  bit_shft = (bit_left < p) ? bit_left : p;
  
  //if we have 10bits then we have a frame
  /*
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
//we are in. And we set (or unset?) it appropriately.
  if (bit_shft > 0) {
    byt_msg >>= bit_shft;
    if (p == 2) byt_msg += 0x40 << (p - bit_shft);
    if (p == 3) byt_msg += 0x60 << (p - bit_shft);
    if (p == 4) byt_msg += 0x70 << (p - bit_shft);
    if (p >= 5) byt_msg += 0xF0;
  }
  //    Serial.print(p); Serial.print(" ");Serial.print(pSum);Serial.print(" ");
  //    Serial.print(bit_left);Serial.print(" ");Serial.print(bit_shft);Serial.print(" ");
  //    Serial.println(byt_msg,BIN);
  if (pSum >= 10) {
    idx++;
    if (idx > idx_max) {
      idx_max = idx ;
    } else {
      Serial.print("bytes received=") ; Serial.println(idx_max) ;
      idx_max = 0 ;
    }
    if (idx != 328) BCC = (BCC + byt_msg) & 255;
    //if (dbug){Serial.print("[");Serial.print(idx);Serial.print(":");Serial.print(byt_msg,HEX); Serial.print("]");}
    //if (dbug){Serial.print((char)byt_msg) & 0x7f;}

    if (idx >= 95 && idx <= 101)
      imps += ((float)byt_msg - 48) * pow(10 , (101 - idx));
    if (idx == 103)
      imps += ((float)byt_msg - 48) / 10;
    if (idx >= 114 && idx <= 120)
      exps += ((float)byt_msg - 48) * pow(10 , (120 - idx));
    if (idx == 122)
      exps += ((float)byt_msg - 48) / 10;
    if (idx == 210)
      sFlag = (byt_msg - 48) >> 3; //1=Exporting ; 0=Importing
    if (byt_msg == 3) eom = 2;
    if (idx == 328) {
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
        Serial.println(""); Serial.print("---->>>>");
        Serial.print(imps); Serial.print("\t");
        Serial.print(exps); Serial.print("\t");
        Serial.print(sFlag); Serial.print("\t");
        Serial.print(pSum); Serial.print("\t");
        Serial.print(byt_msg >> (pSum == 10 ? 1 : 2), BIN); Serial.print("\t"); //BCC read
        Serial.print((~BCC) & 0x7F, BIN); Serial.print("\t"); //BCC calculated

      }
    }
  }
  out = next;
  return 0;
}
