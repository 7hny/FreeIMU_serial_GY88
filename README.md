# FreeIMU_serial_GY88
Arduino MEGA 2560 + FreeIMU + GY-88 + NEO6mV2 + LC-05

Dependencies:
https://github.com/mjs513/FreeIMU-Updates

### Testing the FreeIMU:

* Connect the Arduino to your computer.

* Launch the Arduino Editor program and open the "FreeIMU serial" sketch under "File -> Examples -> FreeIMU -> FreeIMU_serial"  if you are using a Arduino or the “FreeIMU_ FreeIMU_serial_ARM_CPU” if you are using one of the ARM based CPU.

* Select your board under Tools -> Board.

* Select the serial port on which the Arduino is connected (Tools -> Serial Port) and write down the name of the port (Windows something like COM2, Mac something like /dev/tty.usbmodem1421). 

* Upload the sketch

* To make a quick test to check that the Arduino responds, open the Serial Monitor (Tools -> Serial Monitor), check that the properties of the communication are "Newline" and "57600  Baud". Put “v” in the input field and press "Send" button. If everything is ok, the Arduino will send you back:

_FreeIMU library by Fabio Varesano - varesano.net, FREQ:16 MHz, LIB_VERSION: DEV, IMU: FreeIMU v0.4_
_NOTE: there may be a delay until the MPU resets – do not move the IMU if are using the Invensene MPUs._

* You can play with others commands to see the response of the Arduino, here is the current list

      “1’ = Reset the IMU

      “2” = Zero the Quaternion Matrix

      “g” = Zero Gyros

      “t” = implement temperature compensation

      “f” = turn off temperature compensation

      “pxxxxx” = input sea level reference pressure in milibars*100, i.e. p101325 for 1013.25 mb which is the default if you do not input anything

      “r” = outputs raw values of the sensors

      
      
      Ax, Ay, Ax, Gx, Gy, Gz, Mx, My, Mz, ??, pressure, temp (if you have a pressure sensor attached), time in milliseconds

      “b” = outputs raw accelerometer and magnetometer data for the calibration GUI

      “q” = outputs quaternions

      “z” = outputs all calibrated and calculated values for use in the processing sketch

      
      
      Gx, Gy, Gz, Ax, Ay, Az, Mx, My, Mz, q0, q1, q2, q3, MPU temp, heading, pressure, atms temp or sensor temp if no pressure sensor, 10 values of GPS data if selected

      “a” = same as “z” except a 1d Kalman filter is applied to the accelerometer readings.

      “c” = write calibration data to the Arduino Uno EEPROM (used with the GUI)

      “x” = reload calibration

      “C” = lists the calibration data

      “d” = debugging data output

* The application that comes along with the FreeIMU library to test if your FreeIMU is working properly is "FreeIMU_cube_Odo.pde". This is a Processing sketch, so you will need to download Processing version 2.0b7 to run it. The GUI of processing is really similar to the Arduino Editor. Open " FreeIMU_cube_Odo.pde " with processing, it is located under: " /processing/FreeIMU_cube/FreeIMU_cube_ODO.pde".  Suggest that you use the latest version in the Experimental folder.

We need to tell the program on which port the Arduino is attached to. To do so find the code written below in the Processing's code:



_final String serialPort = "xxxxxxxxxxxxxxx"; // replace this with your serial port. On windows you will need something like "COM1". XXXXXX is variable depending on port. Currently defaults to a com port_
_also_

_myPort = new Serial(this, serialPort, 57600);  //change baudrate to match the FreeIMU.ino sketch baudrate selected_

_Replace "/dev/ttyUSB9" by the serial port name you wrote down before (the port name the Arduino is attached to)._

* Run the program, if everything goes well you should see a Window with a 3D cube on it. The cube represent your FreeIMU so when you move your FreeIMU, the cube should follow the mouvement. If it is not the case, don't panic! We must calibrate the FreeIMU.

* If you get some errors or if the program doesn't start (just a void window), try to reset your Arduino and relaunch the program. If it doesn't work please check the troubleshooting section on the FreeIMU website.
Alternately, would suggest that you use “FreeIMU_cube_Odo_Exp_v2.pde” in the “experimental/processing directory”.  Better visualization and additional options which we will get into later.
