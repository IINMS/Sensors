#include "LoraBinary.h"
#include <math.h>
#include <stdio.h>
#include <inttypes.h>

char* cmdArray[8] = {"0", "1", "o", "r", "a", "c"};
char* value_typeArray[8] = {"m", "e", "t", "p", "i"};
char* long_value_sizeArray[5] = {8, 16, 24, 32};
char* dev_codeArray[] = {"d", "ir", "mir", "mth", "cl", "sn", "snrs", "rrv", "cl_legacy", "cl_debug", "cl_debug_mini"};
//char* long_value_signArray[] = {"-", "+"};
//char* op_io_typeArray[] = {"gpio", "port", "subnetwork"};
//char* op_io_portArray[] = {"ttyACM0", "ttyS0", "ttyUSB0", "ttyUSB1"};

//byte uplinkmsg[300];   //maximum for 4*6 flowmeters??????
//byte uplinkslices[10];
byte charedFloat[50];   //

int sizeofCharedFloat = 0;

int arraySearcher(char** Array, int arraySize, char *x)     //get index in array
{
  int pos;
  for (int i = 0; i < arraySize; i++) {
    if (Array[i] == x) {
      pos = i;
    }
  }
  return pos;
}

int ExtractDecimalPart(float Value)         //get decimal part of a float number
{
  String str = String(Value, 2);
  int len = str.length();
  String str2 = &str[len - 2];
  int DecimalPart = str2.toInt();
  return DecimalPart;
}

long getIntegral(float Value)         //get the integral part of a float number
{
  long integral = (long)(Value);
  return integral;
}

int getValueSize(float Value)         //get the integrals size in bits
{
  int integral_size;
  if (Value >= 1.00) {
    double integralPart = (log(Value) / log(2)) + 1 ;
    long integral = (long)(integralPart);
    if ((integral > 0) && (integral <= 8))integral_size = 8;
    else if ((integral > 8) && (integral <= 16))integral_size = 16;
    else if ((integral > 16) && (integral <= 24))integral_size = 24;
    else integral_size = 32;
  } else integral_size = 8;
  return integral_size;
}

void floatsCharArray(int deviceIndex, uint8_t follow, ...) {
  float Value[follow];

  va_list arguments;              //create the list of args
  va_start ( arguments, follow ); // get arguments starting from after arg follow
  for (int i = 0; i < follow; i++) {
    Value[i] = va_arg ( arguments, double );
    Serial1.print(F("float value to char=[")); Serial1.print(i); Serial1.print(F("]=")); Serial1.println(Value[i]);
  }
  va_end ( arguments );

  sizeofCharedFloat = 0;
  charedFloat[0] = deviceIndex;                           //device index

  int valTypeArrayPos = arraySearcher(value_typeArray, sizeof(value_typeArray) / sizeof(value_typeArray[0]), "m");   //value type m=measurment
  charedFloat[1] = valTypeArrayPos;
  charedFloat[1] = charedFloat[1] << 4;
  charedFloat[1] = charedFloat[1] | follow;              //value_subvalues_number
  charedFloat[1] = charedFloat[1] << 1;
  charedFloat[1] = charedFloat[1] | 0x00;            // value_has_time_diff "1" true

  //  int epochDiff = 0x0001;                           //value_minutes_diff 16 bits
  //  charedFloat[3] = epochDiff;
  //  charedFloat[2] = epochDiff >> 8;
  //  int uplinksize = 4;
  int uplinksize = 2;
  for (int i = 0; i < follow; i++) {
    int decimalPartValue = ExtractDecimalPart(abs(Value[i]));                 //long_value_has_fraction "1" true
    if (decimalPartValue != 0)charedFloat[uplinksize] = 1;              //long_value_has_fraction "1" true
    else charedFloat[uplinksize] = 1;            //sos to be changed later on because all numbers are treated as floats
    charedFloat[uplinksize] = charedFloat[uplinksize] << 7;
    byte decimalPartValueByte = decimalPartValue >> 1;
    charedFloat[uplinksize] = charedFloat[uplinksize] | decimalPartValueByte;
    uplinksize++;
    charedFloat[uplinksize] = decimalPartValue << 7;

    int value_size_Value = getValueSize(abs(Value[i]));                    //get the size of the intgral part 8-16-24-32 bit
    int value_sizePos_Value = arraySearcher(long_value_sizeArray, sizeof(long_value_sizeArray) / sizeof(long_value_sizeArray[0]), value_size_Value);  //position in array
    charedFloat[uplinksize] = (value_sizePos_Value << 1) | charedFloat[uplinksize];
    if (Value[i] >= 0)charedFloat[uplinksize] = charedFloat[uplinksize] | 0x01;         ///sign of float is +
    else charedFloat[uplinksize] = charedFloat[uplinksize] | 0x00;                    ///sign of float is -
    long int integral_value_Value = getIntegral(abs(Value[i]));                        //long_value_base
    uplinksize++;
    if ((integral_value_Value & 0xff000000) != 0x0000000) {
      charedFloat[uplinksize] = (integral_value_Value & 0xff000000) >> 24; uplinksize++;
      charedFloat[uplinksize] = (integral_value_Value & 0x00ff0000) >> 16; uplinksize++;
      charedFloat[uplinksize] = (integral_value_Value & 0x0000ff00) >> 8; uplinksize++;
      charedFloat[uplinksize] = (integral_value_Value & 0x000000ff); uplinksize++;
    } else if ((integral_value_Value & 0x00ff0000) != 0x0000000) {
      charedFloat[uplinksize] = (integral_value_Value & 0x00ff0000) >> 16; uplinksize++;
      charedFloat[uplinksize] = (integral_value_Value & 0x0000ff00) >> 8; uplinksize++;
      charedFloat[uplinksize] = (integral_value_Value & 0x000000ff); uplinksize++;
    } else if ((integral_value_Value & 0x0000ff00) != 0x0000000) {
      charedFloat[uplinksize] = (integral_value_Value & 0x0000ff00) >> 8; uplinksize++;
      charedFloat[uplinksize] = integral_value_Value & 0x000000ff; uplinksize++;
    } else if (((integral_value_Value & 0x000000ff) == 0x0000000) || ((integral_value_Value & 0x000000ff) != 0x0000000)) {
      charedFloat[uplinksize] = integral_value_Value & 0x000000ff; uplinksize++;
    }
  }
  Serial1.print(F("chared float value="));
  for (int i = 0; i < uplinksize; i++) {
    if (charedFloat[i] < 0x10)Serial1.print(F("0"));
    Serial1.print(charedFloat[i], HEX);
    Serial1.print(F(" "));
  }
  sizeofCharedFloat = uplinksize;
  Serial1.println(F(""));
}


