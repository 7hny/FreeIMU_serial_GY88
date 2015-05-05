/**
Visualize a cube which will assumes the orientation described
in a quaternion coming from the serial port.

INSTRUCTIONS: 
This program has to be run when you have the FreeIMU_serial
program running on your Arduino and the Arduino connected to your PC.
Remember to set the serialPort variable below to point to the name the
Arduino serial port has in your system. You can get the port using the
Arduino IDE from Tools->Serial Port: the selected entry is what you have
to use as serialPort variable.


Copyright (C) 2011-2012 Fabio Varesano - http://www.varesano.net/

This program is free software: you can redistribute it and/or modify
it under the terms of the version 3 GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

import processing.serial.*;
import processing.opengl.*;
import controlP5.*;
ControlP5 cp5;

Serial myPort;  // Create object from Serial class

//setting a 1D Kalman filter
MyKalman pressK = new MyKalman();

//Settup Stop Watch
StopWatchTimer sw = new StopWatchTimer();

// These are needed for the moving average calculation
float[] data = new float[32];
float total = 0, average = 0;
int p = 0, n = 0;

//LPF
float filterFactor = 0.05;
float heading_f = 0.;

//Moving average Heading
float corr_heading;
float heading_avg;
float heading = 0;
float oldHeading = 0.0;
int windSize = 96;
MovingAverage HeadingAvg = new MovingAverage(windSize);

//set motiondetect types
float accnorm,accnorm_var_test;
int accnorm_test, omegax, omegay, omegaz, omega_test, motionDetect;

//Movingaverage filters for motion detection
MovingAverage accnorm_test_avg = new MovingAverage(5);
MovingAverage accnorm_var = new MovingAverage(7);
MovingAverage motion_detect_ma = new MovingAverage(7);
MovingAverage accnorm_avg = new MovingAverage(5);

final String serialPort = "COM4"; // replace this with your serial port. On windows you will need something like "COM1".

float [] q = new float [4];
float [] acc = new float [3];
float [] gyro = new float [3];
float [] magn = new float [3];
float [] ypr = new float [3];
float temp; float press; float altitude; 
float EstAlt;
float dt;
float tnew;
float told = 0;

// Altitude - Accel Complimentary filter setup
float[] dyn_acc = new float[3];
float fused_alt;
Quaternion dyn_acc_q;
Quaternion q1;
Quaternion multQ;
Quaternion dyn_acc_q_earth;
Quaternion conQ;
AltitudeComplementary altitudeFilter = new AltitudeComplementary();

// Sphere Variables
float R = 150;
int xDetail = 40;
int yDetail = 30;
float[] xGrid = new float[xDetail+1];
float[] yGrid = new float[yDetail+1];
float[][][] allPoints = new float[xDetail+1][yDetail+1][3];

// Rotation Variables
float camDistance = -90;
float rotationX = 100;
float rotationY = 170;
float velocityX = 0;
float velocityY = 0;

// Texture
PImage texmap, Aimage, cmpRng, cmpAP;
float angx, angy, angz, angyLevelControl;

float S;
float A;

float sea_press = 1013.25;            //Input local sea level pressure
float declinationAngle = -13.1603;   //Flushing, NY magnetic declination in degrees
float SEA_PRESS  = 1013.25;          //default sea level pressure level in mb
float KNOWNALT   = 65.0;            //default known altitude, 
float INHG       = 0.02952998751;    //convert mb to in/Hg constant
float MB         = 33.8638815;       //convert in/Hg to mb constant
float FTMETERS   = 0.3048;
float METERS2FT  = 3.2808399;
float PI         = 3.14159;
float deg2rads   = PI/180;
int calib = 1;
int ArtHorFlg = 0;

//FreeIMY setup
float [] hq = null;
float [] Euler = new float [3]; // psi, theta, phi

int lf = 10; // 10 is '\n' in ASCII
byte[] inBuffer = new byte[22]; // this is the number of chars on each line from the Arduino (including /r/n)

//---------------------------------------------------
// types needed for ODO implementation
//
float Sample_X;
float Sample_Y; 
float Sample_Z; 
long [] Sensor_Data = new long [8]; 
short countx; short county ;

float [] accelerationx = new float [2];
float [] accelerationy = new float [2];
float [] velocityx = new float [2];
float [] velocityy = new float [2];
float [] positionX= new float [2]; 
float [] positionY = new float [2]; 
float [] positionZ = new float [2]; 
long direction; 
float sstatex; float sstatey;

// Switch for ODO
int cube_odo = 0;

//-------------------------------------
//
PFont font, font8, font9, font12, font15, letters, lcd;

final int VIEW_SIZE_X = 1000, VIEW_SIZE_Y = 600;
int xCompass    = 865;        int yCompass    = 365;
int xLevelObj   = 723;        int yLevelObj   = 90+0;

final int burst = 32;
int count = 0;

void myDelay(int time) {
  try {
    Thread.sleep(time);
  } catch (InterruptedException e) { }
}

void setup() 
{
  size(VIEW_SIZE_X, VIEW_SIZE_Y, OPENGL);
  //frameRate(120);
  
  // The font must be located in the sketch's "data" directory to load successfully
  font = loadFont("CourierNew36.vlw"); 
  lcd = loadFont("LiquidCrystal-128.vlw");
  font8 = createFont("Arial bold",8,false);
  font9 = createFont("Arial bold",9,false);
  font12 = createFont("Arial bold",12,false);
  font15 = createFont("Arial bold",15,false);

  //Quaternion library calls for complimentary altitude filter  
  dyn_acc_q = new Quaternion();
  q1 = new Quaternion();
  multQ = new Quaternion();
  dyn_acc_q_earth = new Quaternion();
  conQ = new Quaternion();

  cp5 = new ControlP5(this);
  cp5.setControlFont(font,18);
  cp5.setColorValue(color(255, 255, 0));
  cp5.setColorLabel(color(255, 255, 0));
  
  setValues();

  //setup attitdude indicator
  noStroke();
  imageMode(CENTER);
  cmpRng = loadImage("CompassRing.PNG");
  cmpAP  = loadImage("CompassAP.PNG");
  texmap = loadImage("sphere_bckgrnd.png");
  Aimage = loadImage("AttInd.PNG");
  setupSphere(R, xDetail, yDetail);
  
  //serial port set up
  myPort = new Serial(this, serialPort, 38400);

  //elapsed time start call
  sw.start();
  
  println("Waiting IMU..");

  while (myPort.available() > 0) {
    myPort.write("v");
    myPort.write("1");
    myDelay(1000);
  }
  //println(myPort.readStringUntil('\n'));
  myPort.write("z" + char(burst));
  myPort.bufferUntil('\n');
  
}

float decodeFloat(String inString) {
  byte [] inData = new byte[4];
  
  if(inString.length() == 8) {
    inData[0] = (byte) unhex(inString.substring(0, 2));
    inData[1] = (byte) unhex(inString.substring(2, 4));
    inData[2] = (byte) unhex(inString.substring(4, 6));
    inData[3] = (byte) unhex(inString.substring(6, 8));
  }
      
  int intbits = (inData[3] << 24) | ((inData[2] & 0xff) << 16) | ((inData[1] & 0xff) << 8) | (inData[0] & 0xff);
  return Float.intBitsToFloat(intbits);
}

void serialEvent(Serial p) {
  if(p.available() >= 17) {
    String inputString = p.readStringUntil('\n');
    //print(inputString);
    if (inputString != null && inputString.length() > 0) {
      String [] inputStringArr = split(inputString, ",");
      if(inputStringArr.length >= 17) { // q1,q2,q3,q4,\r\n so we have 5 elements
        q[0] = decodeFloat(inputStringArr[0]);
        q[1] = decodeFloat(inputStringArr[1]);
        q[2] = decodeFloat(inputStringArr[2]);
        q[3] = decodeFloat(inputStringArr[3]);
	acc[0] = decodeFloat(inputStringArr[4]);
	acc[1] = decodeFloat(inputStringArr[5]);
	acc[2] = decodeFloat(inputStringArr[6]);
	gyro[0] = decodeFloat(inputStringArr[7]);
	gyro[1] = decodeFloat(inputStringArr[8]);
	gyro[2] = decodeFloat(inputStringArr[9]);
	magn[0] = decodeFloat(inputStringArr[10]);
	magn[1] = decodeFloat(inputStringArr[11]);		
	magn[2] = decodeFloat(inputStringArr[12]);
	temp = decodeFloat(inputStringArr[13]);
	press = decodeFloat(inputStringArr[14]);
        tnew = decodeFloat(inputStringArr[15]);
        heading = decodeFloat(inputStringArr[16]);
        dt = tnew - told;
        told = tnew;
        //getYawPitchRollRad();
      }
    }
    count = count + 1;
    if(burst == count) { // ask more data when burst completed
      //1 = RESET MPU-6050, 2 = RESET Q Matrix
      if(key == '2') {
         myPort.clear();
         myPort.write("2");
         sw.start();
         println("pressed 2");
         key = '0';
      } else if(key == '1') {
            myPort.clear();
            myPort.write("1");
            sw.start();
            println("pressed 1");
            key = '0';
      } else if(key == 'r') {
            myPort.clear();
            ArtHorFlg = 0;
            calib = 1;
            sea_press = 1013.25;
            setup();
  }
      if(calib == 0) {
         myPort.clear();
         myPort.write("f");
         sw.start();
         calib = 99;
      }
      if(calib == 1) {
         myPort.clear();
         myPort.write("t");
         sw.start();
         calib = 99;
      }      
      myDelay(100);
      p.write("z" + char(burst));
      count = 0;
    }
  }
}


void buildBoxShape() {
  //box(60, 10, 40);
  noStroke();
  beginShape(QUADS);

  //Z+ (to the drawing area)
  fill(#00ff00);
  vertex(-30, -5, 20);
  vertex(30, -5, 20);
  vertex(30, 5, 20);
  vertex(-30, 5, 20);
  
  //Z-
  fill(#0000ff);
  vertex(-30, -5, -20);
  vertex(30, -5, -20);
  vertex(30, 5, -20);
  vertex(-30, 5, -20);
  
  //X-
  fill(#ff0000);
  vertex(-30, -5, -20);
  vertex(-30, -5, 20);
  vertex(-30, 5, 20);
  vertex(-30, 5, -20);
  
  //X+
  fill(#ffff00);
  vertex(30, -5, -20);
  vertex(30, -5, 20);
  vertex(30, 5, 20);
  vertex(30, 5, -20);
  
  //Y-
  fill(#ff00ff);
  vertex(-30, -5, -20);
  vertex(30, -5, -20);
  vertex(30, -5, 20);
  vertex(-30, -5, 20);
  
  //Y+
  fill(#00ffff);
  vertex(-30, 5, -20);
  vertex(30, 5, -20);
  vertex(30, 5, 20);
  vertex(-30, 5, 20);
  
  endShape();
}


void drawCube() {  
    pushMatrix();
    translate(VIEW_SIZE_X/2, VIEW_SIZE_Y/2 + 150, +80);
    scale(2,2,2);
    // a demonstration of the following is at 
    // http://www.varesano.net/blog/fabio/ahrs-sensor-fusion-orientation-filter-3d-graphical-rotating-cube
    rotateZ(-Euler[2]);
    rotateX(-Euler[1]+radians(17));
    rotateY(-Euler[0]);
    
    buildBoxShape();
    
  popMatrix();
}


void draw() {
  //background(#585858);
  background(#000000);
  textFont(font, 18);
  textAlign(LEFT, TOP);
  strokeWeight(3);
  fill(#ffffff);
  if(hq != null) { // use home quaternion
    quaternionToEuler(quatProd(hq, q), Euler);
    text("Disable home position by pressing \"n\"", 20, VIEW_SIZE_Y - 30);
 }
  else {
    quaternionToEuler(q, Euler);
    text("Point FreeIMU's X axis to your monitor then press \"h\"", 20, VIEW_SIZE_Y - 30);
  }

  fill(#FFFF00);
  float press1 = pressK.update(press);
  altitude = ((pow((sea_press / press1), 1/5.257) - 1.0) * (temp + 273.15)) / 0.0065;
  altitude = altitude + 36/METERS2FT;
  EstimatedAltitude();
  text("Temp: " + temp + "\n" + "Press: " + press + "\n" , 20, VIEW_SIZE_Y - 110);
  textFont(font,24);
  text("ALT:",(VIEW_SIZE_X/2)-75,40); 
  fill(#00CF00);
  textFont(font,36);
  text(nfp((fused_alt),3,2),(VIEW_SIZE_X/2)-75,70); 
  textFont(font,18);
  fill(#ffff00);
  text("DeltaT: " + dt, 180, VIEW_SIZE_Y - 110);
  
  text("Q:\n" + q[0] + "\n" + q[1] + "\n" + q[2] + "\n" + q[3], 20, 20);

  textFont(font, 24);
  text("Pitch:\n", xLevelObj-50, yLevelObj + 45);
  text("Roll:\n", xLevelObj-50, yLevelObj + 105);
  text("Yaw:\n", xLevelObj-50, yLevelObj + 165);
  text("Heading:\n",850,255);
  
  //text(nfp(degrees(Euler[1]),3,2) + "  " + nfp(degrees(Euler[2]),3,2), xLevelObj + 85, yLevelObj + 125);
  getYawPitchRollRad();
  
  fill(#00CF00);
  //text(nfp(degrees(Euler[1]),3,2), xLevelObj-40, yLevelObj + 75);
  //text(nfp(degrees(Euler[2]),3,2), xLevelObj-40, yLevelObj + 135);
  //text(nfp(degrees(Euler[0]),3,2), xLevelObj-40, yLevelObj + 195);
  text(nfp(degrees(ypr[1]),3,2), xLevelObj-40, yLevelObj + 75);
  text(nfp(degrees(ypr[2]),3,2), xLevelObj-40, yLevelObj + 135);
  text(nfp(degrees(Euler[0]),3,2), xLevelObj-40, yLevelObj + 195);
  
  textFont(font, 18);
  fill(#FFFF00);
  noFill();
  stroke(204, 102, 0);
  rect(10, 17, 145, 95, 7);
  //angx = Euler[2];
  //angy = Euler[1];
  angx = ypr[2];
  angy = ypr[1];
  
  float head1 = iround(heading,1);
  corr_heading = clamp360(head1+declinationAngle);
  HeadingAvg.newNum(corr_heading);
  //HeadingAvg.newNum(HeadingAvgCorr(corr_heading, oldHeading));
  //oldHeading = corr_heading;
  corr_heading = HeadingAvg.getAvg();
  //text("Heading " + nfp(((corr_heading)),4,1),400,20);
  //buildCompass();
  rotComp();
  
  textFont(font, 24);
  fill(#00CF00);
  text(nfp(((corr_heading)),4,1),850,285);
  textFont(font, 18);
  fill(#FFFF00);
  
  noFill();
  stroke(204, 102, 0);
  rect(10, 125, 145, 85, 7);  
  text("Acc:\n" + nfp(acc[0],1,6) + "\n" + nfp(acc[1],1,6) + "\n" + nfp(acc[2],1,6) + "\n", 20, 130);
  //rect(10, 210, 145, 85, 7); 
  //text("Gyro:\n" + nfp(gyro[0],1,6) + "\n" + nfp(gyro[1],1,6) + "\n" + nfp(gyro[2],1,6) + "\n", 20, 220);
  rect(170, 20, 145, 85, 7);
  text("Gyro:\n" + nfp(gyro[0],1,6) + "\n" + nfp(gyro[1],1,6) + "\n" + nfp(gyro[2],1,6) + "\n", 180, 25);
  //rect(10, 295, 145, 90, 7); 
  //text("Magn:\n" + nfp(magn[0],1,6) + "\n" + nfp(magn[1],1,6) + "\n" + nfp(magn[2],1,6) + "\n", 20, 310);
  rect(170, 125, 145, 85, 7); 
  text("Magn:\n" + nfp(magn[0],1,6) + "\n" + nfp(magn[1],1,6) + "\n" + nfp(magn[2],1,6) + "\n", 180, 130);
  
  //text(MotionDetect(),VIEW_SIZE_X-75,VIEW_SIZE_Y-75) ;
  if(MotionDetect() > 0 ){
    fill(#FF0000);
  } else {
    fill(#FFFFFF)
  ; }
  rect(VIEW_SIZE_X-75,VIEW_SIZE_Y-55,35,35);

  if(cube_odo == 0) { 
	drawCube(); }
    else {
	position();
        text("px:  " + positionX[0] + "\n" + "py:  " + positionY[0], 200, 200);
   }
  
  textFont(font,20);
  fill(#ffff00);
  text("Elapsed\n" + "Time", 680,40);   
  textFont(lcd, 48);
  fill(#ff0000);
  text(sw.hour() + ":" + sw.minute() + ":" + sw.second(), 823, 40);
  
  if(ArtHorFlg == 1) {
      NewHorizon();
  }

}


void keyPressed() {
  if(key == 'h') {
    println("pressed h");
    // set hq the home quaternion as the quatnion conjugate coming from the sensor fusion
    hq = quatConjugate(q);
    sw.start();
  }
  else if(key == 'n') {
    println("pressed n");
    hq = null;
  }
  else if(key == 's') {
    println("pressed s"); 
    sw.start();
  }
  else if(key == 'q') {
    exit();
  }
}

// See Sebastian O.H. Madwick report 
// "An efficient orientation filter for inertial and intertial/magnetic sensor arrays" Chapter 2 Quaternion representation

void quaternionToEuler(float [] q, float [] euler) {
  euler[0] = atan2(2 * q[1] * q[2] - 2 * q[0] * q[3], 2 * q[0]*q[0] + 2 * q[1] * q[1] - 1); // psi
  euler[1] = -asin(2 * q[1] * q[3] + 2 * q[0] * q[2]); // theta
  euler[2] = atan2(2 * q[2] * q[3] - 2 * q[0] * q[1], 2 * q[0] * q[0] + 2 * q[3] * q[3] - 1); // phi
}

float [] quatProd(float [] a, float [] b) {
  float [] q = new float[4];
  
  q[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
  q[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
  q[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
  q[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
  
  return q;
}

// returns a quaternion from an axis angle representation
float [] quatAxisAngle(float [] axis, float angle) {
  float [] q = new float[4];
  
  float halfAngle = angle / 2.0;
  float sinHalfAngle = sin(halfAngle);
  q[0] = cos(halfAngle);
  q[1] = -axis[0] * sinHalfAngle;
  q[2] = -axis[1] * sinHalfAngle;
  q[3] = -axis[2] * sinHalfAngle;
  
  return q;
}

// return the quaternion conjugate of quat
float [] quatConjugate(float [] quat) {
  float [] conj = new float[4];
  
  conj[0] = quat[0];
  conj[1] = -quat[1];
  conj[2] = -quat[2];
  conj[3] = -quat[3];
  
  return conj;
}

void getYawPitchRollRad() {
  //float q[4]; // quaternion
  float gx, gy, gz; // estimated gravity direction
  
  gx = 2 * (q[1]*q[3] - q[0]*q[2]);
  gy = 2 * (q[0]*q[1] + q[2]*q[3]);
  gz = q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3];
  
  ypr[0] = atan2(2 * q[1] * q[2] - 2 * q[0] * q[3], 2 * q[0]*q[0] + 2 * q[1] * q[1] - 1);
  ypr[1] = atan(gx / sqrt(gy*gy + gz*gz));
  ypr[2] = atan(gy / sqrt(gx*gx + gz*gz));
}

//=============================================================
void gravityCompensateDynAcc() {
  float[] g = new float[3];
  
  // get expected direction of gravity in the sensor frame
  g[0] = 2 * (q[1] * q[3] - q[0] * q[2]);
  g[1] = 2 * (q[0] * q[1] + q[2] * q[3]);
  g[2] = q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3];
  
  // compensate accelerometer readings with the expected direction of gravity
  dyn_acc[0] = acc[0] - g[0];
  dyn_acc[1] = acc[1] - g[1];
  dyn_acc[2] = acc[2] - g[2];
}
 

//=============================================================
// converted from Michael Shimniok Data Bus code
// http://mbed.org/users/shimniok/code/AVC_20110423/

float clamp360(float x) {
    while ((x) >= 360.0) (x) -= 360.0; 
    while ((x) < 0) (x) += 360.0; 
    return x;
}



//==============================================================
//
float HeadingAvgCorr(float newx, float oldx) {
    while ((newx + 180.0) < oldx) (newx) += 360.0;
    while ((newx - 180.0) > oldx) (newx) -= 360.0;
    while ((newx) == 360.0) (newx) = 0.0;
    return newx;
}

//============================================================
void EstimatedAltitude() {
  
  gravityCompensateDynAcc();
  dyn_acc_q.x = dyn_acc[0];
  dyn_acc_q.y = dyn_acc[1];  
  dyn_acc_q.z = dyn_acc[2];
  dyn_acc_q.w = 0;
  q1.x = q[1]; q1.y = q[2]; q1.z = q[3]; q1.w = q[0];
  multQ = Quaternion.multiply(q1, dyn_acc_q);
  //conQ = Quaternion.conjugate(q1);
  conQ.x = -q1.x; conQ.y = -q1.y; conQ.z = -q1.z; conQ.w = q1.w;
  dyn_acc_q_earth = Quaternion.multiply(multQ, conQ);
  fused_alt = altitudeFilter.update(dyn_acc_q_earth.z, altitude, dt);
  
}
//==============================================================
//SMA filter
// Use the next value and calculate the 
// moving average 
public void AddNewValue(float value){
  total -= data[p];
  data[p] = value;
  total += value;
  p = ++p % data.length;
  if(n < data.length) n++;
  average = total / n;
} 

//=======================================
public float iround(float number, float decimal) {
  int ix;
  ix = round(number*pow(10, decimal));
  return float(ix)/pow(10, decimal);
}


