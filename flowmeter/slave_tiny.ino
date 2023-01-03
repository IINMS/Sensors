#include <Wire.h>
#include <Arduino_CRC32.h>
#include <EEPROM.h>
#include <Comparator.h>
#include <avr/wdt.h>

Arduino_CRC32 crc32;

#ifndef MILLIS_USE_TIMERB0
#error "This sketch is written for use with TCB0 as the millis timing source"
#endif


//typedef union
//{
//  float number;
//  uint8_t bytes[4];
//} FLOATUNION_t;


const byte interruptPin1 = PIN_PA1;
const byte interruptPin2 = PIN_PA2;
const byte interruptPin3 = PIN_PC0;
const byte interruptPin4 = PIN_PC1;
const byte interruptPin5 = PIN_PC2;
const byte interruptPin6 = PIN_PC3;

volatile uint8_t buff[30];                                 //buffer to save incoming data from i2c

int lastAliveMillis;          //for printing out on serial port

volatile unsigned long lastmicrosTable[7];
volatile boolean firstPulseTable[7] = {true, true, true, true, true, true, true} ;

volatile unsigned long timing[7] = {0, 0, 0, 0, 0, 0, 0};
volatile double currentFlowTable[7] = {0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00} ;
volatile double totalVolumeTable[7] = {0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000};

uint8_t i2ccommand;                                       //received i2c command from raspberry

volatile double aaEquationTable[7] = {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000};
volatile double bbEquationTable[7] = {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000};
volatile double aKfactorTable[7] = {0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000};
volatile double bKfactorTable[7] = {0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000};

int dipPins[] = {PIN_PA4, PIN_PA5, PIN_PA6, PIN_PB5};       // DIP Switch Pins to determine i2c address
int SLAVE_ADDRESS = 0;

///////////////////////////////// external interrupt from flowmeter ON PORT A /////////////////////////////////////
ISR(PORTA_PORT_vect) {
  byte flags = PORTA.INTFLAGS;
  PORTA.INTFLAGS = flags;   //clear flags
  int var = 1;
  int fP = 1;
  while (var < 4) {
    var = var << 1;
    if (flags & var) {
      unsigned long nowmicros = micros();   //for hertz calculation and zero detection
      timing[fP] = nowmicros - lastmicrosTable[fP];     //time passed since last pulse
      if (!firstPulseTable[fP]) {                      //if it isnt the first pulse continue to calculations
        currentFlowTable[fP] = (aaEquationTable[fP] / timing[fP]) + bbEquationTable[fP];
        totalVolumeTable[fP] += currentFlowTable[fP] * timing[fP] / 60000000;
      } else {
        firstPulseTable[fP] = false;
      }
      lastmicrosTable[fP] = nowmicros;            //for calculation from last flowmeter interrupt
    }
    fP++;
  }
}

///////////////////////////////// external interrupt from flowmeter ON PORT C /////////////////////////////////////
ISR(PORTC_PORT_vect) {
  byte flags = PORTC.INTFLAGS;
  PORTC.INTFLAGS = flags;   //clear flags
  int var = 1;
  int fP = 3;
  while (var < 15) {
    if (flags & var) {
      unsigned long nowmicros = micros();   //for hertz calculation and zero detection
      timing[fP] = nowmicros - lastmicrosTable[fP];     //time passed since last pulse
      if (!firstPulseTable[fP]) {                      //if it isnt the first pulse continue to calculations
        currentFlowTable[fP] = (aaEquationTable[fP] / timing[fP]) + bbEquationTable[fP];
        totalVolumeTable[fP] += currentFlowTable[fP] * timing[fP] / 60000000;
      } else {
        firstPulseTable[fP] = false;
      }
      lastmicrosTable[fP] = nowmicros;            //for calculation from last flowmeter interrupt
    }
    var = var << 1;
    fP++;
  }
}

