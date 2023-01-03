////// SOS ΜΕΤΑ ΑΠΟ ΑΝΑΒΑΘΜΙΣΗ ΒΙΒΛΙΟΘΗΚΗΣ LMIC ΘΕΛΕΙ ΝΑ ΟΡΙΣΤΕΙ ΠΑΛΙ ΤΟ EU ΣΤΟ ΦΑΚΕΛΟ \Documents\Arduino\libraries\MCCI_LoRaWAN_LMIC_library\project_config
#include <Arduino_CRC32.h>
#include <stdlib.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Wire.h>
#include "LoraBinary.h"
#include "RTClib.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_INA219.h>
#include <avr/wdt.h>

#define tinys 7              //maximun number of tinys that can be connected on i2c bus
#define numberOfVanes 2
#define oneWireBus 12     //thermometer

#define powerIndex 0
#define tempIndex 2
#define pressureIndex 1
#define flowIndex 3
#define vanesIndex 5

const byte rescan = 40;         //digital pin 38 for rescanning i2c slave devices
const byte zeropin = 41;        //pin for zeroing flowmeters through i2c command

Arduino_CRC32 crc32;
RTC_DS1307 rtc;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
Adafruit_INA219 ina219;

uint8_t addressBook[16];        //array containing i2c slave addresses
uint8_t nDevices;               //number of i2c slave devices
boolean firstTime = false;

float pressure, temperature;
const float Offset = 0.5;       //calibrartion offset for pressure sensor

long epochRecieved;

volatile boolean datapacket = false;

long int lastRequestMillis;
long int lastChkPendMillis;
long int lastChk;
int lastvalveChkMillis;
boolean valveFlag;

struct vanes vanes_array[numberOfVanes] = {
  {5, 38, 39, false, 0, 0, false, false},
  {6, 36, 37, false, 0, 0, false, false}
};
//  {7, 34, 35, false, 0, 0, false, false},
//  {8, 32, 33, false, 0, 0, false, false},
//  {9, 30, 31, false, 0, 0, false, false}


const byte getMultiTotalVolume = 130;      //command for getting total volume from all 6 meters
const byte zeroFlowMeters = 30;      //command for zeroing all 6 meters
const byte getKfactor = 40;       //for getting K factor vlues A and B for FLOW=A*HERTZ+B
const byte setKfactor = 25;       //send to flowmeter new A and B values

double currentVolume [tinys][6];
double lastSentVolume [tinys][6];
double volumetoSend [tinys][6];

volatile double Afactor1, Afactor2, Afactor3, Afactor4, Afactor5, Afactor6;
volatile double Bfactor1, Bfactor2, Bfactor3, Bfactor4, Bfactor5, Bfactor6;


#define CHECK_VALVE_CMD_MS 10000
#define PULSE_WIDTH_TIME_MS 200
//////////////////// LORA VARIABLES DEFINES AND VOIDS////////////////////////////
#define UPLINK_INTVAL_SEC 20  //milliseconds to send uplink messages
#define READ_FLOWS_INTVAL_MS 60000  //milliseconds to create and send uplink messages
#define DNLINK_LENGTH 50  // Downlink message payload length (in bytes)
uint8_t UPLINK_LENGTH = 50;  // Uplink message payload length (in bytes)

bool pendingPackets = false;

uint8_t dnlink[DNLINK_LENGTH];
uint8_t uplink[50];

byte uplinkmsgbyte[10][50];
byte byteslices[10];
int sliceIndex;

int downLinkLength;

int nextpart = 0;

String msg;

void printHex2(unsigned v) {
  v &= 0xff;
  if (v < 16)
    Serial1.print('0');
  Serial1.print(v, HEX);
}

uint8_t buf[40];

