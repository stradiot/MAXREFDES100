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
#include "mbed.h"
#include "MAX14720.h"
#include "MAX30101.h"
#include "LIS2DH.h"
#include "Peripherals.h"
#include "MAX30101_helper.h"
#include "PushButton.h"
#include "BLE.h"
#include "HspBLE.h"

/// define the HVOUT Boost Voltage default for the MAX14720 PMIC
#define HVOUT_VOLTAGE 4500 // set to 4500 mV

/// define all I2C addresses
#define MAX14720_I2C_SLAVE_ADDR (0x54)
#define MAX30101_I2C_SLAVE_ADDR (0xAE)
#define LIS2DH_I2C_SLAVE_ADDR (0x32)

///
/// wire Interfaces
///
/// I2C Master 2
I2C i2c2(I2C2_SDA, I2C2_SCL); // used by MAX14720, MAX30101, LIS2DH
///
/// Devices
///
/// Accelerometer
LIS2DH lis2dh(&i2c2, LIS2DH_I2C_SLAVE_ADDR);
InterruptIn lis2dh_Interrupt(P4_7);
/// PMIC
MAX14720 max14720(&i2c2, MAX14720_I2C_SLAVE_ADDR);
/// Optical Oximeter
MAX30101 max30101(&i2c2, MAX30101_I2C_SLAVE_ADDR);
InterruptIn max30101_Interrupt(P4_0);
/// Packet TimeStamp Timer, set for 1uS
Timer timestampTimer;
/// HSP Platform push button
PushButton pushButton(SW1);

/// BLE instance
static BLE ble;

/// HSP BluetoothLE specific functions
HspBLE hspBLE(&ble);

int main() {
  // hold results for returning funtcoins
  int result;

  // display start banner
  printf("Maxim Integrated mbed hSensor 3.0.0 10/14/16\n");
  fflush(stdout);

  // initialize HVOUT on the MAX14720 PMIC
  printf("Init MAX14720...\n");
  fflush(stdout);
  result = max14720.init();
  if (result == MAX14720_ERROR)
    printf("Error initializing MAX14720");
  max14720.boostEn = MAX14720::BOOST_ENABLED;
  max14720.boostSetVoltage(HVOUT_VOLTAGE);

  // set NVIC priorities for GPIO to prevent priority inversion
  printf("Init NVIC Priorities...\n");
  fflush(stdout);
  NVIC_SetPriority(GPIO_P0_IRQn, 5);
  NVIC_SetPriority(GPIO_P1_IRQn, 5);
  NVIC_SetPriority(GPIO_P2_IRQn, 5);
  NVIC_SetPriority(GPIO_P3_IRQn, 5);
  NVIC_SetPriority(GPIO_P4_IRQn, 5);
  NVIC_SetPriority(GPIO_P5_IRQn, 5);
  NVIC_SetPriority(GPIO_P6_IRQn, 5);
  
  // Be able to statically reference these devices anywhere in the application
  Peripherals::setLIS2DH(&lis2dh);
  Peripherals::setTimestampTimer(&timestampTimer);
  Peripherals::setMAX30101(&max30101);
  Peripherals::setI2c2(&i2c2);
  Peripherals::setPushButton(&pushButton);
  Peripherals::setBLE(&ble);
  Peripherals::setMAX14720(&max14720);
  Peripherals::setHspBLE(&hspBLE);

  // Initialize BLE base layer
  printf("Init HSPBLE...\n");
  fflush(stdout);
  hspBLE.init();

  // MAX30101 initialize interrupt
  printf("Init MAX30101 callbacks, interrupt...\n");
  fflush(stdout);
  max30101.onInterrupt(&MAX30101_OnInterrupt);
  max30101_Interrupt.fall(&MAX30101::MidIntHandler);

  // initialize the LIS2DH accelerometer and interrupts
  printf("Init LIS2DH interrupt...\n");
  fflush(stdout);
  lis2dh.init();
  lis2dh_Interrupt.fall(&LIS2DHIntHandler);
  lis2dh_Interrupt.mode(PullUp);

  // start main loop
  printf("Start main loop...\n");
  fflush(stdout);
  
  max30101.HRmode_init(0, 0, 1, 3, 32);
  lis2dh.initStart(5, 8);
  
  while (1)
    ble.waitForEvent();
}