//////////////////////////////////////////////// SETUP  //////////////////////////////////////////////////
void setup() {

  wdt_enable(0x8);                          //watchdog (8 for 1 second, 9 for 2 seconds, A for 4 seconds)
  Serial.begin(115200);
  Serial.setTimeout(30);                   //if noise is noticed, exit serial.readstring in 30msec

  EEPROM.get(60, totalVolumeTable[1]);              //get total volume stored in eeprom
  EEPROM.get(70, totalVolumeTable[2]);
  EEPROM.get(80, totalVolumeTable[3]);
  EEPROM.get(90, totalVolumeTable[4]);
  EEPROM.get(100, totalVolumeTable[5]);
  EEPROM.get(110, totalVolumeTable[6]);
  for (int i = 1; i <= 6; i++) {
    if (isnan(totalVolumeTable[i]))totalVolumeTable[i] = 0;                   //set total volume to zero on fisrt application of the flowmeter
    Serial.println("Volume retreived from EEPROM = "); Serial.println(totalVolumeTable[i]);
  }
  
  for (int i = 0; i <= 3; i++) {                                      //pins configuration for the dip switch
    pinMode(dipPins[i], INPUT_PULLUP);
  }
  getI2cAddress();
  Wire.begin(SLAVE_ADDRESS);                                            //i2c initialisation
  Wire.onRequest(sendData);
  Wire.onReceive(receiveData);
  Serial.print(F("I2C address is= ")); Serial.println(SLAVE_ADDRESS);

  EEPROM.get(0, aKfactorTable[1]);             //get K factor variables from eeprom on every start up
  EEPROM.get(5, bKfactorTable[1]);
  EEPROM.get(10, aKfactorTable[2]);
  EEPROM.get(15, bKfactorTable[2]);
  EEPROM.get(20, aKfactorTable[3]);
  EEPROM.get(25, bKfactorTable[3]);
  EEPROM.get(30, aKfactorTable[4]);
  EEPROM.get(35, bKfactorTable[4]);
  EEPROM.get(40, aKfactorTable[5]);
  EEPROM.get(45, bKfactorTable[5]);
  EEPROM.get(50, aKfactorTable[6]);
  EEPROM.get(55, bKfactorTable[6]);
  Serial.println(F("equation y=ax+b "));
  for (int i = 1; i <= 6; i++) {
    Serial.print(F("a=")); Serial.print(aKfactorTable[i], 4); Serial.print(F(", b=")); Serial.println(bKfactorTable[i], 4);
  }

  for (int i = 1; i <= 6; i++) {
    if (isnan(aKfactorTable[i])) {
      Serial.println(F("Setting defaults for a and b"));          //set default value to K factor variables if eeprom is empty
      aKfactorTable[i] = 0.1333;
      bKfactorTable[i] = 0.7;
    }
    aaEquationTable[i] = aKfactorTable[i] * 1000000;                     //converting A and B values to reduce calculations in flowmeters interrrupt
    bbEquationTable[i] = bKfactorTable[i];
  }

  Comparator.input_p = in_p::in0;       // Use positive input 1 (PB4) for power failure
  Comparator.input_n = in_n::vref;      // 0-series has no DACREF, so use vref directly.
  Comparator.reference = ref::vref_1v5; // Set the DACREF voltage to 1.5V
  Comparator.hysteresis = hyst::large;  // Use a 50mV hysteresis
  Comparator.output = out::disable;     // Use interrupt trigger instead of output pin
  Comparator.init();
  Comparator.start();
  delay(100);                   //delay is required to eliminate comparator interrupt
  AC0.STATUS = 1;               //clear comparator interrupt flag
  Comparator.attachInterrupt(interruptFunction, FALLING);       //comparator attach does work before comparator init()

  pinMode(interruptPin1, INPUT_PULLUP);          //flowmeter interrupt pin
  pinMode(interruptPin2, INPUT_PULLUP);          //flowmeter interrupt pin
  pinMode(interruptPin3, INPUT_PULLUP);          //flowmeter interrupt pin
  pinMode(interruptPin4, INPUT_PULLUP);          //flowmeter interrupt pin
  pinMode(interruptPin5, INPUT_PULLUP);          //flowmeter interrupt pin
  pinMode(interruptPin6, INPUT_PULLUP);          //flowmeter interrupt pin
  PORTA.PIN1CTRL = 0b00001010;                   // PULLUPEN=1(bit4), ISC=2 trigger rising(bit2)
  PORTA.PIN2CTRL = 0b00001010;                   // PULLUPEN=1(bit4), ISC=2 trigger rising(bit2)
  PORTC.PIN0CTRL = 0b00001010;                   // PULLUPEN=1(bit4), ISC=2 trigger rising(bit2)
  PORTC.PIN1CTRL = 0b00001010;                   // PULLUPEN=1(bit4), ISC=2 trigger rising(bit2)
  PORTC.PIN2CTRL = 0b00001010;                   // PULLUPEN=1(bit4), ISC=2 trigger rising(bit2)
  PORTC.PIN3CTRL = 0b00001010;                   // PULLUPEN=1(bit4), ISC=2 trigger rising(bit2)
}

