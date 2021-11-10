/*
// Simple DIY Electronic Music Projects
//    diyelectromusic.wordpress.com
//
//  Arduino Mozzi Sample Drum Sequencer
//  https://diyelectromusic.wordpress.com/2021/06/23/arduino-mozzi-sample-drum-sequencer/
//
      MIT License
      
      Copyright (c) 2020 diyelectromusic (Kevin)
      
      Permission is hereby granted, free of charge, to any person obtaining a copy of
      this software and associated documentation files (the "Software"), to deal in
      the Software without restriction, including without limitation the rights to
      use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
      the Software, and to permit persons to whom the Software is furnished to do so,
      subject to the following conditions:
      
      The above copyright notice and this permission notice shall be included in all
      copies or substantial portions of the Software.
      
      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
      IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
      FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
      COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHERIN
      AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
      WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/*
  Using principles from the following Arduino tutorials:
    Arduino Digital Input Pullup - https://www.arduino.cc/en/Tutorial/DigitalInputPullup
    Mozzi Library        - https://sensorium.github.io/Mozzi/
    Arduino Analog Input - https://www.arduino.cc/en/Tutorial/BuiltInExamples/AnalogInput
    Read Multiple Switches using ADC - https://www.edn.com/read-multiple-switches-using-adc/
  Some of this code is based on the Mozzi example "Sample" (C) Tim Barrass
*/
#include <MozziGuts.h>
#include <Sample.h> // Sample template
#include "d_kit.h"
#include <EventDelay.h>

EventDelay debounce;



#define OUTPUTSCALING 9

#define POT_KP1   A0
#define POT_KP2   A1
#define POT_TEMPO A5

#define D_NUM 4
#define D_BD  1
#define D_SD  2
#define D_CH  3
#define D_OH  4
int drums[D_NUM] = {D_BD,D_SD,D_OH,D_CH};

// use: Sample <table_size, update_rate> SampleName (wavetable)
Sample <BD_NUM_CELLS, AUDIO_RATE> aBD(BD_DATA);
Sample <SD_NUM_CELLS, AUDIO_RATE> aSD(SD_DATA);
Sample <CH_NUM_CELLS, AUDIO_RATE> aCH(CH_DATA);
Sample <OH_NUM_CELLS, AUDIO_RATE> aOH(OH_DATA);

#define TRIG_LED LED_BUILTIN
#define LED_ON_TIME 300 // On time in mS
bool trig[D_NUM];

// The following analog values are the suggested values
// for each switch (as printed on the boards).
//
// These are given assuming a 5V ALG swing and a 1024
// full range reading.
//
// Note: On my matrix, the switches were numbered
//       1,2,3,
//       4,5,6,
//       7,8,9,
//      10,0,11
//
//       But I'm keeping it simple, and numbering the
//       last row more logically as 10,11,12.
//
#define NUM_BTNS 12
#define BEAT0 22
#define BEAT1 23
#define BEAT2 24
#define BEAT3 25
#define BEAT4 26
#define BEAT5 27
#define BEAT6 28
#define BEAT7 29
#define CLEAR 4


#define ALG_ERR 8

int lastbtn1, lastbtn2;


byte Leds[8]= {32,33,34,35,36,37,38,39};


#define BEATS 8
#define DRUMS D_NUM
uint8_t pattern[BEATS][DRUMS];
unsigned long nexttick;
int seqstep;
uint16_t tempo  = 120;
int loopstate;

#ifdef CALIBRATE
void calcMaxMin (int drum) {
  int8_t max=0;
  int8_t min=0;
  int num_cells;
  int8_t *pData;
  switch (drum) {
    case D_BD:
       num_cells = BD_NUM_CELLS;
       pData = (int8_t *)&BD_DATA[0];
       break;
    case D_SD:
       num_cells = SD_NUM_CELLS;
       pData = (int8_t *)&SD_DATA[0];
       break;
    case D_CH:
       num_cells = CH_NUM_CELLS;
       pData = (int8_t *)&CH_DATA[0];
       break;
    case D_OH:
       num_cells = OH_NUM_CELLS;
       pData = (int8_t *)&OH_DATA[0];
       break;
    default:
       Serial.print("Unknown drum on calibration\n");
       return;
  }
  for (int i=0; i<num_cells; i++) {
    int8_t val = pgm_read_byte_near (pData+i);
    if (val > max) max = val;
    if (val < min) min = val;
  }
  Serial.print("Data for drum on pin ");
  Serial.print(drum);
  Serial.print(":\tMax: ");
  Serial.print(max);
  Serial.print("\tMin: ");
  Serial.println(min);  
}
#endif

void startDrum (int drum) {
  switch (drum) {
    case D_BD: aBD.start(); break;
    case D_SD: aSD.start(); break;
    case D_CH: aCH.start(); break;
    case D_OH: aOH.start(); break;
  }
}

