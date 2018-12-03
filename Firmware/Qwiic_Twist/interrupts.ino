//Called every time the encoder is twisted
//Based on http://bildr.org/2012/08/rotary-encoder-arduino/
//Adam - I owe you many beers for bildr
void updateEncoder() {
  byte MSB = digitalRead(encoderAPin); //MSB = most significant bit
  byte LSB = digitalRead(encoderBPin); //LSB = least significant bit

  byte encoded = (MSB << 1) | LSB; //Convert the 2 pin value to single number
  lastEncoded = (lastEncoded << 2) | encoded; //Add this to the previous readings

  //A complete indent occurs across four interrupts and looks like:
  //ABAB.ABAB = 01001011 for CW
  //ABAB.ABAB = 10000111 for CCW

  //lastEncoded could be many things but by looking for two unique values
  //we filter out corrupted and partially dropped encoder readings
  //Gaurantees we will not get partial indent readings

  if (lastEncoded == 0b01001011) //One indent clockwise
  {
    registerMap.encoderCount++;
    registerMap.encoderDifference++;

    //If the user has enabled connection between a color and the knob, change LED brightness here
    if (registerMap.ledConnectRed != 0)
    {
      int newRed = registerMap.ledBrightnessRed + registerMap.ledConnectRed; //May go negative if setting is <0
      if (newRed > 255) newRed = 255; //Cap at max
      if (newRed < 0) newRed = 0; //Cap at min
      registerMap.ledBrightnessRed = newRed;

      analogWrite(ledRedPin, 255 - registerMap.ledBrightnessRed);
    }
    if (registerMap.ledConnectGreen != 0)
    {
      int newGreen = registerMap.ledBrightnessGreen + registerMap.ledConnectGreen; //May go negative if setting is <0
      if (newGreen > 255) newGreen = 255; //Cap at max
      if (newGreen < 0) newGreen = 0; //Cap at min
      registerMap.ledBrightnessGreen = newGreen;

      analogWrite(ledGreenPin, 255 - registerMap.ledBrightnessGreen);
    }
    if (registerMap.ledConnectBlue != 0)
    {
      int newBlue = registerMap.ledBrightnessBlue + registerMap.ledConnectBlue; //May go negative if setting is <0
      if (newBlue > 255) newBlue = 255; //Cap at max
      if (newBlue < 0) newBlue = 0; //Cap at min
      registerMap.ledBrightnessBlue = newBlue;

      analogWrite(ledBluePin, 255 - registerMap.ledBrightnessBlue);
    }
  }
  else if (lastEncoded == 0b10000111) //One indent counter clockwise
  {
    registerMap.encoderCount--;
    registerMap.encoderDifference--;

    //If the user has enabled connection between a color and the knob, change LED brightness here
    if (registerMap.ledConnectRed != 0)
    {
      int newRed = registerMap.ledBrightnessRed - registerMap.ledConnectRed; //May increase if setting is <0
      if (newRed > 255) newRed = 255; //Cap at max
      if (newRed < 0) newRed = 0; //Cap at min
      registerMap.ledBrightnessRed = newRed;

      analogWrite(ledRedPin, 255 - registerMap.ledBrightnessRed);
    }
    if (registerMap.ledConnectGreen != 0)
    {
      int newGreen = registerMap.ledBrightnessGreen - registerMap.ledConnectGreen; //May increase if setting is <0
      if (newGreen > 255) newGreen = 255; //Cap at max
      if (newGreen < 0) newGreen = 0; //Cap at min
      registerMap.ledBrightnessGreen = newGreen;

      analogWrite(ledGreenPin, 255 - registerMap.ledBrightnessGreen);
    }
    if (registerMap.ledConnectBlue != 0)
    {
      int newBlue = registerMap.ledBrightnessBlue - registerMap.ledConnectBlue; //May increase if setting is <0
      if (newBlue > 255) newBlue = 255; //Cap at max
      if (newBlue < 0) newBlue = 0; //Cap at min
      registerMap.ledBrightnessBlue = newBlue;

      analogWrite(ledBluePin, 255 - registerMap.ledBrightnessBlue);
    }
  }

  lastEncoderTwistTime = millis(); //Timestamp this event

  registerMap.status |= (1 << statusEncoderMovedBit); //Set the status bit to true to indicate movement
}