/////////////////////////////////// i2c addressing of slave  ///////////////////////////////////////////////
void getI2cAddress() {
  SLAVE_ADDRESS = 0;
  for (int i = 0; i <= 3; i++) {
    SLAVE_ADDRESS = (SLAVE_ADDRESS << 1) | digitalRead(dipPins[i]);    // read each input pin to calculate the address value
  }
  SLAVE_ADDRESS = 15-SLAVE_ADDRESS;
  Wire.begin(SLAVE_ADDRESS);
}

/////////////////////////////////// comparator interrupt, triggered at low voltage input  ///////////////////////////////////////////////
void interruptFunction() {
  EEPROM.put(60, totalVolumeTable[1]);              //save total volume to eeprom
  EEPROM.put(70, totalVolumeTable[2]);
  EEPROM.put(80, totalVolumeTable[3]);
  EEPROM.put(90, totalVolumeTable[4]);
  EEPROM.put(100, totalVolumeTable[5]);
  EEPROM.put(110, totalVolumeTable[6]);
  Serial.println("Volume saved to EEPROM");
  while (1) {
    Serial.println("waiting for reset");          //endless loop can only exit on WDT timeout
  }
}

//////////////////////////////////////////////// Main loop  ////////////////////////////////////////////
void loop() {
  wdt_reset();                                      //watchdog reset timer
  if (Serial.available() > 0) {                           //serial loop for entering K factor data
    String incomingChar = Serial.readString();
    incomingChar.trim();
    if (incomingChar.substring(0) == "change") {            //key word for entering data is "change", to avoid random noise
      int fM;
      Serial.println(F("Select flow meter [1-6]:"));
      wdt_enable(0xB);                                      //set wdt to 8 seconds enough time to enter data
      while (Serial.available() == 0) {
        fM = Serial.parseInt();
      }
      Serial.println(F("Give a:"));
      wdt_reset();
      while (Serial.available() > 0) {      //this while is needed to clear the input buffer. Flush doesn't seem to be working
        char t = Serial.read();
      }
      while (Serial.available() == 0) {
        aKfactorTable[fM] = Serial.parseFloat();
      }
      Serial.println(("Give b:"));
      wdt_reset();
      while (Serial.available() > 0) {      //this while is needed to clear the input buffer. Flush doesn't seem to be working
        char t = Serial.read();
      }
      wdt_reset();
      while (Serial.available() == 0) {
        bKfactorTable[fM] = Serial.parseFloat();
      }
      switch (fM)
      {
        case 1:
          EEPROM.put(0, aKfactorTable[fM]); EEPROM.put(5, bKfactorTable[fM]); break;
        case 2:
          EEPROM.put(10, aKfactorTable[fM]); EEPROM.put(15, bKfactorTable[fM]); break;
        case 3:
          EEPROM.put(20, aKfactorTable[fM]); EEPROM.put(25, bKfactorTable[fM]); break;
        case 4:
          EEPROM.put(30, aKfactorTable[fM]); EEPROM.put(35, bKfactorTable[fM]); break;
        case 5:
          EEPROM.put(40, aKfactorTable[fM]); EEPROM.put(45, bKfactorTable[fM]); break;
        case 6:
          EEPROM.put(50, aKfactorTable[fM]); EEPROM.put(55, bKfactorTable[fM]); break;
        default:
          break;
      }
      Serial.print(F("Equation is y=")); Serial.print(aKfactorTable[fM], 4); Serial.print(F("*x+")); Serial.println(bKfactorTable[fM], 4);
      aaEquationTable[fM] = aKfactorTable[fM] * 1000000;
      bbEquationTable[fM] = bKfactorTable[fM];
      while (Serial.available() > 0) {      //this while is needed to clear the input buffer. Flush doesn't seem to be working
        char t = Serial.read();
      }
    }
    else {
      Serial.println(F("non valid command"));
    }
    wdt_enable(0x8); //return to normal value after serial inputs
  }

  for (int i = 1; i <= 6; i++) {
    unsigned int long zeroDetector = micros() - lastmicrosTable[i];
    if ((!firstPulseTable[i]) && (zeroDetector > 4000000)) {         //to ignore first pulse after 4 seconds
      firstPulseTable[i] = true;
    }
  }

  for (int i = 1; i <= 6; i++) {
    unsigned int long zeroHertz = micros() - lastmicrosTable[i];
    if (zeroHertz > 4000000) {
      currentFlowTable[i] = 0.0;         //return hertz to zero, 4 second after last pulse
      zeroHertz = micros();
    }
  }

  int alive = millis() - lastAliveMillis;         //just to print out a message on serial port
  if (alive > 2000) {
//    getI2cAddress();
//    Wire.begin(SLAVE_ADDRESS);  
    Serial.print("Flow meter is alive on "); Serial.print(SLAVE_ADDRESS); Serial.print(" i2c address. Time="); Serial.println(millis());
    for (int i = 1; i <= 6; i++) {
      Serial.print("  Total volume=");
      Serial.print(totalVolumeTable[i], 2); Serial.print("  Flow="); Serial.println(currentFlowTable[i], 2);
    }
    lastAliveMillis = millis();
  }
}


