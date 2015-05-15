#include <AP_Math_freeimu.h>
#include <Butter.h>    // Butterworth filter
#include <iCompass.h>
#include <MovingAvarageFilter.h>

/**
* FreeIMU library serial communication protocol
*/

#include <HMC58X3.h>
#include <HMC5883L.h>
#include <BMP085.h>
#include <MPU60X0.h>
#include <I2Cdev.h>

#include <Wire.h>
#include <SPI.h>

#if defined(__AVR__)
#include <EEPROM.h>
#endif

//#define DEBUG
#include "DebugUtils.h"
#include "FreeIMU_settings.h"
#include "CommunicationUtils.h"
#include "FreeIMU.h"
#include "DCM.h"
#include "FilteringScheme.h"
#include "RunningAverage.h"

// extras
#include "pitches.h"


//#define HAS_GPS 1
//
//#define gpsSerial Serial1
//#define outSerial Serial2
//
//static const long GPSBaud = 9600;
//static const long outBaudRate = 115200;


KalmanFilter kFilters[4];
int k_index = 3;

float q[4];
int raw_values[11];
float ypr[3]; // yaw pitch roll
char str[128];
float val[12];
float val_array[19];

// Set the FreeIMU object and LSM303 Compass
FreeIMU my3IMU = FreeIMU();

#if HAS_GPS
#include <TinyGPS++.h>
// The TinyGPS++ object
TinyGPSPlus gps;

// Setup GPS Serial and load config from i2c eeprom
boolean gpsStatus[] = { false, false, false, false, false, false, false };
unsigned long start;
#endif

//The command from the PC
char cmd, tempCorr;

void setup() {

	outSerial.begin(outBaudRate);
	Wire.begin();

	float qVal = 0.125; //Set Q Kalman Filter(process noise) value between 0 and 1
	float rVal = 32.; //Set K Kalman Filter (sensor noise)

	for (int i = 0; i <= k_index; i++) { //Initialize Kalman Filters for 10 neighbors
		//KalmanFilter(float q, float r, float p, float intial_value);
		kFilters[i].KalmanInit(qVal, rVal, 5.0, 0.5);
	}

	//#if HAS_MPU6050()
	//    my3IMU.RESET();
	//#endif

	my3IMU.init(true);

#if HAS_GPS
	// For Galileo,DUE and Teensy use Serial port 1
	//Load configuration from i2c eeprom - this assumes you have saved
	//a default configuration to the eeprom or permanent storage.
	//If you do not have this setup you will have to remove the
	//following lines and the additional code at the end of the sketch.
	gpsSerial.begin(9600);
	//Settings Array
	//Code based on http://playground.arduino.cc/UBlox/GPS
	byte settingsArray[] = { 0x04 }; // Not really used for this example
	configureUblox(settingsArray);
	//Retain this line
	gpsSerial.begin(GPSBaud);
#endif

	// LED
	pinMode(13, OUTPUT);
}

void loop() {


	if (outSerial.available()) {
		cmd = outSerial.read();
		if (cmd == 'v') {
			cmdGetLibVersion();
		}
		else if (cmd == '1'){
			cmdResetIMU();
		}
		else if (cmd == '2'){
			cmdZeroQ();
		}
		else if (cmd == 'g'){
			cmdZeroGyros();    
		}
		else if (cmd == 't'){
			cmdEnableTempeComp();
		}
		else if (cmd == 'f'){
			cmdDisableTempeComp();
		}
		else if (cmd == 'p'){
			cmdSeaPress();
		}
		else if (cmd == 'r') {
			cmdRawValues();
		}
		else if (cmd == 'b') {
			cmdRawCalibValues();
		}
		else if (cmd == 'q') {
			cmdGetQuat();
		}
		else if (cmd == 'z') {
			cmdGetCalib();
		}
		else if (cmd == 'a') {
			cmdGetCalibAccKalman();
		}
		else if (cmd == 'c') {
			cmdWriteCalibEprom();
		}
		else if (cmd == 'x') {
			cmdLoadCalibEprom();
		}
		else if (cmd == 'C') { // check calibration values
			cmdGetCalibData();
		}
		else if (cmd == 'd') { // debugging outputs
			cmdDebug();
		}
	}
}

