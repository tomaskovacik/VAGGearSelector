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

 control is done over transistor switch, so signals must be inverted high state on arduino -> switched transistor ->  pull down signal line
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
#include "VAGGearSelector.h"
#include <Arduino.h>

#define USE_TIMER0
//#define USE_TIMER1
//#define USE_TIMER2
//#define USE_TIMER3
//#define USE_TIMER4
//#define USE_TIMER5

/**
   Constructor
*/
static uint8_t __dataIn[2];
volatile static uint8_t _inpin;
static volatile uint8_t _gearChanged = 0;
volatile static uint16_t counter = 0;

volatile static uint8_t __dataOut[2];
volatile static uint8_t __outPinMode;
volatile static uint8_t __inverted_out=0;
volatile static uint8_t _outpin;
volatile static uint16_t _captime = 0;
volatile static uint16_t _captime_tmp = 0;


VAGGearSelector::VAGGearSelector(uint8_t outpin,uint8_t inpin, uint8_t inverted_out){
	if (outpin != NULL){
		#define _OUTPUT
        	_outpin = outpin;
		__inverted_out = inverted_out;
	}

	if (inpin != NULL){
		#define _INPUT
		_inpin = inpin;
	}
}

/**
   Destructor
*/
VAGGearSelector::~VAGGearSelector(){}

#ifdef USE_TIMER0
void VAGGearSelector::setTimer(void){
  // @ 0.5us ticks
  TCCR0A = 0x00; // Normal port operation, OC0 disconnected
  TCCR0B = 0x00; // Normal port operation, OC0 disconnected
  TCCR0A |= _BV(WGM01); // CTC mode
  TCCR0B |= _BV(CS01);// prescaler = 8 -> 1 timer clock tick is 0.5us long @ 16Mhz
  OCR0A = 100;//run compare rutine every 50us, 0.5x100
  TCNT0 = 0;
  TIMSK0 |= _BV(OCIE0A); // enable output compare interrupt A on timer0
}
#define __TIMERX_COMPA_vect TIMER0_COMPA_vect
#endif

#ifdef USE_TIMER1
void VAGGearSelector::setTimer(void){
  // @ 0.5us ticks
  TCCR1A = 0x00; // Normal port operation, OC0 disconnected
  TCCR1B = 0x00; // Normal port operation, OC0 disconnected
  TCCR1B |= _BV(WGM12); // CTC mode
  TCCR1B |= _BV(CS11);// prescaler = 8 -> 1 timer clock tick is 0.5us long @ 16Mhz
  OCR1A = 100;//run compare rutine every 50us, 0.5x100
  TCNT1 = 0;
  TIMSK1 |= _BV(OCIE1A); // enable output compare interrupt A on timer0
}
#define __TIMERX_COMPA_vect TIMER1_COMPA_vect
#endif

#ifdef USE_TIMER2
void VAGGearSelector::setTimer(void){
  // @ 0.5us ticks
  TCCR2A = 0x00; // Normal port operation, OC0 disconnected
  TCCR2B = 0x00; // Normal port operation, OC0 disconnected
  TCCR2A |= _BV(WGM21); // CTC mode
  TCCR2B |= _BV(CS21);// prescaler = 8 -> 1 timer clock tick is 0.5us long @ 16Mhz
  OCR2A = 100;//run compare rutine every 50us, 0.5x100
  TCNT2 = 0;
  TIMSK2 |= _BV(OCIE2A); // enable output compare interrupt A on timer0
}
#define __TIMERX_COMPA_vect TIMER2_COMPA_vect
#endif

#ifdef USE_TIMER3
void VAGGearSelector::setTimer(void){
  // @ 0.5us ticks
  TCCR3A = 0x00; // Normal port operation, OC0 disconnected
  TCCR3B = 0x00; // Normal port operation, OC0 disconnected
  TCCR3B |= _BV(WGM32); // CTC mode
  TCCR3B |= _BV(CS31);// prescaler = 8 -> 1 timer clock tick is 0.5us long @ 16Mhz
  OCR3A = 100;//run compare rutine every 50us, 0.5x100
  TCNT3 = 0;
  TIMSK3 |= _BV(OCIE3A); // enable output compare interrupt A on timer0
}
#define __TIMERX_COMPA_vect TIMER3_COMPA_vect
#endif