//////////////////////////////////////////////// Send MONO Volume data over i2c  ////////////////////////////////////////////
void sendMonoVolData(int x) {
  int meter = x;
  if (totalVolumeTable[meter] >= 1000000)totalVolumeTable[meter] -= 1000000;
  uint32_t const crc32_resTable = crc32.calc((uint8_t *)&totalVolumeTable[meter], sizeof(totalVolumeTable[meter]));   //get the crc32 of the volume char value
  Wire.write((uint8_t *)&totalVolumeTable[meter], sizeof(totalVolumeTable[meter]));      //send total volume over i2c
  Wire.write((uint8_t *)&crc32_resTable, sizeof(crc32_resTable));
}

//////////////////////////////////////////////// Send MONO Flow data over i2c  ////////////////////////////////////////////
void sendMonoFlowData(int x) {
  int meter = x;
  uint32_t const crc32_resTable = crc32.calc((uint8_t *)&currentFlowTable[meter], sizeof(currentFlowTable[meter]));   //get the crc32 of the volume char value
  Wire.write((uint8_t *)&currentFlowTable[meter], sizeof(currentFlowTable[meter]));      //send total volume over i2c
  Wire.write((uint8_t *)&crc32_resTable, sizeof(crc32_resTable));
}


//////////////////////////////////////////////// Send data over i2c  ////////////////////////////////////////////
void sendData() {             //send data to raspberry pi after the request



  if (i2ccommand == 130) {      //total volume request for all meters
    for (int i = 1; i <= 6; i++) {
      if (totalVolumeTable[i] >= 1000000)totalVolumeTable[i] -= 1000000;
    }
    uint32_t const crc32_resTable = crc32.calc((uint8_t *)&totalVolumeTable[1], 24);   //get the crc32 of all the meters (24 bytes) volume char value
    Wire.write((uint8_t *)&totalVolumeTable[1], sizeof(totalVolumeTable[1]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[2], sizeof(totalVolumeTable[2]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[3], sizeof(totalVolumeTable[3]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[4], sizeof(totalVolumeTable[4]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[5], sizeof(totalVolumeTable[5]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[6], sizeof(totalVolumeTable[6]));      //send total volume over i2c
    Wire.write((uint8_t *)&crc32_resTable, sizeof(crc32_resTable));
  }

  if (i2ccommand == 131) {      //total volume request for meter 1
    int meter = 1;
    sendMonoVolData(meter);
  }

  if (i2ccommand == 132) {      //total volume request for meter 2
    int meter = 2;
    sendMonoVolData(meter);
  }

  if (i2ccommand == 133) {      //total volume request for meter 3
    int meter = 3;
    sendMonoVolData(meter);
  }

  if (i2ccommand == 134) {      //total volume request for meter 4
    int meter = 4;
    sendMonoVolData(meter);
  }

  if (i2ccommand == 135) {      //total volume request for meter 5
    int meter = 5;
    sendMonoVolData(meter);
  }

  if (i2ccommand == 136) {      //total volume request for meter 6
    int meter = 6;
    sendMonoVolData(meter);
  }

  if (i2ccommand == 50) {      //current flow for all 6 meters
    uint32_t const crc32_resTable = crc32.calc((uint8_t *)&currentFlowTable[1], 24);   //get the crc32 of all the meters (24 bytes) volume char value
    Wire.write((uint8_t *)&currentFlowTable[1], sizeof(currentFlowTable[1]));      //send total volume over i2c
    Wire.write((uint8_t *)&currentFlowTable[2], sizeof(currentFlowTable[2]));      //send total volume over i2c
    Wire.write((uint8_t *)&currentFlowTable[3], sizeof(currentFlowTable[3]));      //send total volume over i2c
    Wire.write((uint8_t *)&currentFlowTable[4], sizeof(currentFlowTable[4]));      //send total volume over i2c
    Wire.write((uint8_t *)&currentFlowTable[5], sizeof(currentFlowTable[5]));      //send total volume over i2c
    Wire.write((uint8_t *)&currentFlowTable[6], sizeof(currentFlowTable[6]));      //send total volume over i2c
    Wire.write((uint8_t *)&crc32_resTable, sizeof(crc32_resTable));
  }

  if (i2ccommand == 51) {      //flow request for meter 1
    int meter = 1;
    sendMonoFlowData(meter);
  }

  if (i2ccommand == 52) {      //flow request for meter 1
    int meter = 2;
    sendMonoFlowData(meter);
  }

  if (i2ccommand == 53) {      //flow request for meter 1
    int meter = 3;
    sendMonoFlowData(meter);
  }

  if (i2ccommand == 54) {      //flow request for meter 1
    int meter = 4;
    sendMonoFlowData(meter);
  }
  if (i2ccommand == 55) {      //flow request for meter 1
    int meter = 5;
    sendMonoFlowData(meter);
  }
  if (i2ccommand == 56) {      //flow request for meter 1
    int meter = 6;
    sendMonoFlowData(meter);
  }
  if (i2ccommand == 40) {                     //response to Kfactor request from MEGA2560
    uint32_t const crc32_resTable = crc32.calc((uint8_t *)&aKfactorTable[1], 4);   //get the crc32 of all kfactors
    uint32_t const crc32_resTable1 = crc32.calc((uint8_t *)&bKfactorTable[1], 4);
    uint32_t const crc32_resTable2 = crc32.calc((uint8_t *)&aKfactorTable[2], 4);
    uint32_t const crc32_resTable3 = crc32.calc((uint8_t *)&bKfactorTable[2], 4);
    uint32_t const crc32_resTable4 = crc32.calc((uint8_t *)&aKfactorTable[3], 4);
    uint32_t const crc32_resTable5 = crc32.calc((uint8_t *)&bKfactorTable[3], 4);
    Wire.write((uint8_t *)&aKfactorTable[1], sizeof(aKfactorTable[1]));
    Wire.write((uint8_t *)&bKfactorTable[1], sizeof(bKfactorTable[1]));
    Wire.write((uint8_t *)&aKfactorTable[2], sizeof(aKfactorTable[2]));
    Wire.write((uint8_t *)&bKfactorTable[2], sizeof(bKfactorTable[2]));
    Wire.write((uint8_t *)&aKfactorTable[3], sizeof(aKfactorTable[3]));
    Wire.write((uint8_t *)&bKfactorTable[3], sizeof(bKfactorTable[3]));
    Wire.write((uint8_t *)&crc32_resTable, sizeof(crc32_resTable));
    Wire.write((uint8_t *)&crc32_resTable1, sizeof(crc32_resTable1));
    Wire.write((uint8_t *)&crc32_resTable2, sizeof(crc32_resTable2));
    Wire.write((uint8_t *)&crc32_resTable3, sizeof(crc32_resTable3));
    Wire.write((uint8_t *)&crc32_resTable4, sizeof(crc32_resTable4));
    Wire.write((uint8_t *)&crc32_resTable5, sizeof(crc32_resTable5));

    //    FLOATUNION_t myFloat;
    //    myFloat.number = aKfactorTable[1]; // Assign a number to the float
    //    for (int i = 0; i < 4; i++)
    //    {
    //      Serial.print(myFloat.bytes[i], HEX); // Print the hex representation of the float
    //      Serial.print(' ');
    //    }
    //    Serial.println(crc32_resTable, HEX);
    //    FLOATUNION_t myFloat1;
    //    myFloat1.number = bKfactorTable[1]; // Assign a number to the float
    //    for (int i = 0; i < 4; i++)
    //    {
    //      Serial.print(myFloat1.bytes[i], HEX); // Print the hex representation of the float
    //      Serial.print(' ');
    //    }
    //    Serial.println(crc32_resTable1, HEX);
    //    FLOATUNION_t myFloat2;
    //    myFloat2.number = aKfactorTable[2]; // Assign a number to the float
    //    for (int i = 0; i < 4; i++)
    //    {
    //      Serial.print(myFloat2.bytes[i], HEX); // Print the hex representation of the float
    //      Serial.print(' ');
    //    }
    //    Serial.println(crc32_resTable2, HEX);
    //    FLOATUNION_t myFloat3;
    //    myFloat3.number = bKfactorTable[2]; // Assign a number to the float
    //    for (int i = 0; i < 4; i++)
    //    {
    //      Serial.print(myFloat3.bytes[i], HEX); // Print the hex representation of the float
    //      Serial.print(' ');
    //    }
    //    Serial.println(crc32_resTable3, HEX);


  }
  if (i2ccommand == 41) {                     //response to the rest of Kfactor from MEGA2560
    uint32_t const crc32_resTable6 = crc32.calc((uint8_t *)&aKfactorTable[4], 4);   //get the crc32 of all kfactors
    uint32_t const crc32_resTable7 = crc32.calc((uint8_t *)&bKfactorTable[4], 4);
    uint32_t const crc32_resTable8 = crc32.calc((uint8_t *)&aKfactorTable[5], 4);
    uint32_t const crc32_resTable9 = crc32.calc((uint8_t *)&bKfactorTable[5], 4);
    uint32_t const crc32_resTable10 = crc32.calc((uint8_t *)&aKfactorTable[6], 4);
    uint32_t const crc32_resTable11 = crc32.calc((uint8_t *)&bKfactorTable[6], 4);
    Wire.write((uint8_t *)&aKfactorTable[4], sizeof(aKfactorTable[4]));
    Wire.write((uint8_t *)&bKfactorTable[4], sizeof(bKfactorTable[4]));
    Wire.write((uint8_t *)&aKfactorTable[5], sizeof(aKfactorTable[5]));
    Wire.write((uint8_t *)&bKfactorTable[5], sizeof(bKfactorTable[5]));
    Wire.write((uint8_t *)&aKfactorTable[6], sizeof(aKfactorTable[6]));
    Wire.write((uint8_t *)&bKfactorTable[6], sizeof(bKfactorTable[6]));
    Wire.write((uint8_t *)&crc32_resTable6, sizeof(crc32_resTable6));
    Wire.write((uint8_t *)&crc32_resTable7, sizeof(crc32_resTable7));
    Wire.write((uint8_t *)&crc32_resTable8, sizeof(crc32_resTable8));
    Wire.write((uint8_t *)&crc32_resTable9, sizeof(crc32_resTable9));
    Wire.write((uint8_t *)&crc32_resTable10, sizeof(crc32_resTable10));
    Wire.write((uint8_t *)&crc32_resTable11, sizeof(crc32_resTable11));
  }

  if (i2ccommand == 30) {                     //ZERO TOTALVOLUME
    for (int i = 1; i <= 6; i++) {
      totalVolumeTable[i] = 0;
    }
    uint32_t const crc32_resTable = crc32.calc((uint8_t *)&totalVolumeTable[1], 24);   //get the crc32 of all the meters (24 bytes) volume char value
    Wire.write((uint8_t *)&totalVolumeTable[1], sizeof(totalVolumeTable[1]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[2], sizeof(totalVolumeTable[2]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[3], sizeof(totalVolumeTable[3]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[4], sizeof(totalVolumeTable[4]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[5], sizeof(totalVolumeTable[5]));      //send total volume over i2c
    Wire.write((uint8_t *)&totalVolumeTable[6], sizeof(totalVolumeTable[6]));      //send total volume over i2c
    Wire.write((uint8_t *)&crc32_resTable, sizeof(crc32_resTable));
    EEPROM.put(60, totalVolumeTable[1]);              //save total volume to eeprom
    //      EEPROM.put(65, previousTotalVolumeTable[1]);
    EEPROM.put(70, totalVolumeTable[2]);
    //      EEPROM.put(75, previousTotalVolumeTable[2]);
    EEPROM.put(80, totalVolumeTable[3]);
    //      EEPROM.put(85, previousTotalVolumeTable[3]);
    EEPROM.put(90, totalVolumeTable[4]);
    //      EEPROM.put(95, previousTotalVolumeTable[4]);
    EEPROM.put(100, totalVolumeTable[5]);
    //      EEPROM.put(105, previousTotalVolumeTable[5]);
    EEPROM.put(110, totalVolumeTable[6]);
    //      EEPROM.put(115, previousTotalVolumeTable[6]);
    Serial.println("zeroed all meters");
  }
  i2ccommand = 0;
}

void receiveData () {
  int j;
  Serial.print("d=");
  while (1 <= Wire.available()) {                  //read incoming bytes from i2c
    buff[j] = Wire.read();
    Serial.print(buff[j], HEX); Serial.print(" ");
    j++;
  }
  i2ccommand = *(uint8_t *)buff;      //command recieved
  
  Serial.println("");
  Serial.print("i2ccommand="); Serial.print(i2ccommand); Serial.println("#OK");

  if (i2ccommand == 25) {           //command to read incoming Kfactor data
    buff[0] = 0;
    float a, b;
    uint32_t flowmeterSelected;
    flowmeterSelected =  *(uint32_t *)&buff[1];
    uint32_t flowmeterSelectedCRC = *(uint32_t *)&buff[5];
    a = *(double *)&buff[9];
    uint32_t acrc = *(uint32_t *)&buff[13];
    b = *(double *)&buff[17];
    uint32_t bcrc = *(uint32_t *)&buff[21];
    uint32_t const flmcrc32_res = crc32.calc((uint8_t *)&flowmeterSelected, sizeof(flowmeterSelected));
    uint32_t const Acrc32_res = crc32.calc((uint8_t *)&a, sizeof(a));       //calculation for crc data
    uint32_t const Bcrc32_res = crc32.calc((uint8_t *)&b, sizeof(b));  
    if ((Acrc32_res == acrc) && (Bcrc32_res == bcrc) && (flmcrc32_res == flowmeterSelectedCRC)) {
      Serial.println("crc for K data recieved is OK");
      if (flowmeterSelected == 1) {
        EEPROM.put(0, a); EEPROM.put(5, b); Serial.println("saved kfactor flowmeter 1");
      }
      if (flowmeterSelected == 2) {
        EEPROM.put(10, a); EEPROM.put(15, b); Serial.println("saved kfactor flowmeter 2");
      }
      if (flowmeterSelected == 3) {
        EEPROM.put(20, a); EEPROM.put(25, b); Serial.println("saved kfactor flowmeter 3");
      }
      if (flowmeterSelected == 4) {
        EEPROM.put(30, a); EEPROM.put(35, b); Serial.println("saved kfactor flowmeter 4");
      }
      if (flowmeterSelected == 5) {
        EEPROM.put(40, a); EEPROM.put(45, b); Serial.println("saved kfactor flowmeter 5");
      }
      if (flowmeterSelected == 6) {
        EEPROM.put(50, a); EEPROM.put(55, b); Serial.println("saved kfactor flowmeter 6");
      }
      aKfactorTable[flowmeterSelected] = a;
      bKfactorTable[flowmeterSelected] = b;
      aaEquationTable[flowmeterSelected] = a * 1000000;
      bbEquationTable[flowmeterSelected] = b;
      Serial.print(F("Equation received OK. flowmeter = ")); Serial.print(flowmeterSelected); Serial.println(" ");
      Serial.print(F(" Flow=(")); Serial.print(a, 4); Serial.print(F(" * HERTZ) + ")); Serial.println(b, 4);
    }else{
      Serial.println("crc for K data recieved is NOT valid");
    }
  }
}