//Turn on interrupts for the various pins
void setupInterrupts()
{
  //Call updateEncoder() when any high/low changed seen on interrupt 0 (pin 2), or interrupt 1 (pin 3)
#if defined(__AVR_ATmega328P__)
  attachInterrupt(0, updateEncoder, CHANGE);
  attachInterrupt(1, updateEncoder, CHANGE);
#endif

#if defined(__AVR_ATtiny84__)
  //To make this line work you have to comment out https://github.com/NicoHood/PinChangeInterrupt/blob/master/src/PinChangeInterruptSettings.h#L228
  attachPCINT(digitalPinToPCINT(encoderAPin), updateEncoder, CHANGE); //Doesn't work

  attachPCINT(digitalPinToPCINT(encoderBPin), updateEncoder, CHANGE);
#endif

  //Attach interrupt to switch
  attachPCINT(digitalPinToPCINT(switchPin), buttonInterrupt, CHANGE);
}

//When Twist receives data bytes from Master, this function is called as an interrupt
void receiveEvent(int numberOfBytesReceived)
{
  registerNumber = Wire.read(); //Get the memory map offset from the user

  //Begin recording the following incoming bytes to the temp memory map
  //starting at the registerNumber (the first byte received)
  byte x = 0;
  while (Wire.available())
  {
    byte temp = Wire.read(); //We might record it, we might throw it away

    if ( (x + registerNumber) < (sizeof(memoryMap) - 1) )
    {
      //Clense the incoming byte against the read only protected bits
      //Store the result into the register map

      //*(registerPointer + registerNumber + x) &= ~*(protectionPointer + registerNumber + x); //Clear this register if needed
      //*(registerPointer + registerNumber + x) |= temp & *(protectionPointer + registerNumber + x); //Or in the user's request (clensed against protection bits)
      
      *(registerPointer + registerNumber + x) = temp; //No protection

      x++;
    }
  }

  updateOutputs = true; //Update things like LED brightnesses in the main loop
}

//Respond to GET commands
//When Twist gets a request for data from the user, this function is called as an interrupt
//The interrupt will respond with bytes starting from the last byte the user sent to us
//While we are sending bytes we may have to do some calculations
void requestEvent()
{
  //Calculate time stamps before we start sending bytes via I2C
  if(lastEncoderTwistTime > 0) registerMap.timeSinceLastMovement = millis() - lastEncoderTwistTime;
  if(lastButtonTime > 0) registerMap.timeSinceLastButton = millis() - lastButtonTime;

  //This will write the entire contents of the register map struct starting from
  //the register the user requested, and when it reaches the end the master
  //will read 0xFFs.
  Wire.write((registerPointer + registerNumber), sizeof(memoryMap) - registerNumber);

  //Clear the interrupt pin once user has requested something
  interruptIndicated = false;
  pinMode(interruptPin, INPUT); //Go to high impedance
}

//Called any time the pin changes state
void buttonInterrupt()
{
  registerMap.status ^= (1 << statusButtonPressedBit); //Toggle the status bit to indicate button interaction

  if ( (registerMap.status & (1 << statusButtonPressedBit)) == 0) //User has released the button, we have completed a click cycle
  {
    registerMap.status |= (1 << statusButtonClickedBit); //Set the clicked bit
    lastButtonTime = millis();
  }

  //Check if button interrupt is enabled
  if ( (registerMap.interruptEnable & (1 << enableInterruptButtonBit) ) )
  {
    indicateInterrupt();
  }
}