unsigned long millitime;
void ledOff () {
  if (millitime < millis()) {
     digitalWrite(TRIG_LED, LOW);
  }
}

void ledOn () {
  millitime = millis() + LED_ON_TIME;
  digitalWrite(TRIG_LED, HIGH);
}

void setup () {
  pinMode(TRIG_LED, OUTPUT);
  ledOff();
  startMozzi();
  // Initialise all samples to play at the speed it was recorded
  aBD.setFreq((float) D_SAMPLERATE / (float) BD_NUM_CELLS);
  aSD.setFreq((float) D_SAMPLERATE / (float) SD_NUM_CELLS);
  aCH.setFreq((float) D_SAMPLERATE / (float) CH_NUM_CELLS);
  aOH.setFreq((float) D_SAMPLERATE / (float) OH_NUM_CELLS);
  Serial.begin(9600);
  
  Serial.println("starting");
  debounce.set(400);
  pinMode(BEAT0, INPUT_PULLUP);
  pinMode(BEAT1, INPUT_PULLUP);
  pinMode(BEAT2, INPUT_PULLUP);
  pinMode(BEAT3, INPUT_PULLUP);
  pinMode(BEAT4, INPUT_PULLUP);
  pinMode(BEAT5, INPUT_PULLUP);
  pinMode(BEAT6, INPUT_PULLUP);
  pinMode(BEAT7, INPUT_PULLUP);
  pinMode(CLEAR, INPUT_PULLUP);
/*#ifdef CALIBRATE
  //Serial.begin(9600);
  calcMaxMin (D_BD);
  calcMaxMin (D_SD);
  calcMaxMin (D_CH);
  calcMaxMin (D_OH);
#endif
*/

pinMode(32, OUTPUT);
pinMode(33, OUTPUT);
pinMode(34, OUTPUT);
pinMode(35, OUTPUT);
pinMode(36, OUTPUT);
pinMode(37, OUTPUT);
pinMode(38, OUTPUT);
pinMode(39, OUTPUT);
}

int drumScan;

// 1014 1004 970  931 839 700
byte lastdrumidx = 0;
void updateControl() {
  ledOff();
  // Only play the note in the pattern if we've met the criteria for
  // the next "tick" happening.
  unsigned long timenow = millis();
  if (timenow >= nexttick) {
    // Start the clock for the next tick.
    // Take the tempo mark as a "beats per minute" measure...
    // So time (in milliseconds) to the next beat is:
    //    1000 * 60 / tempo
    //
    // This assumes that one rhythm position = one beat.
    //
    nexttick = millis() + (unsigned long)(60000/tempo);

    // Now play the next beat in the pattern
    seqstep++;
    if (seqstep >= BEATS){
      seqstep = 0;
      // Flash the LED every cycle
      ledOn();
    }
    for (int d=0; d<DRUMS; d++) {
      if (pattern[seqstep][d] != 0) {
        startDrum(pattern[seqstep][d]);
      }
    }
  }

  

  if (loopstate == 0) {
    // Read the first keypad
    //int btn = mozziAnalogRead(5);
    //Serial.println(mozziAnalogRead(1)/256);
    int drumidx= (mozziAnalogRead(7)>>8);
    Serial.println(drumidx);
    int beatidx;
    if(lastdrumidx==drumidx){
    for(byte j = 0; j<BEATS;j++){
      if(pattern[j][drumidx] != 0){
        digitalWrite(Leds[j],HIGH);
        }
      else{
        digitalWrite(Leds[j],LOW);
        }
      }
    }
    else{
      for(byte j = 0; j<BEATS;j++){
        digitalWrite(Leds[j],LOW);
      }
      }
    lastdrumidx = drumidx;
    if(debounce.ready()){
    
    
    

    if(digitalRead(BEAT0)==0){
      beatidx = 0;
      Serial.print(beatidx);
      Serial.print(" ");
      Serial.println(drumidx);

      if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
         debounce.start();
      
      }
      if(digitalRead(BEAT1)==0){
      beatidx = 1;
      Serial.print(beatidx);
      Serial.print(" ");
      Serial.println(drumidx);

      if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
         debounce.start();
         
      }

      if(digitalRead(BEAT2)==0){
      beatidx = 2;
      Serial.print(beatidx);
      Serial.print(" ");
      Serial.println(drumidx);

      if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
         debounce.start();
         
      }

      if(digitalRead(BEAT3)==0){
      beatidx = 3;
      Serial.print(beatidx);
      Serial.print(" ");
      Serial.println(drumidx);

      if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
         debounce.start();
      }
    if(digitalRead(BEAT4)==0){
      beatidx = 4;
      Serial.print(beatidx);
      Serial.print(" ");
      Serial.println(drumidx);

      if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
         debounce.start();
      }

      if(digitalRead(BEAT5)==0){
      beatidx = 5;
      Serial.print(beatidx);
      Serial.print(" ");
      Serial.println(drumidx);

      if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
         debounce.start();
      }
      if(digitalRead(BEAT6)==0){
      beatidx = 6;
      Serial.print(beatidx);
      Serial.print(" ");
      Serial.println(drumidx);

      if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
         debounce.start();
      }

      if(digitalRead(BEAT7)==0){
      beatidx = 7;
      Serial.print(beatidx);
      Serial.print(" ");
      Serial.println(drumidx);

      if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
         debounce.start();
      }

      if(digitalRead(CLEAR)==0){

        for(byte k = 0; k<BEATS ;k++){
           pattern[k][0] = 0;
           pattern[k][1] = 0;
           pattern[k][2] = 0;
           pattern[k][3] = 0;
      
      }
         debounce.start();
      }

      

      
    
   
   }
  }

  else if (loopstate == 1) {
#ifdef POT_TEMPO
    // Read the potentiometer for a value between 0 and 255.
    // This will be converted into a delay to be used to control the tempo.
    int pot1 = 20 + (mozziAnalogRead (POT_TEMPO) >> 2);
    if (pot1 != tempo) {
      tempo = pot1;  // Tempo range is 20 to 275.
    }
#endif
  }
  loopstate++;
  if (loopstate > 1) loopstate = 0;
}




