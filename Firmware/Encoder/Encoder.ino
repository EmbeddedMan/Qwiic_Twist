#include "PinChangeInterrupt.h"

//these pins can not be changed 2/3 are special pins
int encoderPin1 = 2;
int encoderPin2 = 3;
int encoderSwitchPin = 8; //push button switch

volatile byte lastEncoded = 0;
volatile long encoderValue = 0;

long lastencoderValue = 0;

int lastMSB = 0;
int lastLSB = 0;

boolean buttonPressed = false;

void setup() {
  Serial.begin (9600);

  pinMode(encoderPin1, INPUT);
  pinMode(encoderPin2, INPUT);

  pinMode(encoderSwitchPin, INPUT);

  digitalWrite(encoderPin1, HIGH); //turn pullup resistor on
  digitalWrite(encoderPin2, HIGH); //turn pullup resistor on

  digitalWrite(encoderSwitchPin, HIGH); //turn pullup resistor on

  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3)
  attachInterrupt(0, updateEncoder, CHANGE);
  attachInterrupt(1, updateEncoder, CHANGE);

  //Attach interrupt to switch
  attachPCINT(digitalPinToPCINT(encoderSwitchPin), buttonInterrupt, CHANGE);
}

void loop() {
  //Do stuff here
  if (buttonPressed == true) {
    Serial.println("Switch!");
    //button is not being pushed
  } else {
    //button is being pushed
  }

  Serial.println(encoderValue);
  delay(100); //just here to slow down the output, and show it will work even during a delay
}

void buttonInterrupt()
{
  buttonPressed = !buttonPressed;
}

void updateEncoder() {
  byte MSB = digitalRead(encoderPin1); //MSB = most significant bit
  byte LSB = digitalRead(encoderPin2); //LSB = least significant bit

  byte encoded = (MSB << 1) | LSB; //converting the 2 pin value to single number
  byte sum = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;

  lastEncoded = encoded; //store this value for next time
}