////SPITI
//static const u1_t PROGMEM APPEUI[8] = { 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13 };
//static const u1_t PROGMEM DEVEUI[8] = { 0x06, 0x76, 0x04, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
//static const u1_t PROGMEM APPKEY[16] = { 0xA5, 0xFC, 0x3E, 0xC3, 0x5F, 0xAC, 0x0C, 0x9A, 0xBF, 0x54, 0xC1, 0xB3, 0x1C, 0xD7, 0x8F, 0xE3 };

//////TEI
static const u1_t PROGMEM APPEUI[8] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11 };
static const u1_t PROGMEM DEVEUI[8] = { 0x40, 0x41, 0x98, 0x12, 0x9C, 0x20, 0x2F, 0x00 };
static const u1_t PROGMEM APPKEY[16] = { 0x2B, 0x5E, 0x44, 0x54, 0x60, 0xA1, 0x4C, 0xC9, 0xA9, 0x8C, 0x81, 0x9F, 0x79, 0x20, 0x9C, 0xD8 };

void os_getArtEui (u1_t* buf) {
  memcpy_P(buf, APPEUI, 8);
}

void os_getDevEui (u1_t* buf) {
  memcpy_P(buf, DEVEUI, 8);
}

void os_getDevKey(u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}

static osjob_t sendjob;

// Pin mapping
const lmic_pinmap lmic_pins =
{
  .nss = 10,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 9,
  .dio = {2, 6, 7},
};

#define LMIC_USE_INTERRUPTS

void onEvent(ev_t ev) {

  switch (ev)
  {
    case EV_SCAN_TIMEOUT:
      Serial1.println(F("EV_SCAN_TIMEOUT"));
      break;
    case EV_BEACON_FOUND:
      Serial1.println(F("EV_BEACON_FOUND"));
      break;
    case EV_BEACON_MISSED:
      Serial1.println(F("EV_BEACON_MISSED"));
      break;
    case EV_BEACON_TRACKED:
      Serial1.println(F("EV_BEACON_TRACKED"));
      break;
    case EV_JOINING:
      Serial1.println(F("EV_JOINING"));
      break;
    case EV_JOINED:
      Serial1.println(F("EV_JOINED"));
      {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        Serial1.print(F("netid: "));
        Serial1.println(netid, DEC);
        Serial1.print(F("devaddr: "));
        Serial1.println(devaddr, HEX);
        Serial1.print(F("AppSKey: "));
        for (size_t i = 0; i < sizeof(artKey); ++i) {
          if (i != 0)
            Serial1.print(F("-"));
          printHex2(artKey[i]);
        }
        Serial1.println(F(""));
        Serial1.print(F("NwkSKey: "));
        for (size_t i = 0; i < sizeof(nwkKey); ++i) {
          if (i != 0)
            Serial1.print(F("-"));
          printHex2(nwkKey[i]);
        }
        Serial1.println();
      }
      LMIC_setLinkCheckMode(0);
      break;
    case EV_JOIN_FAILED:
      Serial1.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial1.println(F("EV_REJOIN_FAILED"));
      break;
    case EV_TXCOMPLETE:
      Serial1.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      if (LMIC.txrxFlags & TXRX_ACK)
      {
        Serial1.println(F("Received ack"));
        if (datapacket == true) {
          if (byteslices[nextpart] != 0) {
            pendingPackets = true;
            Serial1.print(F("sending part")); Serial1.print(nextpart);
            Serial1.println(F(""));
            UPLINK_LENGTH = byteslices[nextpart];
            memcpy(uplink, uplinkmsgbyte[nextpart], byteslices[nextpart]);
            nextpart ++;
            os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(UPLINK_INTVAL_SEC), do_send);
            //os_setCallback(&sendjob, do_send);
          } else {
            Serial1.println(F("all packets sent"));
            for (int i = 0; i <= nDevices - 1; i++) {
              for (int j = 0; j < 6; j++) {
                lastSentVolume [addressBook[i]][j] = lastSentVolume [addressBook[i]][j] + volumetoSend [addressBook[i]][j];
                //                volumeSent [addressBook[i]][j] = volumetoSend [addressBook[i]][j];
                volumetoSend [addressBook[i]][j] = 0;
              }
            }
            pendingPackets = false;
            lastChkPendMillis = millis();  //timer for sending events
          }
        }
        datapacket = false;
      }
      if (LMIC.dataLen > 0)
      {
        memset(dnlink, 0, DNLINK_LENGTH);
        for (uint8_t i = 0; i < LMIC.dataLen; i++)
        {
          dnlink[i] = LMIC.frame[LMIC.dataBeg + i];
        }
        if (LMIC.dataLen >= DNLINK_LENGTH)
        {
          dnlink[DNLINK_LENGTH - 1] = 0;
        }
        else
        {
          dnlink[LMIC.dataLen] = 0;
        }
      }
      downLinkLength = LMIC.dataLen;
      break;
    case EV_LOST_TSYNC:
      Serial1.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      Serial1.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      // data received in ping slot
      Serial1.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      Serial1.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial1.println(F("EV_LINK_ALIVE"));
      break;
    case EV_TXSTART:
      Serial1.println(F("EV_TXSTART"));
      break;
    case EV_TXCANCELED:
      Serial1.println(F("EV_TXCANCELED"));
      break;
    case EV_RXSTART:
      /* do not print anything -- it wrecks timing */
      break;
    case EV_JOIN_TXCOMPLETE:
      Serial1.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
      break;
    default:
      Serial1.print(F("Unknown event: "));
      Serial1.println((unsigned) ev);
      break;
  }
}