/*
void updateControl() {
  ledOff();

  // Only play the note in the pattern if we've met the criteria for
  // the next "tick" happening.
  unsigned long timenow = millis();
  if (timenow >= nexttick) {
    // Start the clock for the next tick.
    // Take the tempo mark as a "beats per minute" measure...
    // So time (in milliseconds) to the next beat is:
    //    1000 * 60 / tempo
    //
    // This assumes that one rhythm position = one beat.
    //
    nexttick = millis() + (unsigned long)(60000/tempo);

    // Now play the next beat in the pattern
    seqstep++;
    if (seqstep >= BEATS){
      seqstep = 0;
      // Flash the LED every cycle
      ledOn();
    }
    for (int d=0; d<DRUMS; d++) {
      if (pattern[seqstep][d] != 0) {
        startDrum(pattern[seqstep][d]);
      }
    }
  }

  // Take it in turns each loop depending on the value of "loopstate" to:
  //    0 - read the first keypad
  //    1 - read the second keypad
  //    2 - read the tempo pot
  //
  if (loopstate == 0) {
    // Read the first keypad
    uint8_t btn1 = readSwitches(POT_KP1);
    if (btn1 != lastbtn1) {
      if (btn1 != 0) {
        // A keypress has been detected.
        // The key values are encoded in the form:
        //    <beat><drum> HEX
        // Where <beat> is from 1 to BEATS
        //   and <drum> is from 1 to DRUMS
        //   in hexadecimal.
        //
        // This means the beat index and drum index
        // can be pulled directly out of the key value.
        //
        uint8_t key = keys[0][btn1-1];
        int drumidx = (key & 0x0F) - 1;
        int beatidx = (key >> 4) - 1;
  
        // So toggle this drum
        if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
      }    
    }
    lastbtn1 = btn1;
  }
  else if (loopstate == 1) {
    // Repeat for the second keypad
    uint8_t btn2 = readSwitches(POT_KP2);
    if (btn2 != lastbtn2) {
      if (btn2 != 0) {
        byte key = keys[1][btn2-1];
        int drumidx = (key & 0x0F) - 1;
        int beatidx = (key >> 4) - 1;

        // So toggle this drum
        if (pattern[beatidx][drumidx] != 0) {
          pattern[beatidx][drumidx] = 0;
        } else {
          pattern[beatidx][drumidx] = drums[drumidx];
        }
      }    
    }
    lastbtn2 = btn2;
  }
  else if (loopstate == 2) {
#ifdef POT_TEMPO
    // Read the potentiometer for a value between 0 and 255.
    // This will be converted into a delay to be used to control the tempo.
    int pot1 = 20 + (mozziAnalogRead (POT_TEMPO) >> 2);
    if (pot1 != tempo) {
      tempo = pot1;  // Tempo range is 20 to 275.
    }
#endif
  }
  loopstate++;
  if (loopstate > 2) loopstate = 0;
}
*/

AudioOutput_t updateAudio(){
  // Need to add together all the sample sources.
  // We down-convert using the scaling factor worked out
  // for our specific sample set from running in "CALIBRATE" mode.
  //
  int16_t d_sample = aBD.next() + aSD.next() + aCH.next() + aOH.next();
  return MonoOutput::fromNBit(OUTPUTSCALING, d_sample);
}

void loop(){
  audioHook();
}