char serial_busy_wait() {
	while (!outSerial.available()) {
		; // do nothing until ready
	}
	return outSerial.read();
}

#ifdef __AVR__
const int EEPROM_MIN_ADDR = 0;
const int EEPROM_MAX_ADDR = 511;

void eeprom_serial_dump_column() {
	// counter
	int i;

	// byte read from eeprom
	byte b;

	// buffer used by sprintf
	char buf[10];

	for (i = EEPROM_MIN_ADDR; i <= EEPROM_MAX_ADDR; i++) {
		b = EEPROM.read(i);
		sprintf(buf, "%03X: %02X", i, b);
		outSerial.println(buf);
	}
}
#endif

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
#if HAS_GPS
	unsigned long start = millis();
	do
	{
		while (gpsSerial.available())
			gps.encode(gpsSerial.read());
	} while (millis() - start < ms);
#endif
}

#if HAS_GPS

void configureUblox(byte *settingsArrayPointer) {
	byte gpsSetSuccess = 0;
	//outSerial.println("Configuring u-Blox GPS initial state...");

	//Generate the configuration string for loading from i2c eeprom
	byte setCFG[] = { 0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x04, 0x1E,
		0xB4 };
	calcChecksum(&setCFG[2], sizeof(setCFG) - 4);

	delay(2500);

	gpsSetSuccess = 0;
	while (gpsSetSuccess < 3) {
		//outSerial.print("Loading permanent configuration... ");
		sendUBX(&setCFG[0], sizeof(setCFG));  //Send UBX Packet
		gpsSetSuccess += getUBX_ACK(&setCFG[2]);
		//Passes Class ID and Message ID to the ACK Receive function      
		if (gpsSetSuccess == 10) gpsStatus[1] = true;
		if (gpsSetSuccess == 5 | gpsSetSuccess == 6) gpsSetSuccess -= 4;
	}
	if (gpsSetSuccess == 3) outSerial.println("Config update failed.");
	gpsSetSuccess = 0;
}

void calcChecksum(byte *checksumPayload, byte payloadSize) {
	byte CK_A = 0, CK_B = 0;
	for (int i = 0; i < payloadSize; i++) {
		CK_A = CK_A + *checksumPayload;
		CK_B = CK_B + CK_A;
		checksumPayload++;
	}
	*checksumPayload = CK_A;
	checksumPayload++;
	*checksumPayload = CK_B;
}

void sendUBX(byte *UBXmsg, byte msgLength) {
	for (int i = 0; i < msgLength; i++) {
		gpsSerial.write(UBXmsg[i]);
		gpsSerial.flush();
	}
	gpsSerial.println();
	gpsSerial.flush();
}


