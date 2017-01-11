/*  
 * ------------------------------------------------------------------------
 * "PH2LB LICENSE" (Revision 1) : (based on "THE BEER-WARE LICENSE" Rev 42) 
 * <lex@ph2lb.nl> wrote this file. As long as you retain this notice
 * you can do modify it as you please. It's Free for non commercial usage 
 * and education and if we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return
 * Lex Bolkesteijn 
 * ------------------------------------------------------------------------ 
 * Filename : BITX40-led-v1.ino  
 * Version  : 0.1 (DRAFT)
 * ------------------------------------------------------------------------
 * Description : A Arduino NANO DDS based VFO for the BITX40 with LED display
 * ------------------------------------------------------------------------
 * Revision : 
 *  - 2017-jan-08 0.1 initial version 
 * ------------------------------------------------------------------------
 * Hardware used : 
 *  - Arduino NANO  
 *  - MAX7219 
 *  - AD9833
 *  - Simpel rotary encoder with switch
 * ------------------------------------------------------------------------
 * Software used :  
 *  - LedControl library from Eberhard Fahle
 * ------------------------------------------------------------------------ 
 * TODO LIST : 
 *  - add more sourcode comment
 *  - add debounce for the keys 
 *  - harden the rotary switch (sometimes strange behaviour due to timing
 *    isues (current no interrupt).
 * ------------------------------------------------------------------------ 
 */
 
  
#include "AD9833.h"    
#include "LedControl.h"  

#define debugSerial Serial

#define debugPrintLn(...) { if (debugSerial) debugSerial.println(__VA_ARGS__); }
#define debugPrint(...) { if (debugSerial) debugSerial.print(__VA_ARGS__); }

 
AD9833 ad9833= AD9833(11, 13, A1); // SPI_MOSI, SPI_CLK, SPI_CS 
LedControl lc=LedControl(11,13,A2,1);  // SPI_MOSI, SPI_CLK, SPI_CS, NR OF DEVICES  NOTE TO MY SELF>> CS is now A1 instead of 12 (12=MISO on UNO) 
 
#define buttonAnalogInput A0   // A0 on the LCD shield (save some pins)

// BITX40 VFO limits (4.8Mhz - 5Mhz)
uint32_t vfofreqlowerlimit = 48e5;
uint32_t vfofrequpperlimit = 5e6;

// for a little debounce (needs to be better then now)
int_fast32_t prevtimepassed = millis(); // int to hold the arduino miilis since startup

// define the band
uint32_t FreqLowerLimit = (uint32_t)7000e3; // lower limit acording to band plan
uint32_t FreqUpperLimit = (uint32_t)7200e3; // upper limit according to bandplan
uint32_t FreqBase = (uint32_t)7000e3; // the base frequency (first full 500Khz block from FreqLowerLimit)
uint32_t Freq = (uint32_t)7100e3 ; // the current frequency on that band (set with default) 
 

// define the stepstruct
typedef struct 
{
  char * Text;
  uint32_t Step;
} 
StepStruct;

// define the step enum
typedef enum 
{
  STEPMIN = 0,
  S10 = 0,
  S25 = 1,
  S100 = 2,
  S250 = 3,
  S1KHZ = 4,
  S2_5KHZ = 5,
  S10KHZ = 6,
  STEPMAX = 6
} 
StepEnum;

// define the bandstruct array
StepStruct  Steps [] =
{
  { (char *)"10Hz", 10 }, 
  { (char *)"25Hz", 25 }, 
  { (char *)"100Hz", 100 }, 
  { (char *)"250Hz", 250 }, 
  { (char *)"1KHz", 1000 }, 
  { (char *)"2.5KHz", 2500 }, 
  { (char *)"10KHz", 10000 }
};

// Switching band stuff
boolean switchBand = false; 
int currentFreqStepIndex = (int)S250; // default 10Hz.

// Encoder stuff
int encoder0PinALast = LOW; 
#define encoderPin1 2
#define encoderPin2 3
#define encoderSW 7
#define disableLed 6 

volatile int lastEncoded = 0;
volatile long encoderValue = 0;
long lastencoderValue = 0;
int lastMSB = 0;
int lastLSB = 0;

int nrOfSteps = 0;

// LCD stuff
boolean updatedisplayfreq = false;
boolean updatedisplaystep = false;

