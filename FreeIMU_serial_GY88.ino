#include <AP_Math_freeimu.h>
#include <Butter.h>    // Butterworth filter
#include <iCompass.h>

/**
 * FreeIMU library serial communication protocol
*/

#include <HMC58X3.h>
#include <HMC5883L.h>
#include <BMP085.h>
#include <MPU60X0.h>
#include <I2Cdev.h>

//#include <ADXL345.h>
//#include <HMC58X3.h>
//#include <LSM303.h>
//#include <L3G.h>
//#include <ITG3200.h>
//#include <bma180.h>
//#include <MS561101BA.h>
//#include <BMP085.h>
//#include <I2Cdev.h>
//#include <MPU60X0.h>
//#include <AK8975.h>
//#include <AK8963.h>
//#include <LPS331.h>
//#include <AP_Baro_MS5611.h>

#include <EEPROM.h>
#include <Wire.h>
#include <SPI.h>

//#define DEBUG
#include "DebugUtils.h"
#include "CommunicationUtils.h"
#include "FreeIMU.h"
#include "DCM.h"
#include "FilteringScheme.h"
#include "RunningAverage.h"
#include <MovingAvarageFilter.h>

#define HAS_GPS 1
#define BaudRate 9600

KalmanFilter kFilters[4];
int k_index = 3;

float q[4];
int raw_values[11];
float ypr[3]; // yaw pitch roll
char str[128];
float val[12];
float val_array[18]; 

// Set the FreeIMU object and LSM303 Compass
FreeIMU my3IMU = FreeIMU();

#if HAS_GPS
  #include <TinyGPS++.h>
  static const unsigned long GPSBaud = 9600;
  // The TinyGPS++ object
  TinyGPSPlus gps;
#endif

//The command from the PC
char cmd, tempCorr;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  float qVal = 0.125; //Set Q Kalman Filter(process noise) value between 0 and 1
  float rVal = 32.; //Set K Kalman Filter (sensor noise)
  
  for(int i = 0; i <= k_index; i++) { //Initialize Kalman Filters for 10 neighbors
  //KalmanFilter(float q, float r, float p, float intial_value);
    kFilters[i].KalmanInit(qVal,rVal,5.0,0.5);
  }
  
  //#if HAS_MPU6050()
  //    my3IMU.RESET();
  //#endif
	
  my3IMU.init(true);
  delay(500);
  
  #if HAS_GPS
    // Arduino Mega GPS on Serial 1
    Serial1.begin(9600);
  #endif
  
  // LED
  pinMode(13, OUTPUT);
}

