/*
  (C) Tomas Kovacik
  https://github.com/tomaskovacik/
  GNU GPL3

arduino library for reading and writing gear selector display protocol on  V.A.G. cars (gear selector display, is display on cluster, which indicate enganged gear on automatic transmition, text on bootom side of displa: "PRND321")

this library use timer, by uncomenting one of lines "#define USE_TIMER0-5" you can select which one you want to use"
input pit has to have hardware interrupt (INTx), output pin did not have any limitaions, only that it has to be output capable of corse

====================== reading example:
#include <VAGGearSelector.h>
#define INPIN 2

VAGGearSelector gearSelector(NULL,INPIN);

void setup() {
  gearSelector.begin();
Serial.begin(115200); 
}

void loop() {
  if (gearSelector.gearChanged()){
     //Serial.print("trans type: ");Serial.println(gearSelector.getTransmitionType());
     //Serial.print("trans gear: ");Serial.println(gearSelector.getGear());
     Serial.println(gearSelector.decodeSelectedGear());
     gearSelector.clearGearChangedFlag();
  }
}
=====================

=====================  writing example on diferent device of corse
#include <VAGGearSelector.h>
  
VAGGearSelector gearSelector(13,NULL,1);

uint8_t transType=5; // start with 5speed trans, 
int8_t i=1; //and selected parking mode

void setup() {
Serial.begin(115200);
gearSelector.begin();
gearSelector.setGear(i); // set gear
gearSelector.setTransmitionType(transType);// set transmition type
}

void loop() {
  if (Serial.available()){ 
   char c = Serial.read();
   if (c == '+') i++; // type + for next gear
   if (c == '-') i--; // type - for previous gear
   if (i < 1) i=20;
   if (i > 20) i=1; 
   if (c == ',') transType+=5; // type , for next transmition type
   if (c == '.') transType-=5; // type . for previous transmition type
   if (transType < 5 ) transType=20;
   if (transType > 20 ) transType=5;
   gearSelector.setGear(i); // set gear 
   gearSelector.setTransmitionType(transType); //set transmition type
   Serial.print(gearSelector.decodeSelectedGear()); // display what we have selected
  }
}
===================

protocol overview:

 control is done over transistor switch, so signals must be inverted (high state on arduino -> switched transistor ->  pull down signal line)
 high (low in this sketch) pusle is identification of transmition, for this inverted output need to be enabled by seting
 3th parameter to be TRUE:

 VAGGearSelector gearSelector(13,NULL,1);

 type of trans. High pulse in miliseconds
 5  -> PRND432
 10 -> PRND321
 20 -> PRNDL

 low pusle is multiply of lenght of identification pulse (5,10,20) and selected gear/mode
 
 for example:
 [P]RND432 -> 5ms HIGH -> 5*1ms LOW
 P[R]ND432 -> 5ms HIGH -> 5*2ms LOW 
 PR[N]D432 -> 5ms HIGH -> 5*3ms LOW
 PRN[D]432 -> 5ms HIGH -> 5*4ms LOW
 PRND[4]32 -> 5ms HIGH -> 5*5ms LOW
 PRND4[3]2 -> 5ms HIGH -> 5*6ms LOW
 PRND43[2] -> 5ms HIGH -> 5*7ms LOW
 PRND432 -> 5ms HIGH -> 5*8ms LOW
 [PRND432] -> 5ms HIGH -> 5*9ms LOW - transmition ERROR

 6-7 gear transmition, manual mode:
 [7]654321 - 20ms*12
 7[6]54321 - 20ms*13
 76[5]4321 - 20ms*14
 765[4]321 - 20ms*15
 7654[3]21 - 20ms*16
 76543[2]1 - 20ms*17
 765432[1] - 20ms*18
 7654321   - 20ms*19
 [7654321] - 20ms*20


 output pin of arduino uno is connected to base of NPN transistor (2n5551 in my case) via 1k resistor
 emitor of transistor to ground, in this mode "inverted output is selected, 3th parameter set to 1

 collector on pin to pin 6 of 20pin red connector of audi a4 1998 cluster (or black connector in early models)

                       cluster 
                          |
                          |
                          |         
         ____             |
 D2 ____|1k  |__________|/  NPN
        |____|          |\e
                          |
                          |
                          |
                         _|_ GND

 video: https://www.youtube.com/watch?v=0d18H4fjk1M


*/

#ifndef VAGGearSelector_h
#define VAGGearSelector_h
#include <inttypes.h>
#include <Arduino.h>
#define NO_AUTOMATIC_TRANSMITION 0
#define FIVE_SPEED 5
#define FOUR_SPEED 10
#define SIX_OR_SEVEN_SPEED 20

class VAGGearSelector
{
public:

VAGGearSelector(uint8_t outpin, uint8_t inpin, uint8_t inverted_out = 0);
~VAGGearSelector();
void begin(); //setup port
uint8_t gearChanged();
uint8_t getGear();
uint8_t getTransmitionType();
uint8_t setGear(uint8_t gear);
uint8_t setTransmitionType(uint8_t type);
void clearGearChangedFlag();
static void setOutPin(uint8_t mode);

String decodeSelectedGear();
static void pinGoingHigh();
static void pinGoingLow();

private:
void setTimer(void);
};

#endif