boolean useLed = true;

// Playtime
void setup() 
{ 
  
  debugSerial.begin(57600); 
  debugPrintLn(F("setup"));
  
  pinMode(disableLed, INPUT);
  digitalWrite(disableLed, HIGH); //turn pullup resistor on
  useLed = digitalRead(disableLed);

  debugPrint(F("useled = "));  
  debugPrintLn(useLed);  
  
  SPI.begin();  
  SPI.setDataMode(SPI_MODE2);   
  
  debugPrintLn(F("spi done"));  
 
  lc.init();
  /*
  The MAX72XX is in power-saving mode on startup,
  we have to do a wakeup call
  */
  lc.shutdown(0,false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0,6);
  /* and clear the display */
  lc.clearDisplay(0);  
  
  debugPrintLn(F("led done"));  
  
  writeTextToLed("DDS");
  //delay(1000); 
  ad9833.init();
  ad9833.reset(); 
  writeTextToLed("initdone");
  //delay(1000); 
  updatedisplayfreq = true;
  updatedisplaystep = false;
  
  updateDisplays();
  setFreq();
   
  debugPrintLn(F("start loop")); 
 
  pinMode(encoderPin1, INPUT); 
  pinMode(encoderPin2, INPUT);
  pinMode(encoderSW, INPUT);

  digitalWrite(encoderPin1, HIGH); //turn pullup resistor on
  digitalWrite(encoderPin2, HIGH); //turn pullup resistor on
  digitalWrite(encoderSW, HIGH); //turn pullup resistor on
  
  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3) 
  attachInterrupt(0, updateEncoder, CHANGE); 
  attachInterrupt(1, updateEncoder, CHANGE);
} 


void updateEncoder(){
  int MSB = digitalRead(encoderPin1); //MSB = most significant bit
  int LSB = digitalRead(encoderPin2); //LSB = least significant bit

  int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;

  lastEncoded = encoded; //store this value for next time
}

// function to set the AD9850 to the VFO frequency depending on bandplan.
void setFreq()
{
  // now we know what we want, so scale it back to the 5Mhz - 5.5Mhz freq
  uint32_t frequency = Freq - FreqBase;
  uint32_t ft301freq = (vfofrequpperlimit - vfofreqlowerlimit) - frequency + vfofreqlowerlimit;
 
  ad9833.setFrequency((long)ft301freq); 
}
 
void writeTextToLed(char *p)
{
  if (useLed)
  {
    lc.clearDisplay(0);
    lc.setChar(0, 0, p[7], false);
    lc.setChar(0, 1, p[6], false);
    lc.setChar(0, 2, p[5], false);
    lc.setChar(0, 3, p[4], false);   
    lc.setChar(0, 4, p[3], false);  
    lc.setChar(0, 5, p[2], false);  
    lc.setChar(0, 6, p[1], false);  
    lc.setChar(0, 7, p[0], false);   
    delay ( 00 );
  }
}

void freqToLed( long v)
{
  if (useLed)
  {
     lc.clearDisplay(0);
  
    // Variable value digit 
    int digito1;
     int digito2;
     int digito3;
     int digito4;
     int digito5;
     int digito6;
     int digito7;
     int digito8;
  
    // Calculate the value of each digit 
    digito1 = v %  10 ;
    digito2 = (v / 10 )% 10 ;
    digito3 = (v / 100 )% 10 ;  
    digito4 = (v / 1000 )% 10 ;
    digito5 = (v / 10000 )% 10 ;
    digito6 = (v / 100000 )% 10 ;
    digito7 = (v / 1000000 )% 10 ;
    digito8 = (v / 10000000 )% 10 ;
    
    // Display the value of each digit in the display 
    if (digito8 > 0)
    {
      lc.setDigit ( 0 , 7 , (byte) digito8, false);
    }
    lc.setDigit ( 0 , 6 , (byte) digito7, true);
    lc.setDigit ( 0 , 5 , (byte) digito6, false);
    lc.setDigit ( 0 , 4 , (byte) digito5, false);
    lc.setDigit ( 0 , 3 , (byte) digito4, true);
    lc.setDigit ( 0 , 2 , (byte) digito3, false);
    lc.setDigit ( 0 , 1 , (byte) digito2, false);
    lc.setDigit ( 0 , 0 , (byte) digito1, false);
    delay ( 00 );
  }
} 