/////////////////////////// END OF LORA DEFINES VOIDS VARIABLES ////////////////////////////

void setup() {
  Wire.begin();

  sensors.begin();

  Serial1.begin(115200);        // start Serial1 for output
  Serial1.println(F("Mega2560 starting"));

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //rtc.adjust(DateTime(2022, 6, 29, 10, 30, 0));
  }
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  DateTime now = rtc.now();
  Serial1.print(now.year(), DEC);
  Serial1.print('/');
  Serial1.print(now.month(), DEC);
  Serial1.print('/');
  Serial1.print(now.day(), DEC);
  Serial1.print(" (");

  Serial1.print(now.hour(), DEC);
  Serial1.print(':');
  Serial1.print(now.minute(), DEC);
  Serial1.print(':');
  Serial1.print(now.second(), DEC);
  Serial1.println();

    if (! ina219.begin()) {
      Serial1.println(F("Failed to find INA219 chip"));
      while (1) {
        delay(10);
      }
    }
    //to use a lower 16V, 400mA range (higher precision on volts and amps):
    ina219.setCalibration_16V_400mA();
    Serial1.println(F("Measuring voltage and current with INA219 ..."));

  scan_slaves();                    //call scan routine for i2c devices (flowmeters)
  if (nDevices == 0)
  {
    Serial1.println(F("Rescanning for flowmeters"));
    scan_slaves();
  }
  if (nDevices != 0) {
    firstTime = true;
    for (int i = 0; i <= nDevices - 1; i++) {
      Serial1.println(F("Scanning for volumes for first time"));
      requestMultiDataFromFlowmeter(getMultiTotalVolume, addressBook[i]);
      for (int j = 0; j < 6; j++) {
        lastSentVolume[addressBook[i]][j] =  currentVolume [addressBook[i]][j];
        Serial1.print(F("last volume sent ")); Serial1.print(addressBook[i]); Serial1.print(" ");
        Serial1.print(j); Serial1.print(F(" ")); Serial1.println(lastSentVolume[addressBook[i]][j]);
      }
    }
  }
  pinMode(rescan, INPUT_PULLUP);      //pin 40 to invoke a rescan CAN CHANGE PIN OR BE REMOVED IF NOT NEEDED
  pinMode(zeropin, INPUT_PULLUP);     //++++++++++++++++pin 41 to invoke a zero on all flow meters    SOS  +++++++++++++++ DELETE LATER

  for (int i = 0; i < numberOfVanes; i++) {
    pinMode(vanes_array[i].pinHigh, OUTPUT);
    pinMode(vanes_array[i].pinLow, OUTPUT);
  }
  delay(150);
  for (int i = 0; i < numberOfVanes; i++) {
    digitalWrite(vanes_array[i].pinHigh, LOW);
    digitalWrite(vanes_array[i].pinLow, HIGH);
  }
  delay(150);
  for (int i = 0; i < numberOfVanes; i++) {
    digitalWrite(vanes_array[i].pinHigh, LOW);
    digitalWrite(vanes_array[i].pinLow, LOW);
  }

  ////////////////////      LORA SETUP ////////////////////////
  memset(dnlink, 0, DNLINK_LENGTH);
  os_init();
  LMIC_reset();
  LMIC_setClockError(MAX_CLOCK_ERROR * 5 / 100);

  int binaryMsgtest = NodeInit(uplinkmsgbyte);
  UPLINK_LENGTH = binaryMsgtest;
  memcpy(uplink, uplinkmsgbyte[0], UPLINK_LENGTH);

  do_send(&sendjob);
  Serial1.println();
  ///////////////////  END OF LORA SETUP /////////////////////
  wdt_enable(0xB);                          //watchdog (8 for 1 second, 9 for 2 seconds, A for 4 seconds)

}