void loop() {
  if(Serial.available()) {
    cmd = Serial.read();
    if(cmd=='v') {
      sprintf(str, "FreeIMU library by %s, FREQ:%s, LIB_VERSION: %s, IMU: %s", FREEIMU_DEVELOPER, FREEIMU_FREQ, FREEIMU_LIB_VERSION, FREEIMU_ID);
      Serial.print(str);
      Serial.print('\n');
    }
    else if(cmd=='1'){
      my3IMU.init(true);
    }
    else if(cmd=='2'){
      my3IMU.RESET_Q();           
    }
    else if(cmd=='g'){
      my3IMU.initGyros();
      //my3IMU.zeroGyro();      
    }
    else if(cmd=='t'){
      //available opttions temp_corr_on, instability_fix
      my3IMU.setTempCalib(1);   
    }
    else if(cmd=='f'){
      //available opttions temp_corr_on, instability_fix
      my3IMU.initGyros();
      my3IMU.setTempCalib(0);
    }
    else if(cmd=='p'){
      //set sea level pressure
      long sea_press = Serial.parseInt();        
      my3IMU.setSeaPress(sea_press/100.0);
      //Serial.println(sea_press);
    }    
    else if(cmd=='r') {
      uint8_t count = serial_busy_wait();
      for(uint8_t i=0; i<count; i++) {
        //my3IMU.getUnfilteredRawValues(raw_values);
        my3IMU.getRawValues(raw_values);
        sprintf(str, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,", raw_values[0], raw_values[1], raw_values[2], raw_values[3], raw_values[4], raw_values[5], raw_values[6], raw_values[7], raw_values[8], raw_values[9]);
        Serial.print(str);
        #if (HAS_MS5611() || HAS_BMP085() || HAS_LPS331())
          Serial.print(my3IMU.getBaroTemperature()); Serial.print(",");
          Serial.print(my3IMU.getBaroPressure()); Serial.print(",");
        #endif
        Serial.print(millis()); Serial.print(",");
        Serial.println("\r\n");
     }
    }
    else if(cmd=='b') {
      uint8_t count = serial_busy_wait();
      for(uint8_t i=0; i<count; i++) {
        #if HAS_ITG3200()
          my3IMU.acc.readAccel(&raw_values[0], &raw_values[1], &raw_values[2]);
          my3IMU.gyro.readGyroRaw(&raw_values[3], &raw_values[4], &raw_values[5]);
          writeArr(raw_values, 6, sizeof(int)); // writes accelerometer, gyro values & mag if 9150
        #elif HAS_MPU9150() || HAS_MPU9250()
          my3IMU.getRawValues(raw_values);
          writeArr(raw_values, 9, sizeof(int)); // writes accelerometer, gyro values & mag if 9150
        #elif HAS_MPU6050() || HAS_MPU6000()   // MPU6050
          my3IMU.accgyro.getMotion6(&raw_values[0], &raw_values[1], &raw_values[2], &raw_values[3], &raw_values[4], &raw_values[5]);
          writeArr(raw_values, 6, sizeof(int)); // writes accelerometer, gyro values & mag if 9150
        #elif HAS_ALTIMU10()
          my3IMU.getRawValues(raw_values);
          writeArr(raw_values, 9, sizeof(int)); // writes accelerometer, gyro values & mag of Altimu 10        
        #endif
        //writeArr(raw_values, 6, sizeof(int)); // writes accelerometer, gyro values & mag if 9150
        
        #if IS_9DOM() && (!HAS_MPU9150() && !HAS_MPU9250()&& !HAS_ALTIMU10())
          my3IMU.magn.getValues(&raw_values[0], &raw_values[1], &raw_values[2]);
          writeArr(raw_values, 3, sizeof(int));
        #endif
        Serial.println();
      }
    }
    else if(cmd == 'q') {
      uint8_t count = serial_busy_wait();
      for(uint8_t i=0; i<count; i++) {
        my3IMU.getQ(q, val);
        serialPrintFloatArr(q, 4);
        Serial.println("");
      }
    }
    else if(cmd == 'z') {
      float val_array[18] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      uint8_t count = serial_busy_wait();
      for(uint8_t i=0; i<count; i++) {
        my3IMU.getQ(q, val);
		val_array[15] = my3IMU.sampleFreq;        
        //my3IMU.getValues(val);       
        val_array[7] = (val[3] * M_PI/180);
        val_array[8] = (val[4] * M_PI/180);
        val_array[9] = (val[5] * M_PI/180);
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
	
        #if HAS_PRESS()
           // with baro
           val_array[17] = val[10];
           val_array[13] = (my3IMU.getBaroTemperature());
           val_array[14] = (my3IMU.getBaroPressure());
        #elif HAS_MPU6050()
           val_array[13] = (my3IMU.DTemp/340.) + 35.;
		#elif HAS_MPU9150()  || HAS_MPU9250()
           val_array[13] = ((float) my3IMU.DTemp) / 333.87 + 21.0;
        #elif HAS_ITG3200()
           val_array[13] = my3IMU.rt;
        #endif

        serialPrintFloatArr(val_array,18);
        //Serial.print('\n');
        
        #if HAS_GPS
          val_array[0] = (float) gps.hdop.value();
          val_array[1] = (float) gps.hdop.isValid();
          val_array[2] = (float) gps.location.lat();
          val_array[3] = (float) gps.location.lng();
          val_array[4] = (float) gps.location.isValid();
          val_array[5] = (float) gps.altitude.meters();
          val_array[6] = (float) gps.altitude.isValid();
          val_array[7] = (float) gps.course.deg();
          val_array[8] = (float) gps.course.isValid();
          val_array[9] = (float) gps.speed.kmph();
          val_array[10] = (float) gps.speed.isValid();
          val_array[11] = (float) gps.charsProcessed();
          serialPrintFloatArr(val_array,12);
          Serial.print('\n');
          smartDelay(20);
        #else
          Serial.print('\n');
        #endif        
      }
    } 
    else if(cmd == 'a') {
      float val_array[18] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      uint8_t count = serial_busy_wait();
      for(uint8_t i=0; i<count; i++) {
        my3IMU.getQ(q, val);
        val_array[15] = my3IMU.sampleFreq;
        //my3IMU.getValues(val);        
		val_array[7] = (val[3] * M_PI/180);
		val_array[8] = (val[4] * M_PI/180);
		val_array[9] = (val[5] * M_PI/180);
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

        #if HAS_PRESS()
           // with baro
           val_array[17] = val[10];
           val_array[13] = (my3IMU.getBaroTemperature());
           val_array[14] = (my3IMU.getBaroPressure());
        #elif HAS_MPU6050()
           val_array[13] = (my3IMU.DTemp/340.) + 35.;
		#elif HAS_MPU9150()  || HAS_MPU9250()
           val_array[13] = ((float) my3IMU.DTemp) / 333.87 + 21.0;
        #elif HAS_ITG3200()
           val_array[13] = my3IMU.rt;
        #endif
        serialPrintFloatArr(val_array, 18);
        //Serial.print('\n');
        
        #if HAS_GPS
          val_array[0] = (float) gps.hdop.value();
          val_array[1] = (float) gps.hdop.isValid();
          val_array[2] = (float) gps.location.lat();
          val_array[3] = (float) gps.location.lng();
          val_array[4] = (float) gps.location.isValid();
          val_array[5] = (float) gps.altitude.meters();
          val_array[6] = (float) gps.altitude.isValid();
          val_array[7] = (float) gps.course.deg();
          val_array[8] = (float) gps.course.isValid();
          val_array[9] = (float) gps.speed.kmph();
          val_array[10] = (float) gps.speed.isValid();
          val_array[11] = (float) gps.charsProcessed();
          serialPrintFloatArr(val_array,12);
          Serial.print('\n');
          smartDelay(20);
        #else
          Serial.print('\n');
        #endif 
       }
     }

    #ifndef CALIBRATION_H
    else if(cmd == 'c') {
      const uint8_t eepromsize = sizeof(float) * 6 + sizeof(int) * 6;
      while(Serial.available() < eepromsize) ; // wait until all calibration data are received
      EEPROM.write(FREEIMU_EEPROM_BASE, FREEIMU_EEPROM_SIGNATURE);
      for(uint8_t i = 1; i<(eepromsize + 1); i++) {
        EEPROM.write(FREEIMU_EEPROM_BASE + i, (char) Serial.read());
      }
      my3IMU.calLoad(); // reload calibration
      // toggle LED after calibration store.
      digitalWrite(13, HIGH);
      delay(1000);
      digitalWrite(13, LOW);
    }
    else if(cmd == 'x') {
      EEPROM.write(FREEIMU_EEPROM_BASE, 0); // reset signature
      my3IMU.calLoad(); // reload calibration
    }
    #endif
    else if(cmd == 'C') { // check calibration values
      Serial.print("acc offset: ");
      Serial.print(my3IMU.acc_off_x);
      Serial.print(",");
      Serial.print(my3IMU.acc_off_y);
      Serial.print(",");
      Serial.print(my3IMU.acc_off_z);
      Serial.print("\n");
      
      Serial.print("magn offset: ");
      Serial.print(my3IMU.magn_off_x);
      Serial.print(",");
      Serial.print(my3IMU.magn_off_y);
      Serial.print(",");
      Serial.print(my3IMU.magn_off_z);
      Serial.print("\n");
      
      Serial.print("acc scale: ");
      Serial.print(my3IMU.acc_scale_x);
      Serial.print(",");
      Serial.print(my3IMU.acc_scale_y);
      Serial.print(",");
      Serial.print(my3IMU.acc_scale_z);
      Serial.print("\n");
      
      Serial.print("magn scale: ");
      Serial.print(my3IMU.magn_scale_x);
      Serial.print(",");
      Serial.print(my3IMU.magn_scale_y);
      Serial.print(",");
      Serial.print(my3IMU.magn_scale_z);
      Serial.print("\n");
    }
    else if(cmd == 'd') { // debugging outputs
      while(1) {
        my3IMU.getRawValues(raw_values);
        sprintf(str, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,", raw_values[0], raw_values[1], raw_values[2], raw_values[3], raw_values[4], raw_values[5], raw_values[6], raw_values[7], raw_values[8], raw_values[9], raw_values[10]);
        Serial.print(str);
        Serial.print('\n');
        my3IMU.getQ(q, val);
        serialPrintFloatArr(q, 4);
        Serial.println("");
        my3IMU.getYawPitchRoll(ypr);
        Serial.print("Yaw: ");
        Serial.print(ypr[0]);
        Serial.print(" Pitch: ");
        Serial.print(ypr[1]);
        Serial.print(" Roll: ");
        Serial.print(ypr[2]);
        Serial.println("");
      }
    }
  }
}

char serial_busy_wait() {
  while(!Serial.available()) {
    ; // do nothing until ready
  }
  return Serial.read();
}

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
    Serial.println(buf);
  }
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
  #if HAS_GPS
  unsigned long start = millis();
  do 
  {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
  #endif
}
