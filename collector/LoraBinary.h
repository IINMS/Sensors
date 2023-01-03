#ifndef LoraBinary_H
#define LoraBinary_H

#include <Arduino.h>

struct vanes {
  int vanaId;
  int pinHigh;              //cotrol pin
  int pinLow;               //control pin
  boolean pendCmd;          //if there is a pending command
  long epochCmd;            //epoch time to execute command
  int diffEpoch;            //differential from epoch
  boolean onOff;            //command to turn off or on on the given above epoch
  boolean passReport;       //report to cloud new state
};


int NodeInit(byte uplinkmsg[10][50]);
int NodeReport(byte uplinkmsg[10][50], byte* uplinkslices, double currentVolume[][6],double lastSentVolume[][6], uint8_t nDevices, 
                uint8_t addressBook[16], double volumetoSend[][6], long currentEpoch, float pressure, float temperature, float loadvoltage,
                float current_mA, int numberOfVanes, struct vanes* vanes_array, uint8_t powerIndex, 
                              uint8_t tempIndex, uint8_t pressureIndex, uint8_t flowIndex, uint8_t vanesIndex);
void decodeForVanes(byte* dnlink , int sizeofdnlink, struct vanes* vanes_array, int x);

#endif