void scan_slaves()
{
  Serial1.println(F("Scanning for flowmeters"));
  byte error;
  uint8_t address;
  for (int i = 0; i < 16; i++)addressBook[i] = 0;         //scan for devices from address 0 to 15
  nDevices = 0;
  for (address = 0; address < 16; address++ )
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    Serial1.print(F("Scanning address: ")); Serial1.println(address);
    if (error == 0)
    {
      Serial1.print(F("Flowmeter found on address 0x"));
      if (address < 16)
      {
        Serial1.print(F("0"));
      }
      Serial1.print(address, HEX);
      Serial1.println(F("  !"));
      addressBook[nDevices] = address;        //save found devices to addressbook
      ++nDevices;
    }
  }
  Serial1.print(F("Number of flowmeters found: ")); Serial1.println(nDevices);
  for (int i = 0; i <= nDevices - 1; i++) {
    Serial1.print(F("Device number:")); Serial1.print(i + 1);
    Serial1.print(F(" found on address ")); Serial1.println(addressBook[i]);
  }
}

void requestMultiDataFromFlowmeter(uint8_t cmd, uint8_t FlowmeterAddress) {              //requesting multiple data from flowmeter
  Serial1.print(F("Requesting all 6 parameters, with command=")); Serial1.println(cmd);
  os_runloop_once();
  uint8_t bytesRequest = 29;
  Wire.beginTransmission(FlowmeterAddress);
  Wire.write(cmd);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Wire.requestFrom(FlowmeterAddress, bytesRequest);
    uint8_t databuff[32];
    int i = 0;
    while (1 < Wire.available()) {
      databuff[i] = Wire.read();
      i++;
    }
    double dataReceived1, dataReceived2, dataReceived3, dataReceived4, dataReceived5, dataReceived6;
    uint32_t crcReceived;
    dataReceived1 = *(double*)&databuff[0];
    dataReceived2 = *(double*)&databuff[4];
    dataReceived3 = *(double*)&databuff[8];
    dataReceived4 = *(double*)&databuff[12];
    dataReceived5 = *(double*)&databuff[16];
    dataReceived6 = *(double*)&databuff[20];
    crcReceived = *(uint32_t*)&databuff[24];
    uint32_t const crc32_res = crc32.calc((uint8_t *)&databuff, 24);            //master calulating crc from recieved data
    if (crcReceived == crc32_res)                                       //comparing recieved crc with calculated crc
    {
      currentVolume [FlowmeterAddress][0] = dataReceived1;
      currentVolume [FlowmeterAddress][1] = dataReceived2;
      currentVolume [FlowmeterAddress][2] = dataReceived3;
      currentVolume [FlowmeterAddress][3] = dataReceived4;
      currentVolume [FlowmeterAddress][4] = dataReceived5;
      currentVolume [FlowmeterAddress][5] = dataReceived6;
    } else {
      Serial1.println(F("CRC error"));
    }
    Serial1.print(currentVolume [FlowmeterAddress][0]); Serial1.print(F(":")); Serial1.print(currentVolume [FlowmeterAddress][1]);
    Serial1.print(F(":")); Serial1.print(currentVolume [FlowmeterAddress][2]); Serial1.print(F(":"));
    Serial1.print(currentVolume [FlowmeterAddress][3]); Serial1.print(F(":")); Serial1.print(currentVolume [FlowmeterAddress][4]);
    Serial1.print(F(":")); Serial1.println(currentVolume [FlowmeterAddress][5]);
  } else {
    Serial1.print(F("I2C Communication error with device address: ")); Serial1.println(FlowmeterAddress);
  }
  Serial1.print(F("Fulfilled request for all 6 parameters, with command=")); Serial1.println(cmd);
}


