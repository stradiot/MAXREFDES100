/*******************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *******************************************************************************
 */
#include "HspBLE.h"
#include "Peripherals.h"

/// define all of the characteristic UUIDs
uint8_t HspBLE::accelerometerCharUUID[] = {0xe6,0xc9,0xda,0x1a,0x80,0x96,0x48,0xbc,0x83,0xa4,0x3f,0xca,0x38,0x37,0x05,0xaf};
uint8_t HspBLE::heartrateCharUUID[] = {0x62,0x1a,0x00,0xe3,0xb0,0x93,0x46,0xbf,0xaa,0xdc,0xab,0xe4,0xc6,0x48,0xc5,0x69};
uint8_t HspBLE::commandCharUUID[] = {0x36,0xe5,0x5e,0x37,0x6b,0x5b,0x42,0x0b,0x91,0x07,0x0d,0x34,0xa0,0xe8,0x67,0x5a};


/// define the BLE device name
uint8_t HspBLE::deviceName[] = "MAXREFDES100";
/// define the BLE serial number
uint8_t HspBLE::serialNumber[] = {0xA5, 0x3B, 0x51, 0x00, 0x00, 0x03};
/// define the BLE service UUID
uint8_t HspBLE::envServiceUUID[] = {0x5c,0x6e,0x40,0xe8,0x3b,0x7f,0x42,0x86,0xa5,0x2f,0xda,0xec,0x46,0xab,0xe8,0x51};

HspBLE *HspBLE::instance = NULL;

/**
* @brief Constructor that inits the BLE helper object
* @param ble Pointer to the mbed BLE object
*/
HspBLE::HspBLE(BLE *ble) {
  bluetoothLE = new BluetoothLE(ble, NUMBER_OF_CHARACTERISTICS);
  instance = this;
  notificationUpdateRoundRobin = 0;
}

/**
* @brief Constructor that deletes the bluetoothLE object
*/
HspBLE::~HspBLE(void) { delete bluetoothLE; }

/**
* @brief Initialize all of the HSP characteristics, initialize the ble service
* and attach callbacks
*/
void HspBLE::init(void) {
  bluetoothLE->addCharacteristic(new Characteristic(6 /* number of bytes */,accelerometerCharUUID,GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY));
  bluetoothLE->addCharacteristic(new Characteristic(1 /* number of bytes */,heartrateCharUUID,GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY));
  bluetoothLE->addCharacteristic(new Characteristic(1 /* number of bytes */,commandCharUUID,GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE));
  bluetoothLE->initService(serialNumber, deviceName, sizeof(deviceName),envServiceUUID);

  bluetoothLE->onDataWritten(&HspBLE::_onDataWritten);
  ticker.attach(callback(this, &HspBLE::tickerHandler), 1);
}

void HspBLE::_onDataWritten(int index) {
  HspBLE::instance->onDataWritten(index);
}

/**
* @brief Callback for written characteristics
* @param index Index of whose characteristic is written to
*/
void HspBLE::onDataWritten(int index) {
  int length;
  uint8_t *data;
  
  if (index == CHARACTERISTIC_CMD) {
    data = bluetoothLE->getDataWritten(index, &length);
    if (length == 1) {
	  if (data[0] == 0x00) {
		  printf("00 received\n");
		  Peripherals::max30101()->HRmode_stop();
	  }
	  else if (data[0] == 0x01) {
	    printf("01 received\n");
		Peripherals::max30101()->HRmode_init(0, 0, 1, 3, 32);
	  }
	  else {
		printf("unknown command %d received\n", data[0]);    		
      }
	  printf("onDataWritten index %d, data %02X, length %d \n",index,data[0],length); 
	  fflush(stdout);
    }
  }
}

void HspBLE::pollSensor(int sensorId, uint8_t *data) {
  switch (sensorId) {
    case CHARACTERISTIC_ACCELEROMETER: {
      unsigned int i;
      uint8_t *bytePtr;
      int16_t acclPtr[3];
      Peripherals::lis2dh()->get_motion_cached(&acclPtr[0], &acclPtr[1],
                                             &acclPtr[2]);
      bytePtr = reinterpret_cast<uint8_t *>(&acclPtr);
      for (i = 0; i < sizeof(acclPtr); i++)
        data[i] = bytePtr[i];    
    } break;
    case CHARACTERISTIC_HEARTRATE: {
      int8_t heartrate;
      Peripherals::max30101()->ReadHeartrateData(&heartrate);
      data[0] = heartrate;
    } break;
  }
}

bool HspBLE::getStartDataLogging(void) { return startDataLogging; }

/**
* @brief Timer Callback that updates all sensor characteristic notifications
*/
void HspBLE::tickerHandler(void) {
  uint8_t data[8];
  if (bluetoothLE->isConnected()) {
    pollSensor(CHARACTERISTIC_ACCELEROMETER, data);
    bluetoothLE->notifyCharacteristic(CHARACTERISTIC_ACCELEROMETER, data);
    pollSensor(CHARACTERISTIC_HEARTRATE, data);
    bluetoothLE->notifyCharacteristic(CHARACTERISTIC_HEARTRATE, data);
  }
}