#ifdef USE_TIMER4
void VAGGearSelector::setTimer(void){
  // @ 0.5us ticks
  TCCR4A = 0x00; // Normal port operation, OC0 disconnected
  TCCR4B = 0x00; // Normal port operation, OC0 disconnected
  TCCR4B |= _BV(WGM42); // CTC mode
  TCCR4B |= _BV(CS41);// prescaler = 8 -> 1 timer clock tick is 0.5us long @ 16Mhz
  OCR4A = 100;//run compare rutine every 50us, 0.5x100
  TCNT4 = 0;
  TIMSK4 |= _BV(OCIE4A); // enable output compare interrupt A on timer0
}
#define __TIMERX_COMPA_vect TIMER4_COMPA_vect
#endif

#ifdef USE_TIMER5
void VAGGearSelector::setTimer(void){
  // @ 0.5us ticks
  TCCR5A = 0x00; // Normal port operation, OC0 disconnected
  TCCR5B = 0x00; // Normal port operation, OC0 disconnected
  TCCR5B |= _BV(WGM52); // CTC mode
  TCCR5B |= _BV(CS51);// prescaler = 8 -> 1 timer clock tick is 0.5us long @ 16Mhz
  OCR5A = 100;//run compare rutine every 50us, 0.5x100
  TCNT5 = 0;
  TIMSK5 |= _BV(OCIE5A); // enable output compare interrupt A on timer0
}
#define __TIMERX_COMPA_vect TIMER5_COMPA_vect
#endif

void VAGGearSelector::begin()
{

#ifdef _OUTPUT
	pinMode(_outpin,OUTPUT);
	VAGGearSelector::setOutPin(0);//	digitalWrite(_outpin,LOW);
	counter=0;
#endif

#ifdef _INPUT
	pinMode(_inpin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(_inpin), &VAGGearSelector::pinGoingLow, FALLING);
#endif
	setTimer();
}

ISR(__TIMERX_COMPA_vect)
{
#ifdef _INPUT
    _captime++;
  if (_captime == 10000) {
    _captime = 0;
    attachInterrupt(digitalPinToInterrupt(_inpin), &VAGGearSelector::pinGoingHigh, RISING);
  }
#endif

#ifdef _OUTPUT
if (counter == 0)
{//end of pulse, calculate next step
	if (__outPinMode) { //we were up, so we transmited "gearbox type" info, so next step is to transmit selected gear info
		VAGGearSelector::setOutPin(0);//digitalWrite(_outpin,LOW);
		counter = __dataOut[0] * __dataOut[1] * 20; 
//		Serial.print("Low count: ");Serial.println(counter);
	} else { //pin was set to low so pulse which represent selected gear was sent
		VAGGearSelector::setOutPin(1);//digitalWrite(_outpin,HIGH);
		counter = __dataOut[0] * 20;
//		Serial.print("high count: ");Serial.println(counter);
	}
} else counter --;
#endif
}

#ifdef _INPUT
void VAGGearSelector::pinGoingHigh()
{
	_captime_tmp=(_captime+20)/20; //time in ms?

	_captime = 0; //reset timer

//Serial.print("gear: ");Serial.print(_captime_tmp);Serial.print(" __dataIn[1]: ");Serial.print(__dataIn[1]);Serial.print(" __dataIn[0]: ");Serial.println(__dataIn[0]);

	if (__dataIn[0] != 0) //do we have transmitiontype?
	{
		if (_captime_tmp/__dataIn[0] != __dataIn[1]) //new gear is selected
		{
			__dataIn[1] = _captime_tmp/__dataIn[0];
			_gearChanged=1;
		}
	}
	attachInterrupt(digitalPinToInterrupt(_inpin), &VAGGearSelector::pinGoingLow, FALLING);
}