byte getUBX_ACK(byte *msgID) {
	byte CK_A = 0, CK_B = 0;
	byte incoming_char;
	boolean headerReceived = false;
	unsigned long ackWait = millis();
	byte ackPacket[10] = { 0xB5, 0x62, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int i = 0;
	while (1) {
		if (gpsSerial.available()) {
			incoming_char = gpsSerial.read();
			if (incoming_char == ackPacket[i]) {
				i++;
			}
			else if (i > 2) {
				ackPacket[i] = incoming_char;
				i++;
			}
		}
		if (i > 9) break;
		if ((millis() - ackWait) > 1500) {
			//outSerial.println("ACK Timeout");
			return 5;
		}
		if (i == 4 && ackPacket[3] == 0x00) {
			//outSerial.println("NAK Received");
			return 1;
		}
	}

	for (i = 2; i < 8; i++) {
		CK_A = CK_A + ackPacket[i];
		CK_B = CK_B + CK_A;
	}

	if (msgID[0] == ackPacket[6] && msgID[1] == ackPacket[7] && CK_A == ackPacket[8] && CK_B == ackPacket[9]) {
		//outSerial.println("Success!");
		//outSerial.print("ACK Received! ");
		//printHex(ackPacket, sizeof(ackPacket));
		return 10;
	}
	else {
		//outSerial.print("ACK Checksum Failure: ");
		//printHex(ackPacket, sizeof(ackPacket));
		delay(1000);
		return 1;
	}
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// COMMANDS //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// FreeIMU version
void cmdGetLibVersion(){
	sprintf(str, "FreeIMU library by %s, FREQ:%s, LIB_VERSION: %s, IMU: %s", FREEIMU_DEVELOPER, FREEIMU_FREQ, FREEIMU_LIB_VERSION, FREEIMU_ID);
	outSerial.print(str);
	outSerial.print('\n');
}

//Reset the IMU
void cmdResetIMU(){
	my3IMU.init(true);
}

//Zero the Quaternion Matrix
void cmdZeroQ(){
	my3IMU.RESET_Q();
}

//Zero gyros
void cmdZeroGyros(){
	my3IMU.initGyros();
	//my3IMU.zeroGyro();  
}

//implement temperature compensation
void cmdEnableTempeComp(){
	//available opttions temp_corr_on, instability_fix
	my3IMU.setTempCalib(1);
}

//turn off temperature compensation
void cmdDisableTempeComp(){
	//available opttions temp_corr_on, instability_fix
	my3IMU.initGyros();
	my3IMU.setTempCalib(0);
}

//set sea level pressure
void cmdSeaPress(){
	long sea_press = outSerial.parseInt();
	my3IMU.setSeaPress(sea_press / 100.0);
	//outSerial.println(sea_press);
}

//outputs raw values of the sensors
void cmdRawValues(){
	uint8_t count = serial_busy_wait();
	for (uint8_t i = 0; i<count; i++) {
		//my3IMU.getUnfilteredRawValues(raw_values);
		my3IMU.getRawValues(raw_values);
		sprintf(str, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,", raw_values[0], raw_values[1], raw_values[2], raw_values[3], raw_values[4], raw_values[5], raw_values[6], raw_values[7], raw_values[8], raw_values[9]);
		outSerial.print(str);
#if (HAS_MS5611() || HAS_BMP085() || HAS_LPS331())
		outSerial.print(my3IMU.getBaroTemperature()); outSerial.print(",");
		outSerial.print(my3IMU.getBaroPressure()); outSerial.print(",");
#endif
		outSerial.print(millis()); outSerial.print(",");
		outSerial.println("\r\n");
	}
}

//outputs raw accelerometer and magnetometer data for the calibration GUI
void cmdRawCalibValues(){
	uint8_t count = serial_busy_wait();
	for (uint8_t i = 0; i<count; i++) {
#if HAS_ITG3200()
		my3IMU.acc.readAccel(&raw_values[0], &raw_values[1], &raw_values[2]);
		my3IMU.gyro.readGyroRaw(&raw_values[3], &raw_values[4], &raw_values[5]);
		writeArr(raw_values, 6, sizeof(int)); // writes accelerometer, gyro values & mag if 9150
#elif HAS_MPU9150()  || HAS_MPU9250()
		my3IMU.getRawValues(raw_values);
		writeArr(raw_values, 9, sizeof(int)); // writes accelerometer, gyro values & mag if 9150
#elif HAS_MPU6050() || HAS_MPU6000()   // MPU6050
		//my3IMU.accgyro.getMotion6(&raw_values[0], &raw_values[1], &raw_values[2], &raw_values[3], &raw_values[4], &raw_values[5]);
		my3IMU.getRawValues(raw_values);
		writeArr(raw_values, 6, sizeof(int)); // writes accelerometer, gyro values & mag if 9150
#elif HAS_ALTIMU10()
		my3IMU.getRawValues(raw_values);
		writeArr(raw_values, 9, sizeof(int)); // writes accelerometer, gyro values & mag of Altimu 10        
#endif
		//writeArr(raw_values, 6, sizeof(int)); // writes accelerometer, gyro values & mag if 9150

#if IS_9DOM() && (!HAS_MPU9150()  && !HAS_MPU9250() && !HAS_ALTIMU10())
		my3IMU.magn.getValues(&raw_values[0], &raw_values[1], &raw_values[2]);
		writeArr(raw_values, 3, sizeof(int));
#endif
		outSerial.println();
	}
}

//outputs quaternions
void cmdGetQuat(){
	uint8_t count = serial_busy_wait();
	for (uint8_t i = 0; i<count; i++) {
		my3IMU.getQ(q, val);
		serialPrintFloatArr(q, 4);
		outSerial.println("");
	}
}

//outputs all calibrated and calculated values for use in the processing sketch
void cmdGetCalib(){
	float val_array[19] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	uint8_t count = serial_busy_wait();
	for (uint8_t i = 0; i<count; i++) {
		my3IMU.getQ(q, val);
		val_array[15] = my3IMU.sampleFreq;
		//my3IMU.getValues(val);       
		val_array[7] = (val[3] * M_PI / 180);
		val_array[8] = (val[4] * M_PI / 180);
		val_array[9] = (val[5] * M_PI / 180);
		val_array[4] = (val[0]);
		val_array[5] = (val[1]);
		val_array[6] = (val[2]);
		val_array[10] = (val[6]);
		val_array[11] = (val[7]);
		val_array[12] = (val[8]);
		val_array[0] = (q[0]);
		val_array[1] = (q[1]);
		val_array[2] = (q[2]);
		val_array[3] = (q[3]);
		//val_array[15] = millis();
		val_array[16] = val[9];
		val_array[18] = val[11];

#if HAS_PRESS()
		// with baro
		val_array[17] = val[10];
		val_array[13] = (my3IMU.getBaroTemperature());
		val_array[14] = (my3IMU.getBaroPressure());
#elif HAS_MPU6050()
		val_array[13] = (my3IMU.DTemp / 340.) + 35.;
#elif HAS_MPU9150()  || HAS_MPU9250()
		val_array[13] = ((float)my3IMU.DTemp) / 333.87 + 21.0;
#elif HAS_ITG3200()
		val_array[13] = my3IMU.rt;
#endif

		serialPrintFloatArr(val_array, 19);
		//outSerial.print('\n');

#if HAS_GPS
		val_array[0] = (float)gps.hdop.value();
		val_array[1] = (float)gps.hdop.isValid();
		val_array[2] = (float)gps.location.lat();
		val_array[3] = (float)gps.location.lng();
		val_array[4] = (float)gps.location.isValid();
		val_array[5] = (float)gps.altitude.meters();
		val_array[6] = (float)gps.altitude.isValid();
		val_array[7] = (float)gps.course.deg();
		val_array[8] = (float)gps.course.isValid();
		val_array[9] = (float)gps.speed.kmph();
		val_array[10] = (float)gps.speed.isValid();
		val_array[11] = (float)gps.charsProcessed();
		serialPrintFloatArr(val_array, 12);
		outSerial.print('\n');
		smartDelay(20);
#else
		outSerial.print('\n');
#endif        
	}
}

//outputs all calibrated and calculated values for use in the processing sketch, same as �z� except a 1d Kalman filter is applied to the accelerometer readings.
void cmdGetCalibAccKalman(){
	float val_array[19] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	uint8_t count = serial_busy_wait();
	for (uint8_t i = 0; i<count; i++) {
		my3IMU.getQ(q, val);
		val_array[15] = my3IMU.sampleFreq;
		//my3IMU.getValues(val);        
		val_array[7] = (val[3] * M_PI / 180);
		val_array[8] = (val[4] * M_PI / 180);
		val_array[9] = (val[5] * M_PI / 180);
		val_array[4] = (val[0]);
		val_array[5] = (val[1]);
		val_array[6] = (val[2]);
		val_array[10] = (val[6]);
		val_array[11] = (val[7]);
		val_array[12] = (val[8]);
		val_array[0] = kFilters[0].measureRSSI(q[0]);
		val_array[1] = kFilters[1].measureRSSI(q[1]);
		val_array[2] = kFilters[2].measureRSSI(q[2]);
		val_array[3] = kFilters[3].measureRSSI(q[3]);
		//val_array[15] = millis();
		val_array[16] = val[9];
		val_array[18] = val[11];

#if HAS_PRESS() 
		// with baro
		val_array[17] = val[10];
		val_array[13] = (my3IMU.getBaroTemperature());
		val_array[14] = (my3IMU.getBaroPressure());
#elif HAS_MPU6050()
		val_array[13] = (my3IMU.DTemp / 340.) + 35.;
#elif HAS_MPU9150()  || HAS_MPU9250()
		val_array[13] = ((float)my3IMU.DTemp) / 333.87 + 21.0;
#elif HAS_ITG3200()
		val_array[13] = my3IMU.rt;
#endif
		serialPrintFloatArr(val_array, 19);
		//outSerial.print('\n');

#if HAS_GPS
		val_array[0] = (float)gps.hdop.value();
		val_array[1] = (float)gps.hdop.isValid();
		val_array[2] = (float)gps.location.lat();
		val_array[3] = (float)gps.location.lng();
		val_array[4] = (float)gps.location.isValid();
		val_array[5] = (float)gps.altitude.meters();
		val_array[6] = (float)gps.altitude.isValid();
		val_array[7] = (float)gps.course.deg();
		val_array[8] = (float)gps.course.isValid();
		val_array[9] = (float)gps.speed.kmph();
		val_array[10] = (float)gps.speed.isValid();
		val_array[11] = (float)gps.charsProcessed();
		serialPrintFloatArr(val_array, 12);

		outSerial.print('\n');
		smartDelay(20);
#else
		outSerial.print('\n');
#endif 
	}
}

//write calibration data to the Arduino Uno EEPROM (used with the GUI)
void cmdWriteCalibEprom(){
#ifdef __AVR__
#ifndef CALIBRATION_H
	const uint8_t eepromsize = sizeof(float) * 6 + sizeof(int) * 6;
	while (outSerial.available() < eepromsize); // wait until all calibration data are received
	EEPROM.write(FREEIMU_EEPROM_BASE, FREEIMU_EEPROM_SIGNATURE);
	for (uint8_t i = 1; i<(eepromsize + 1); i++) {
		EEPROM.write(FREEIMU_EEPROM_BASE + i, (char)outSerial.read());
	}
	my3IMU.calLoad(); // reload calibration
	// toggle LED after calibration store.
	digitalWrite(13, HIGH);
	delay(1000);
	digitalWrite(13, LOW);

#endif
#endif
}

//reload calibration
void cmdLoadCalibEprom(){
#ifdef __AVR__
#ifndef CALIBRATION_H
	EEPROM.write(FREEIMU_EEPROM_BASE, 0); // reset signature
	my3IMU.calLoad(); // reload calibration
#endif
#endif
}

//lists the calibration data
void cmdGetCalibData(){
	outSerial.print("acc offset: ");
	outSerial.print(my3IMU.acc_off_x);
	outSerial.print(",");
	outSerial.print(my3IMU.acc_off_y);
	outSerial.print(",");
	outSerial.print(my3IMU.acc_off_z);
	outSerial.print("\n");

	outSerial.print("magn offset: ");
	outSerial.print(my3IMU.magn_off_x);
	outSerial.print(",");
	outSerial.print(my3IMU.magn_off_y);
	outSerial.print(",");
	outSerial.print(my3IMU.magn_off_z);
	outSerial.print("\n");

	outSerial.print("acc scale: ");
	outSerial.print(my3IMU.acc_scale_x);
	outSerial.print(",");
	outSerial.print(my3IMU.acc_scale_y);
	outSerial.print(",");
	outSerial.print(my3IMU.acc_scale_z);
	outSerial.print("\n");

	outSerial.print("magn scale: ");
	outSerial.print(my3IMU.magn_scale_x);
	outSerial.print(",");
	outSerial.print(my3IMU.magn_scale_y);
	outSerial.print(",");
	outSerial.print(my3IMU.magn_scale_z);
	outSerial.print("\n");
}

//debugging data output
void cmdDebug(){
	while (1) {
		my3IMU.getRawValues(raw_values);
		sprintf(str, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,", raw_values[0], raw_values[1], raw_values[2], raw_values[3], raw_values[4], raw_values[5], raw_values[6], raw_values[7], raw_values[8], raw_values[9], raw_values[10]);
		outSerial.print(str);
		outSerial.print('\n');
		my3IMU.getQ(q, val);
		serialPrintFloatArr(q, 4);
		outSerial.println("");
		my3IMU.getYawPitchRoll(ypr);
		outSerial.print("Yaw: ");
		outSerial.print(ypr[0]);
		outSerial.print(" Pitch: ");
		outSerial.print(ypr[1]);
		outSerial.print(" Roll: ");
		outSerial.print(ypr[2]);
		outSerial.println("");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