void sendToFlowmeter(uint32_t cmd, uint8_t FlowmeterAddress, uint32_t FlowMeterNumber , double a, double b) {        //send kfactor A and B to one flowmeter
  Serial1.print(F("Sending kfactor parameters A and B, with command=")); Serial1.println(cmd);
  uint32_t const Acrc32_res = crc32.calc((uint8_t *)&a, sizeof(a));
  uint32_t const Bcrc32_res = crc32.calc((uint8_t *)&b, sizeof(b));
  uint32_t const FMNcrc32_res = crc32.calc((uint8_t *)&FlowMeterNumber, 4);
  Wire.beginTransmission(FlowmeterAddress);
  Wire.write(cmd);
  Wire.write((uint8_t *)&FlowMeterNumber, 4);//send it in 4 bytes for compatibility with raspberry master
  Wire.write((uint8_t *)&FMNcrc32_res, sizeof(FMNcrc32_res));
  Wire.write((uint8_t *)&a, sizeof(a));
  Wire.write((uint8_t *)&Acrc32_res, sizeof(Acrc32_res));
  Wire.write((uint8_t *)&b, sizeof(b));
  Wire.write((uint8_t *)&Bcrc32_res, sizeof(Bcrc32_res));
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial1.print(F("K factor data sent. Flow=(")); Serial1.print(a, 4); Serial1.print(F(" x Hertz) + ")); Serial1.println(b, 4);
  } else {
    Serial1.println(F("Error sending K factor data A and B"));
  }
  Serial1.print(F("Kfactor parameters A and B sent, with command=")); Serial1.println(cmd);
}