void VAGGearSelector::pinGoingLow()
{
	_gearChanged=0;
	_captime_tmp = _captime+10;

	_captime=0; // reset timing for low pulse

	if (_captime_tmp < 80){
		__dataIn[0] = 0;
		__dataIn[1] = 0;
	} else if(_captime_tmp < 120){
		__dataIn[0] = 5;
		__dataIn[1] = 0;
	} else if (_captime_tmp < 260){
		__dataIn[0] = 10;
		__dataIn[1] = 0;
	} else if (_captime_tmp < 440){
		__dataIn[0] = 20;
		__dataIn[1] = 0;
	} else {
		__dataIn[0] = 0;
		__dataIn[1] = 0;
	}
//Serial.print("trans type: ");Serial.print(_captime_tmp);Serial.print(" __dataIn[0]: ");Serial.println(__dataIn[0]);
	attachInterrupt(digitalPinToInterrupt(_inpin), &VAGGearSelector::pinGoingHigh, RISING);

}

uint8_t VAGGearSelector::gearChanged(){
	return _gearChanged;
}

uint8_t VAGGearSelector::getGear(){
	return __dataIn[1];
}

uint8_t VAGGearSelector::getTransmitionType(){
	return __dataIn[0];
}

void VAGGearSelector::clearGearChangedFlag(){
	_gearChanged=0;
}
#endif

String VAGGearSelector::decodeSelectedGear(){
#ifdef _OUTPUT
	uint8_t trans = __dataOut[0];
	uint8_t gear = __dataOut[1];
#else
	uint8_t trans = __dataIn[0];
	uint8_t gear = __dataIn[1];
#endif

String output;



	if (gear < 10)
	{
     		output  = "PRND";
		if (trans == 5) output += "432";
		if (trans == 10) output += "321";
		if (trans == 20) output += "L";

	switch(gear){
		case 1:
		{ //[P]
			output.replace("P", "[P]");
		}
		break;
		case 2:
		{//[R]
			output.replace("R", "[R]");
		}
		break;
		case 3:
		{//[N]
          		output.replace("N", "[N]");
                }
                break;
                case 4:
                { //[D]
                        output.replace("D", "[D]");
                }
                break;
                case 5:
                {
			if (trans == 5)
                        output.replace("4", "[4]");
			if (trans == 10)
                        output.replace("3", "[3]");
			if (trans == 20)
                        output.replace("L", "[L]");
                }
                break;
                case 6:
                {
			if (trans == 5)
                        output.replace("3","[3]");
                        if (trans == 10)
                        output.replace("2","[2]");
                }
                break;
                case 7:
                {
                        if (trans == 5)
                        output.replace("2","[2]");
                        if (trans == 10)
                        output.replace("1","[1]");
                }
                break;
                case 9:
                {
                        output = "["+output+"]";
                }
                break;
	}
	} else {
                if (trans == 5) output += "54321";
                if (trans == 10) output += "4321";
                if (trans == 20) output += "7654321";
		switch(gear){
			case 20:
			{
				output = "["+output+"]";
			}
			break;
			case 19:
			{
//				output="";
			}
			break;
        	        case 18:
	                {
				output.replace("1","[1]");
        	        }
	                break;
                        case 17:
                        {
                                output.replace("2","[2]");
                        }
                        break;
                        case 16:
                        {
                                output.replace("3","[3]");
                        }
                        break;
                        case 15:
                        {
                                output.replace("4","[4]");
                        }
                        break;
                        case 14:
                        {
                                output.replace("5","[5]");
                        }
                        break;
                        case 13:
                        {
                                output.replace("6","[6]");
                        }
                        break;
                        case 12:
                        {
                                output.replace("7","[7]");
                        }
                        break;
                        case 11:
			{

			}
			break;
			case 10:
			output = "";
			break;
		}
        }
 
return output;
}

#ifdef _OUTPUT
void VAGGearSelector::setOutPin(uint8_t mode)
{
	__outPinMode=mode;
	if (__inverted_out)
		mode = !mode;
	digitalWrite(_outpin,mode);
}

uint8_t VAGGearSelector::setGear(uint8_t gear)
{
__dataOut[1] = gear;
//Serial.println("selected gear: "+String(gear));
}

uint8_t VAGGearSelector::setTransmitionType(uint8_t type)
{
__dataOut[0] = type;
//Serial.println("selected trans: "+String(type));
}
#endif