void decodeForVanes(byte* dnlink, int sizeofdnlink, struct vanes* vanes_array, int x) {       //opoy x = arthmos vanwn
  Serial1.print(F("payload hex for decoding="));
  for (int i = 0; i < sizeofdnlink; i++) {
    if (dnlink[i] < 0x10)Serial1.print(F("0"));
    Serial1.print(dnlink[i], HEX);
    Serial1.print(F(""));
  }
  Serial1.println(F(""));

  int is_partReceived = *(uint8_t*)&dnlink[1];
  uint32_t epochReceivedReversed = *(uint32_t*)&dnlink[2];
  long int epochReceived = __builtin_bswap32(epochReceivedReversed);
  int j = 6;
  while (j < sizeofdnlink) {
    int index = 0;
    int value_type = 0;
    boolean trigger = false;
    int subvalue = 0;
    boolean hastimediff = false;
    boolean onoff = false;

    index = int(dnlink[j]);
    j += 1;
    value_type = (int(dnlink[j])) >> 5;
    if (value_typeArray[value_type] == "t") { //maybe switch-case
      trigger = true;
    }

    subvalue = (int(dnlink[j]) & 0x1E) >> 1;
    if (subvalue == 0) {
    } else {}

    hastimediff = (int(dnlink[j]) & 0x01);
    //int msbBitMinDiff = (int(dnlink[j]) & 0x01);
    //int16_t reversedMsbBit = __builtin_bswap16(msbBitMinDiff);
    j += 1;
    int16_t timediff = 0;
    if (hastimediff == 1) {
      timediff = ((*(int16_t*)&dnlink[j]));
      Serial1.print(F("timediff=")); Serial1.println(timediff);
      j += 2; //αυτό δεν υπήρχε, αλλα μάλλον χρειάζεται. θέλει έλεγχο 16-6-2022
    } else timediff = 0;

    onoff = (int(dnlink[j]) & 0x01);
    if (trigger == true) {
      for (int i = 0; i < x; i++) {
        if (vanes_array[i].vanaId == index) {
          vanes_array[i].passReport = false;
          vanes_array[i].onOff = onoff;
          vanes_array[i].pendCmd = true;
          if (hastimediff == 1) {
            vanes_array[i].epochCmd = epochReceived;
            vanes_array[i].diffEpoch = timediff * 60;
            Serial1.println(vanes_array[i].diffEpoch);
          } else {
            vanes_array[i].epochCmd = epochReceived;
            vanes_array[i].diffEpoch = 0;
          }
        }
      }
    }
    j += 1;
  }
  for (int i = 0; i < x; i++) {
    Serial1.print(F("vanes data is="));
    Serial1.print(vanes_array[i].vanaId);
    Serial1.print(vanes_array[i].pendCmd);
    Serial1.print(vanes_array[i].epochCmd);
    Serial1.print(vanes_array[i].diffEpoch);
    Serial1.print(vanes_array[i].onOff);
    Serial1.println(vanes_array[i].passReport);
  }
  Serial1.println("hex payload has been decoded");
}


