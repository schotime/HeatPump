/*
  HeatPump.cpp - Mitsubishi Heat Pump control library for Arduino
  Copyright (c) 2017 Al Betschart.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "HeatPump.h"

// Structures //////////////////////////////////////////////////////////////////

bool operator==(const heatpumpSettings& lhs, const heatpumpSettings& rhs) {
  return lhs.power == rhs.power && 
         lhs.mode == rhs.mode && 
         lhs.temperature == rhs.temperature && 
         lhs.fan == rhs.fan &&
         lhs.vane == rhs.vane &&
         lhs.wideVane == rhs.wideVane;
}

bool operator!=(const heatpumpSettings& lhs, const heatpumpSettings& rhs) {
  return lhs.power != rhs.power || 
         lhs.mode != rhs.mode || 
         lhs.temperature != rhs.temperature || 
         lhs.fan != rhs.fan ||
         lhs.vane != rhs.vane ||
         lhs.wideVane != rhs.wideVane;
}

bool operator!(const heatpumpSettings& settings) {
  return !settings.power && 
         !settings.mode && 
         !settings.temperature && 
         !settings.fan &&
         !settings.vane &&
         !settings.wideVane;
}


// Constructor /////////////////////////////////////////////////////////////////

HeatPump::HeatPump() {
  lastSend = 0;
  infoMode = false;
}

// Public Methods //////////////////////////////////////////////////////////////

void HeatPump::connect(HardwareSerial *serial) {
  _HardSerial = serial;
  _HardSerial->begin(2400, SERIAL_8E1);
  
  // settle before we start sending packets
  delay(2000);

  // send the CONNECT packet twice - need to copy the CONNECT packet locally
  byte packet[CONNECT_LEN];
  memcpy(packet, CONNECT, CONNECT_LEN);
  for(int count = 0; count < 2; count++) {
    writePacket(packet, CONNECT_LEN);
    delay(1100);
  }
}

bool HeatPump::update() {
  while(!canSend()) { delay(10); }

  byte packet[PACKET_LEN] = {};
  createPacket(packet, wantedSettings);
  writePacket(packet, PACKET_LEN);

  delay(1000);
  
  int packetType = readPacket();
  
  if(packetType == RCVD_PKT_UPDATE_SUCCESS) {
    // call sync() to get the latest settings from the heatpump, which should now have the updated settings
    sync(RQST_PKT_SETTINGS);

    return true;
  } else {
    return false;
  }
}

void HeatPump::sync(byte packetType) {
  if(canSend()) {
    byte packet[PACKET_LEN] = {};
    createInfoPacket(packet, packetType);
    writePacket(packet, PACKET_LEN);
  }

  readPacket(); 
}


heatpumpSettings HeatPump::getSettings() {
  return currentSettings;
}

void HeatPump::setSettings(heatpumpSettings settings) {
  setPowerSetting(settings.power);
  setModeSetting(settings.mode);
  setTemperature(settings.temperature);
  setFanSpeed(settings.fan);
  setVaneSetting(settings.vane);
  setWideVaneSetting(settings.wideVane);
}

bool HeatPump::getPowerSettingBool() {
  return currentSettings.power == POWER_MAP[1] ? true : false;
}

void HeatPump::setPowerSetting(bool setting) {
  wantedSettings.power = lookupByteMapIndex(POWER_MAP, 2, POWER_MAP[setting ? 1 : 0]) > -1 ? POWER_MAP[setting ? 1 : 0] : POWER_MAP[0];
}

String HeatPump::getPowerSetting() {
  return currentSettings.power;
}

void HeatPump::setPowerSetting(String setting) {
  wantedSettings.power = lookupByteMapIndex(POWER_MAP, 2, setting) > -1 ? setting : POWER_MAP[0];
}

String HeatPump::getModeSetting() {
  return currentSettings.mode;
}

void HeatPump::setModeSetting(String setting) {
  wantedSettings.mode = lookupByteMapIndex(MODE_MAP, 5, setting) > -1 ? setting : MODE_MAP[0];
}

int HeatPump::getTemperature() {
  return currentSettings.temperature;
}

void HeatPump::setTemperature(int setting) {
  wantedSettings.temperature = lookupByteMapIndex(TEMP_MAP, 16, setting) > -1 ? setting : TEMP_MAP[0];
}

String HeatPump::getFanSpeed() {
  return currentSettings.fan;
}

void HeatPump::setFanSpeed(String setting) {
  wantedSettings.fan = lookupByteMapIndex(FAN_MAP, 6, setting) > -1 ? setting : FAN_MAP[0];
}

String HeatPump::getVaneSetting() {
  return currentSettings.vane;
}

void HeatPump::setVaneSetting(String setting) {
  wantedSettings.vane = lookupByteMapIndex(VANE_MAP, 7, setting) > -1 ? setting : VANE_MAP[0];
}

String HeatPump::getWideVaneSetting() {
  return currentSettings.wideVane;
}

void HeatPump::setWideVaneSetting(String setting) {
  wantedSettings.wideVane = lookupByteMapIndex(WIDEVANE_MAP, 7, setting) > -1 ? setting : WIDEVANE_MAP[0];
}

int HeatPump::getRoomTemperature() {
  return currentRoomTemp;
}

unsigned int HeatPump::FahrenheitToCelsius(unsigned int tempF) {
  double temp = (tempF - 32) / 1.8;                //round up if heat, down if cool or any other mode
  return currentSettings.mode == MODE_MAP[0] ? ceil(temp) : floor(temp);
}

unsigned int HeatPump::CelsiusToFahrenheit(unsigned int tempC) {
  double temp = (tempC * 1.8) + 32;                //round up if heat, down if cool or any other mode
  return currentSettings.mode == MODE_MAP[0] ? ceil(temp) : floor(temp);
}

void HeatPump::setSettingsChangedCallback(SETTINGS_CHANGED_CALLBACK_SIGNATURE) {
  this->settingsChangedCallback = settingsChangedCallback;
}

void HeatPump::setPacketCallback(PACKET_CALLBACK_SIGNATURE) {
  this->packetCallback = packetCallback;
}

void HeatPump::setRoomTempChangedCallback(ROOM_TEMP_CHANGED_CALLBACK_SIGNATURE) {
  this->roomTempChangedCallback = roomTempChangedCallback;
}

//#### WARNING, THE FOLLOWING METHOD CAN F--K YOUR HP UP, USE WISELY ####
void HeatPump::sendCustomPacket(byte data[], int len) {
  while(!canSend()) { delay(10); }
  len += 2;                          //+2, for FC and CHKSUM
  byte packet[len];
  packet[0] = 0xfc;
  for (int i = 0; i < len-1; i++) {
    packet[i+1] = data[i]; 
  }
  byte chkSum = checkSum(packet, len-1);
  packet[len] = chkSum;

  writePacket(packet, len);
  delay(1000);
}

// Private Methods //////////////////////////////////////////////////////////////

int HeatPump::lookupByteMapIndex(const int valuesMap[], int len, int lookupValue) {
  for (int i = 0; i < len; i++) {
    if (valuesMap[i] == lookupValue) {
      return i;
    }
  }
  return -1;
}

int HeatPump::lookupByteMapIndex(const String valuesMap[], int len, String lookupValue) {
  for (int i = 0; i < len; i++) {
    if (valuesMap[i] == lookupValue) {
      return i;
    }
  }
  return -1;
}


String HeatPump::lookupByteMapValue(const String valuesMap[], const byte byteMap[], int len, byte byteValue) {
  for (int i = 0; i < len; i++) {
    if (byteMap[i] == byteValue) {
      return valuesMap[i];
    }
  }
  return valuesMap[0];
}

int HeatPump::lookupByteMapValue(const int valuesMap[], const byte byteMap[], int len, byte byteValue) {
  for (int i = 0; i < len; i++) {
    if (byteMap[i] == byteValue) {
      return valuesMap[i];
    }
  }
  return valuesMap[0];
}

bool HeatPump::canSend() {
  return (millis() - PACKET_SENT_INTERVAL_MS) > lastSend;
}  

byte HeatPump::checkSum(byte bytes[], int len) {
  byte sum = 0;
  for (int i = 0; i < len; i++) {
    sum += bytes[i];
  }
  return (0xfc - sum) & 0xff;
}

void HeatPump::createPacket(byte *packet, heatpumpSettings settings) {
  for (int i = 0; i < HEADER_LEN; i++) {
    packet[i] = HEADER[i];
  }

  packet[8]  = POWER[lookupByteMapIndex(POWER_MAP, 2, settings.power)];
  packet[9]  = MODE[lookupByteMapIndex(MODE_MAP, 5, settings.mode)];
  packet[10] = TEMP[lookupByteMapIndex(TEMP_MAP, 16, settings.temperature)];
  packet[11] = FAN[lookupByteMapIndex(FAN_MAP, 6, settings.fan)];
  packet[12] = VANE[lookupByteMapIndex(VANE_MAP, 7, settings.vane)];
  packet[13] = 0x00;
  packet[14] = 0x00;
  packet[15] = WIDEVANE[lookupByteMapIndex(WIDEVANE_MAP, 7, settings.wideVane)];

  // pad the packet out 
  for (int i = 0; i < 5; i++) {
    packet[i + 16] = 0x00;
  }

  // add the checksum
  byte chkSum = checkSum(packet, 21);
  packet[21] = chkSum;
}

void HeatPump::createInfoPacket(byte *packet, byte packetType) {
  // add the header to the packet
  for (int i = 0; i < INFOHEADER_LEN; i++) {
    packet[i] = INFOHEADER[i];
  }
  
  // set the mode - settings or room temperature
  if(packetType) {
    packet[5] = INFOMODE[packetType];
  } else {
    packet[5] = INFOMODE[infoMode ? 1 : 0];
    infoMode = !infoMode;
  }

  // pad the packet out
  for (int i = 0; i < 15; i++) {
    packet[i + 6] = 0x00;
  }

  // add the checksum
  byte chkSum = checkSum(packet, 21);
  packet[21] = chkSum;
}

void HeatPump::writePacket(byte *packet, int length) {
  for (int i = 0; i < length; i++) {
     _HardSerial->write((uint8_t)packet[i]);
  }

  if(packetCallback) {
    packetCallback(packet, length, "packetSent");
  }

  lastSend = millis();
}

int HeatPump::readPacket() {
  byte header[INFOHEADER_LEN] = {};
  byte data[PACKET_LEN] = {};
  bool foundStart = false;
  int dataSum = 0;
  byte checksum = 0;
  byte dataLength = 0;
  
  if(_HardSerial->available() > 0) {
    // read until we get start byte 0xfc
    while(_HardSerial->available() > 0 && !foundStart) {
      header[0] = _HardSerial->read();
      if(header[0] == 0xFC) {
        foundStart = true;
        delay(100); // found that this delay increases accuracy when reading, might not be needed though
      }
    }

    if(!foundStart) {
      return RCVD_PKT_FAIL;
    }
    
    //read header
    for(int i=1;i<5;i++) {
      header[i] =  _HardSerial->read();
    }
    
    //check header
    if(header[0] == 0xFC && header[2] == 0x01 && header[3] == 0x30) {
      dataLength = header[4];
      
      for(int i=0;i<dataLength;i++) {
        data[i] = _HardSerial->read();
      }
  
      // read checksum byte
      data[dataLength] = _HardSerial->read();
  
      for (int i = 0; i < INFOHEADER_LEN; i++) {
        dataSum += header[i];
      }

      for (int i = 0; i < dataLength; i++) {
        dataSum += data[i];
      }
  
      checksum = (0xfc - dataSum) & 0xff;
      
      if(data[dataLength] == checksum) {
        if(packetCallback) {
          byte packet[37]; // we are going to put header[5] and data[32] into this, so the whole packet is sent to the callback
          for(int i=0; i<INFOHEADER_LEN; i++) {
            packet[i] = header[i];
          }
          for(int i=0; i<(dataLength+1); i++) { //must be dataLength+1 to pick up checksum byte
            packet[(i+5)] = data[i];
          }
          packetCallback(packet, PACKET_LEN, "packetRecv");
        }

        if(header[1] == 0x62 && data[0] == 0x02) { // setting information
          heatpumpSettings receivedSettings;
          receivedSettings.power       = lookupByteMapValue(POWER_MAP, POWER, 2, data[3]);
          receivedSettings.mode        = lookupByteMapValue(MODE_MAP, MODE, 5, data[4]);
          receivedSettings.temperature = lookupByteMapValue(TEMP_MAP, TEMP, 16, data[5]);
          receivedSettings.fan         = lookupByteMapValue(FAN_MAP, FAN, 6, data[6]);
          receivedSettings.vane        = lookupByteMapValue(VANE_MAP, VANE, 7, data[7]);
          receivedSettings.wideVane    = lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 7, data[10]);   
          
          if(settingsChangedCallback && receivedSettings != currentSettings) {
            currentSettings = receivedSettings;
            settingsChangedCallback();
          } else {
            currentSettings = receivedSettings;
          }

          // if wantedSettings is null (indicating that this is the first time we have synced with the heatpump, set it to receivedSettings
          if(!wantedSettings) {
            wantedSettings = receivedSettings;
          }

          return RCVD_PKT_SETTINGS;
        } else if(header[1] == 0x62 && data[0] == 0x03) { //Room temperature reading
          int receivedRoomTemp = lookupByteMapValue(ROOM_TEMP_MAP, ROOM_TEMP, 32, data[3]);

          if(roomTempChangedCallback && currentRoomTemp != receivedRoomTemp) {
            currentRoomTemp = receivedRoomTemp;
            roomTempChangedCallback(currentRoomTemp);
          } else {
            currentRoomTemp = receivedRoomTemp;
          }
          return RCVD_PKT_ROOM_TEMP;
        } else if(header[1] == 0x61) { //Last update was successful 
          return RCVD_PKT_UPDATE_SUCCESS;
        }
      }
    }
  }

  return RCVD_PKT_FAIL;
}