void loop() {
  wdt_reset();
  os_runloop_once() ;
  //  DateTime now = rtc.now();
  //  long currentEpoch = now.unixtime() - 10800;

  double avgpressureVolt = 0;
  double pressureVolt = 0;
  for (int i = 0; i < 10; i++) {
    pressureVolt = pressureVolt + ((analogRead(5) * 5.00) / 1024);
  }
  avgpressureVolt = (pressureVolt / 10) - Offset;
  pressure = (avgpressureVolt * 7.895) / 4 ;              //in Atm
  if (pressure <= 0)pressure = 0;

  float oldtemperature = temperature;
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  if (temperature <= -127.00)temperature = oldtemperature;

  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;
  float power_mW = 0;
  //ina219.begin();
  power_mW = ina219.getPower_mW();
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  loadvoltage = busvoltage + (shuntvoltage / 1000);// 0.8 calibration
  //current_mA = 110.24;
  //loadvoltage = 12.32;
  int chk = millis() - lastChk;       //just to print something every 5 secs
  if ((chk > 5000)) {
    Serial1.print(F("Temperature:")); Serial1.println(temperature);
    Serial1.print(F("Pressure:")); Serial1.println(pressure);
    Serial1.print(F("current_mA:")); Serial1.println(current_mA);
    Serial1.print(F("loadvoltage:")); Serial1.println(loadvoltage);
    //    Serial1.print(F("currentEpoch:")); Serial1.println(currentEpoch);
    Serial1.print(F("busvoltage:")); Serial1.println(busvoltage);
    Serial1.print(F("PendingPkts:")); Serial1.println(pendingPackets);
    Serial1.println(F(""));
    lastChk = millis();
  }

  os_runloop_once() ;

  int statusRescan = digitalRead(rescan);     //read digital pin for rescan
  if (statusRescan == LOW) {                  //if pin is pulled low scan for i2c devices
    Serial1.print(F("Scan button pressed, scanning for flowmeters."));
    scan_slaves();
    firstTime = false;
  }
  if (nDevices == 0) {
    Serial1.print(F("There are no flowmeters connected, rescanning..."));
    scan_slaves();
    firstTime = false;
  }
  os_runloop_once() ;
  if ((nDevices != 0) && (firstTime == false)) {
    firstTime = true;
    Serial1.println(F("first time"));
    for (int i = 0; i <= nDevices - 1; i++) {
      os_runloop_once() ;
      firstTime = true;
      Serial1.println(F("Scanning for volumes for first time"));
      Serial1.println(addressBook[i]);
      requestMultiDataFromFlowmeter(getMultiTotalVolume, addressBook[i]);
      for (int j = 0; j < 6; j++) {
        os_runloop_once() ;
        firstTime = true;
        lastSentVolume[addressBook[i]][j] =  currentVolume [addressBook[i]][j];
        Serial1.print(F("last volume sent ")); Serial1.print(addressBook[i]); Serial1.print(F(" "));
        Serial1.print(j); Serial1.print(F(" ")); Serial1.println(lastSentVolume[addressBook[i]][j]);
      }
    }
    firstTime = true;
  }


  if (digitalRead(zeropin) == LOW) {
    for (int i = 0; i <= nDevices - 1; i++) {
      Serial1.println(F("zeroing button pressed flowmeters."));
      for (int j = 0; j < 6; j++) {
        volumetoSend [addressBook[0]][j] = 0;
        lastSentVolume [addressBook[0]][j] = 0;
      }
      requestMultiDataFromFlowmeter(zeroFlowMeters, addressBook[i]);
    }
  }


  if (dnlink[0] != 0)
  {
    String payload = String((char*)dnlink);
    Serial1.print(F("Payload hex: ")) ;
    for (int i = 0; i < (LMIC.dataLen); i++) {
      Serial1.print(dnlink[i], HEX);
      Serial1.print(F(" "));
    }
    Serial1.println(F(""));
    if ((dnlink[0] == 4) || (dnlink[0] == 2)) {             //"a"->'00000100' or "o"->'00000010'
      os_runloop_once() ;
      decodeForVanes(dnlink, LMIC.dataLen, vanes_array, numberOfVanes);              // decode binary message
    }
    memset(dnlink, 0, DNLINK_LENGTH);
  }

  int chkPendingCmd = millis() - lastChkPendMillis;
  if ((chkPendingCmd > CHECK_VALVE_CMD_MS)) {
    DateTime now = rtc.now();
    long currentEpoch = now.unixtime() - 10800;
    boolean sendEvent = false;
    os_runloop_once();
    for (int j = 0; j < numberOfVanes; j++) {
      os_runloop_once() ;
      if (vanes_array[j].pendCmd == true) {
        Serial1.print(F("epochdiff=")); Serial1.println(vanes_array[j].diffEpoch);
        Serial1.print(F("epochcmd=")); Serial1.println(vanes_array[j].epochCmd);
        Serial1.print(F("current epoch=")); Serial1.println(currentEpoch);
        if ((vanes_array[j].epochCmd + vanes_array[j].diffEpoch) <= currentEpoch) {
          if (vanes_array[j].onOff == true) {
            digitalWrite(vanes_array[j].pinHigh, HIGH);
            digitalWrite(vanes_array[j].pinLow, LOW);
            Serial1.print(F("vana ")); Serial1.print(vanes_array[j].vanaId);
            Serial1.println(F(" turned on"));
            sendEvent = true;
          } else if (vanes_array[j].onOff == false) {
            digitalWrite(vanes_array[j].pinHigh, LOW);
            digitalWrite(vanes_array[j].pinLow, HIGH);
            Serial1.print(F("vana ")); Serial1.print(vanes_array[j].vanaId);
            Serial1.println(F(" turned off"));
            sendEvent = true;
          }
          valveFlag = true;
          vanes_array[j].passReport = true;
          lastvalveChkMillis = millis();
          vanes_array[j].pendCmd = false;
        }
      }
    }
    lastChkPendMillis = millis();
  }
  os_runloop_once() ;

  int valveChkMillis = millis() - lastvalveChkMillis;          //pull both valve pins to low
  if ((valveChkMillis > PULSE_WIDTH_TIME_MS) && (valveFlag == true)) {         //for testing with leds, valve needs smaller time about 200mS
    Serial1.println(F("timer"));
    os_runloop_once();
    for (int j = 0; j < numberOfVanes; j++) {
      digitalWrite(vanes_array[j].pinHigh, LOW);
      digitalWrite(vanes_array[j].pinLow, LOW);
      valveFlag = false;
      Serial1.println(F("zero all vanes pins"));
    }
    lastvalveChkMillis = millis();
  }

  long int alive = millis() - lastRequestMillis;
  if ((alive > READ_FLOWS_INTVAL_MS)) { //++++++INTERVAL FOR MESSAGE SENDING+++++++
    os_runloop_once();
    if (pendingPackets) {
      LMIC_clrTxData();
      os_clearCallback (&sendjob);
      Serial1.println(F("sending OLD data again"));
      //      if (sliceIndex >= 1) {
      //        pendingPackets = true;
      //      }
      UPLINK_LENGTH = byteslices[0];
      memcpy(uplink, uplinkmsgbyte[0], byteslices[0]);
      nextpart = 1;
      //os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(UPLINK_INTVAL_SEC), do_send);
      os_setCallback(&sendjob, do_send);
      lastRequestMillis = millis();
    }
    else                                    ////////////if there were  NOT  pending packets just update all volumes and create new pessage
    {
      Serial1.println(F("sending data to create new message"));
      for (int i = 0; i <= nDevices - 1; i++) {                 // commands for being sent to flowmeter
        requestMultiDataFromFlowmeter(getMultiTotalVolume, addressBook[i]);
      }
      DateTime now = rtc.now();
      long currentEpoch = now.unixtime() - 10800;
      sliceIndex = 0;
      sliceIndex = NodeReport(uplinkmsgbyte, byteslices, currentVolume, lastSentVolume, nDevices,
                              addressBook, volumetoSend, currentEpoch, pressure, temperature,
                              loadvoltage, current_mA, numberOfVanes, vanes_array, powerIndex,
                              tempIndex, pressureIndex, flowIndex, vanesIndex);
      if (sliceIndex >= 1) {
        pendingPackets = true;
      }
      UPLINK_LENGTH = byteslices[0];
      memcpy(uplink, uplinkmsgbyte[0], byteslices[0]);
      nextpart = 1;
      //os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(UPLINK_INTVAL_SEC), do_send);
      os_setCallback(&sendjob, do_send);
      lastRequestMillis = millis();
    }
  }
}


void do_send(osjob_t* _j)
{
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial1.println(F("OP_TXRXPEND, not sending"));
  } else {
    datapacket = true;
    LMIC_setTxData2(1, uplink, UPLINK_LENGTH, 1);
    Serial1.print(F("Packet queued="));
    for (int m = 0 ; m < UPLINK_LENGTH; m++) {
      if ( uplink[m] < 0x10)Serial1.print(F("0"));
      Serial1.print(uplink[m], HEX);
      Serial1.print(F(" "));
    }
    Serial1.println(F(""));
  }
}