int NodeInit(byte uplinkmsg[10][50]) {
  Serial1.println(F("creating init message"));
  int uplinkmsgIndex = 0;
  for (int i = 0; i <= 10; i++) {
    for (int j = 0; j <= 50; j++) {
      uplinkmsg[i][j] = 0;
    }
  }
  int cmdArrayPos = arraySearcher(cmdArray, sizeof(cmdArray) / sizeof(cmdArray[0]), "0");   //cmd
  uplinkmsg[0][0] = cmdArrayPos;    //starting from [1], because [0] is lost at memcpy
  int dev_codeArrayPos = arraySearcher(dev_codeArray, sizeof(dev_codeArray) / sizeof(dev_codeArray[0]), "d");   //dev_code
  uplinkmsg[0][1] = dev_codeArrayPos;
  uplinkmsgIndex = 2;
  Serial1.print(F("Init message created="));
  for (int i = 0; i < uplinkmsgIndex; i++) {
    if (uplinkmsg[0][i] < 0x10)Serial1.print("0");
    Serial1.print(uplinkmsg[0][i], HEX);
    Serial1.print(F(" "));
  }
  Serial1.println(F(" "));
  return uplinkmsgIndex;
}


int NodeReport(byte uplinkmsg[10][50], byte* uplinkslices, double currentVolume[][6], double lastSentVolume[][6],
               uint8_t nDevices, uint8_t addressBook[16], double volumetoSend[][6], long currentEpoch, float pressure,
               float temperature, float loadvoltage, float current_mA, int numberOfVanes, struct vanes* vanes_array, uint8_t powerIndex, 
               uint8_t tempIndex, uint8_t pressureIndex, uint8_t flowIndex, uint8_t vanesIndex) {
  Serial1.println(F("creating report message"));
  int uplinkmsgIndex = 0;
  int pseudomsgIndex = 0;
  int sliceInd = 0;
  for (int i = 0; i <= 10; i++) {
    for (int j = 0; j <= 50; j++) {
      uplinkmsg[i][j] = 0;
    }
  }
  for (int i = 0; i <= 10; i++) {
    uplinkslices[i] = 0;
  }
  int cmdArrayPos = arraySearcher(cmdArray, sizeof(cmdArray) / sizeof(cmdArray[0]), "r");   //cmd
  uplinkmsg[sliceInd][1] = cmdArrayPos;    //is part starting from [1], because [0] is lost at memcpy
  uplinkmsg[sliceInd][2] = 0;
  uplinkmsg[sliceInd][3] = currentEpoch >> 24;
  uplinkmsg[sliceInd][4] = currentEpoch >> 16;
  uplinkmsg[sliceInd][5] = currentEpoch >> 8;
  uplinkmsg[sliceInd][6] = currentEpoch;

  //float loadvoltage = 12.34;
  //float current_mA = 0.36;
                                         //DEVICE INDEX 0 IS BATTERY VOLTAGE AND POWER CONSUMPTION
  for (int i = 0; i < 50; i++) {
    charedFloat[i] = 0x00;
  }
  floatsCharArray(powerIndex, 2, loadvoltage, current_mA);
  memcpy(&uplinkmsg[sliceInd][7], charedFloat, sizeofCharedFloat);
  uplinkmsgIndex = 7 + sizeofCharedFloat;

                                        //DEVICE INDEX 1 IS PRESSURE SENSOR
  for (int i = 0; i < 50; i++) {
    charedFloat[i] = 0x00;
  }
  floatsCharArray(pressureIndex, 1, pressure);
  memcpy(&uplinkmsg[sliceInd][uplinkmsgIndex], charedFloat, sizeofCharedFloat);
  uplinkmsgIndex = uplinkmsgIndex + sizeofCharedFloat;

                                      //DEVICE INDEX 2 IS TEMPERATURE SENSOR
  for (int i = 0; i < 50; i++) {
    charedFloat[i] = 0x00;
  }
  floatsCharArray(tempIndex, 1, temperature);
  memcpy(&uplinkmsg[sliceInd][uplinkmsgIndex], charedFloat, sizeofCharedFloat);
  uplinkmsgIndex = uplinkmsgIndex + sizeofCharedFloat;

  uint8_t deviceIndex = flowIndex;
  for (int i = 0; i <= nDevices - 1; i++) {
    for (int j = 0; j < 6; j++) {
      if ((currentVolume [addressBook[i]][j] - lastSentVolume [addressBook[i]][j]) > 0) {
        if ((currentVolume [addressBook[i]][j] - lastSentVolume [addressBook[i]][j]) >= 0) {
          volumetoSend [addressBook[i]][j] = currentVolume [addressBook[i]][j] - lastSentVolume [addressBook[i]][j];
        }
        else if ((currentVolume [addressBook[i]][j] - lastSentVolume [addressBook[i]][j] < 0)) {
          volumetoSend [addressBook[i]][j] = currentVolume [addressBook[i]][j] - lastSentVolume [addressBook[i]][j] + 1000000000; //tiny flowmeter resets volume at 1000000m3 of volume
        }
        for (int z = 0; z < 50; z++) {
          charedFloat[z] = 0x00;
        }
        floatsCharArray(deviceIndex, 1, volumetoSend [addressBook[i]][j]);
        memcpy(&uplinkmsg[sliceInd][uplinkmsgIndex], charedFloat, sizeofCharedFloat);
        uplinkmsgIndex = uplinkmsgIndex + sizeofCharedFloat;
        deviceIndex++;
      }
      if (uplinkmsgIndex >= 35) {
        uplinkslices[sliceInd] = uplinkmsgIndex - 1;
        sliceInd++;
        uplinkmsgIndex = 0;
        for (int x = 1; x < 7; x++) {   //copy cmd-ispart-epoch to the message again
          uplinkmsg[sliceInd][x] = uplinkmsg[0][x];
        }
        uplinkmsgIndex = uplinkmsgIndex + 7;
      }
    }
    //uplinkslices[sliceInd] = uplinkmsgIndex - 1;//////////*************sos does it remain here???? *******************//////////////////
  }
  deviceIndex = vanesIndex;
  for (int j = 0; j < numberOfVanes; j++) {
    if (vanes_array[j].passReport) {
      Serial1.print(F("creating vanes message"));Serial1.println(deviceIndex);
      vanes_array[j].passReport = false;
      uplinkmsg[sliceInd][uplinkmsgIndex] = deviceIndex;
      //Serial1.println(&uplinkmsg[sliceInd][uplinkmsgIndex]);
      uplinkmsgIndex = uplinkmsgIndex + 1;
      uplinkmsg[sliceInd][uplinkmsgIndex] = 34;    //34 is value type = e (event) value sub value = 1 and has time diff =0
      uplinkmsgIndex = uplinkmsgIndex + 1;
      if (vanes_array[j].onOff == false)uplinkmsg[sliceInd][uplinkmsgIndex] = 0;
      else uplinkmsg[sliceInd][uplinkmsgIndex] = 1;
      uplinkmsgIndex = uplinkmsgIndex + 1;
      vanes_array[j].passReport = false;
    }
    deviceIndex++;
    if (uplinkmsgIndex >= 35) {
      uplinkslices[sliceInd] = uplinkmsgIndex - 1;
      sliceInd++;
      uplinkmsgIndex = 0;
      for (int x = 1; x < 7; x++) {   //copy cmd-ispart-epoch to the message again
        uplinkmsg[sliceInd][x] = uplinkmsg[0][x];
      }
      uplinkmsgIndex = uplinkmsgIndex + 7;
    }
    //uplinkslices[sliceInd] = uplinkmsgIndex - 1;//////////*************sos does it remain here???? *******************//////////////////
  }
  uplinkslices[sliceInd] = uplinkmsgIndex - 1;//////////*************sos does it remain here???? *******************//////////////////

  for (int j = 0; j <= sliceInd; j++) {             //to resolve above problem with [0]
    for (int i = 1; i < uplinkslices[j] + 1; i++) {
      uplinkmsg[j][i - 1] = uplinkmsg[j][i];
    }
  }

  for (int i = 0; i <= sliceInd; i++) {       //to remove message containing only epoch
    if (uplinkslices[i] <= 7) {
      for (int j = 0; j <= 50; j++) {
        uplinkmsg[i][j] = 0;
      }
      uplinkslices[i] = 0;
      sliceInd = sliceInd - 1;
    }
  }

  for (int i = 0; i <= sliceInd; i++) {
    Serial1.print(F("size=")); Serial1.print(uplinkslices[i]);
    Serial1.println(F("")); Serial1.print(F("report message="));
    for (int j = 0; j < uplinkslices[i]; j++) {
      if (uplinkmsg[i][j] < 0x10)Serial1.print(F("0"));
      Serial1.print(uplinkmsg[i][j], HEX);
      Serial1.print(F(""));
    }
    Serial1.print(F(""));
  }
  Serial1.println(F(""));
  Serial1.print(F("slice index=")); Serial1.println(sliceInd);
  return sliceInd;
}