void stepToLed( long s)
{
   if (useLed)
    {
    lc.clearDisplay(0);
    lc.setChar(0, 7, 's', false);
    lc.setChar(0, 6, 't', false);
    lc.setChar(0, 5, 'e', false);
    lc.setChar(0, 4, 'p', true);  
     
    // Variable value digit 
    int digito1;
    int digito2;
    int digito3;
    int digito4; 
    boolean is10Kplus = false;
  
    if (s >= 1000)
    {
      s = s / 100;  
      is10Kplus = true;
    }
  
    // Calculate the value of each digit 
    digito1 = s %  10 ;
    digito2 = (s / 10 )% 10 ;
    digito3 = (s / 100 )% 10 ;  
    digito4 = (s / 1000 )% 10 ; 
    
    // Display the value of each digit in the display 
    if (s >= 1000)
    {
      lc.setDigit ( 0 , 3 , (byte) digito4, false);
    }
    if (s >= 100)
    {
      lc.setDigit ( 0 , 2 , (byte) digito3, false);
    }
    // there isn't anything smaller then 10
    lc.setDigit ( 0 , 1 , (byte) digito2, is10Kplus);
    lc.setDigit ( 0 , 0 , (byte) digito1, false);
  }
} 
 

// function to update . . the LCD 
void updateDisplays()
{
  if (updatedisplayfreq)
  {
    float freq = Freq / 1e3; // because we display Khz and the Freq is in Mhz.
 
    freqToLed(Freq); 
  }

  if (updatedisplaystep)
  { 
    stepToLed(Steps[currentFreqStepIndex].Step); 
  } 
} 


boolean ccw = false;
boolean cw = false;
boolean changeStep = false;
boolean prevSw = false;

// the main loop
void loop() 
{  
  boolean updatefreq = false; 
  updatedisplayfreq = false;  
  updatedisplaystep = false;
      
  if ((int)encoderValue / 4 !=0 )
  {
    nrOfSteps = (int)encoderValue / 4;
    encoderValue = 0; 
  } 
  
  boolean sw = digitalRead(encoderSW) == 0;

  if (sw != prevSw)
  {
     prevSw = sw;
     if (sw)
     {
       changeStep = changeStep == 0; // toggle 
       Serial.println(changeStep); 
       if (changeStep)
         updatedisplaystep = true;
       else
         updatedisplayfreq = true;
       delay(250);  // software debounce
     }
  }
    
  int_fast32_t timepassed = millis(); // int to hold the arduino milis since startup
  if ( prevtimepassed + 10 < timepassed  || nrOfSteps != 0)
  {
    prevtimepassed = timepassed; 
    
    if (changeStep)
    {
      int nrOfStepsTmp = nrOfSteps; 
      nrOfSteps = 0;
        
      if (nrOfStepsTmp > 0 && currentFreqStepIndex < STEPMAX)
      {
        currentFreqStepIndex = (StepEnum)currentFreqStepIndex + 1;
        updatedisplaystep = true;
      } 
      else if (nrOfStepsTmp < 0 && currentFreqStepIndex > STEPMIN)
      {
        currentFreqStepIndex = (StepEnum)currentFreqStepIndex - 1;
        updatedisplaystep = true;
      } 
    }
    else
    {
      if (nrOfSteps > 0)
      {
        int nrOfStepsTmp = nrOfSteps; 
        nrOfSteps = 0;
          if (Freq < FreqUpperLimit)
          {
            Freq = Freq + (Steps[currentFreqStepIndex].Step * nrOfStepsTmp);
            updatefreq = true;
            updatedisplayfreq = true;
          }
          if (Freq > FreqUpperLimit)
          {
            Freq = FreqUpperLimit;
          } 
      }
      else if ( nrOfSteps < 0)
      {
        int nrOfStepsTmp = nrOfSteps;
        nrOfSteps = 0;
          if (Freq > FreqLowerLimit)
          {
            Freq = Freq + (Steps[currentFreqStepIndex].Step * nrOfStepsTmp);
            updatefreq = true;
            updatedisplayfreq = true;
          }
          if (Freq < FreqLowerLimit)
          {
            Freq = FreqLowerLimit;
          }  
      } 
    }
  }

  if (updatefreq)
  { 
    setFreq();
  }

  updateDisplays();   
}

