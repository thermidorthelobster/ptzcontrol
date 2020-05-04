#include <BMDSDIControl.h>
#include <include/BMDSDIControlShieldRegisters.h>
#include <Streaming.h>

const int shieldAddress = 0x6E;
BMD_SDICameraControl_I2C sdiCameraControl(shieldAddress);

/* Constants */
const byte          kBufferSize     = kRegICDATA_Width;
const int           kHeaderLen      = 4;
const int           kAlignment      = 4;

/* Define pin mappings */
const int pinLeft = 3;
const int pinRight = 4;
const int pinUp = 5;
const int pinDown = 6;
const int pinUDRate = 10;     // use this pin for tilt speed or single-speed controller
const int pinLRRate = 11;     // use this pin for pan speed if using dual-speed controller, otherwise ignored
const int rateOverride = 12;  // use this pin if constant max speed required for single-speed controller
const int joyX(A0);           // pins for joystick analogue inputs
const int joyY(A1);
const int joyPress(2);        // pin for joystick button press - must be interrupt compatible

/* joystick setup */
const int anaMax = 1023;      // max analogue value output by joystick (depending on 8 or 10 bit analogue sampling)
const int anaScalefactor = (anaMax + 1) / 256;
unsigned long modeChangetime;
const int deBouncetime = 500;  // debounce interval for interrupt trigger
int udRate;
int lrRate;
int joyXdefault;
int joyYdefault;
int joyPressed;
volatile boolean mode(true); // default mode is joystick control; change to False for default ATEM control

void setup() {
  Serial.begin(9600);

  Serial << "Starting camera control...\n";
  sdiCameraControl.begin();

  Wire.setClock(400000);
  sdiCameraControl.setOverride(false);

  pinMode(pinLeft, OUTPUT);
  pinMode(pinRight, OUTPUT);
  pinMode(pinUp, OUTPUT);
  pinMode(pinDown, OUTPUT);
  pinMode(pinUDRate, OUTPUT);
  pinMode(pinLRRate, OUTPUT);
  pinMode(rateOverride, OUTPUT);

  digitalWrite(pinLeft, LOW);
  digitalWrite(pinRight, LOW);
  digitalWrite(pinUp, LOW);
  digitalWrite(pinDown, LOW);
  analogWrite(pinUDRate, LOW);
  analogWrite(pinLRRate, LOW);
  analogWrite(rateOverride, HIGH);

  joyXdefault = analogRead(joyX);   // finds the default x/y value for joystick without movement - usually not the exact midpoint. Assumes joystick not disturbed when unit powered on. If it is, reset the unit.
  joyYdefault = analogRead(joyY);

  Serial.print("X def: ");
  Serial.println(joyXdefault);
  Serial.print("Y def: ");
  Serial.println(joyYdefault);

  pinMode(joyPress, INPUT_PULLUP);  // these lines configure the interrupt for joystick button press to change modes
  attachInterrupt(digitalPinToInterrupt(joyPress), changeMode, LOW);

  Serial << "Setup Done\n\n";
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  switch (mode) {
    case false:
      atemControl();
      break;
    case true:
      joyControl();
      break;
  }
}

void changeMode() {
  // interrupt triggered - change mode
  if (millis() >= modeChangetime + deBouncetime) {
    Serial.print("Changing mode to ");
    mode = !mode;

    digitalWrite(pinLeft, LOW);
    digitalWrite(pinRight, LOW);
    digitalWrite(pinUp, LOW);
    digitalWrite(pinDown, LOW);
    analogWrite(pinUDRate, LOW);
    analogWrite(pinLRRate, LOW);

    switch (mode) {
      case false:
        Serial.println("ATEM Mode");
        break;
      case true:
        Serial.println("Joystick Control Mode");
        break;
    }
    modeChangetime = millis();
  }
}

void atemControl() {
  static unsigned long aliveTimerVal = 0;
  static int lineNumber = 0;

  byte rxData[kBufferSize];
  byte* command = nullptr;
  int dataLen;

  int ptzID = 11;
  int upDown = 0;
  int leftRight = 0;
  int udRate = 0;
  int lrRate = 0;

  // Read the data from the device. Add 1 to compensate for 3G-SDI Shield Read issue.
  dataLen = sdiCameraControl.read(rxData, kBufferSize) + 1;

  //initialise the first command to the start of the rxData payload
  command = rxData;

  if (dataLen >= 4)
  {
    /* -------------- For debugging -------------- */
    //     printDataDec(rxData, dataLen);     // Print entire payload in decimal format
    //     printDataHex(rxData, dataLen);     // Print entire payload in hex format

    //    Serial << "---- Received " << dataLen << " bytes of data. \n";

    // loop through each command in the packet received
    while (command < rxData + dataLen)
    {
      static int prevDestination = 0;

      int destination = command[0];
      int length = command[1];
      int cmdID = command[2];
      int reserved = command[3];
      int category = command[4];
      int param = command[5];
      int type = command[6];
      byte op1 = command[7];  // apparently always 0
      byte op2 = command[8];  // these two bytes are x value
      byte op3 = command[9];
      byte op4 = command[10]; // these two bytes are y value
      byte op5 = command[11];
      int16_t xInput = op2 + (int8_t) op3 * 256;
      int16_t yInput = op4 + (int8_t) op5 * 256;

      // print result to Serial
      //      Serial << "(" << lineNumber++ << ") \t";
      //      Serial << "ID [" << category << "." << param << "]: \t";
      //      printType(type);
      //
      //      Serial << "len=" << length - 3 << "\t";
      //      Serial << "Data = ";
      //      printCmdData(command, length - kHeaderLen + 1);

      // Determine how far the pointer needs to increment
      int multiplier = length / kAlignment + 1;
      if (length % kAlignment != 0)
        length = kAlignment * multiplier;     // Is the length already divisible by 4?

      // Increment the command pointer to the position of the next command
      command += length + kHeaderLen;

      // Is the command PTZ?
      if (category = ptzID) {
        if (param == 0) {                // case 11.0
          Serial.print("x:");
          Serial.print(xInput);
          Serial.print("  y:");
          Serial.println(yInput);

          if (xInput > 0) {                   // right
            lrRate = (abs (xInput) - 1) / 8;  // reduce range from -2048+2048 to 0-255
            analogWrite(pinLRRate, lrRate);
            digitalWrite(pinLeft, LOW);
            digitalWrite(pinRight, HIGH);
            Serial.print("Left ");
            Serial.println(lrRate);
          } else if (xInput < 0) {            // left
            lrRate = (abs (xInput) - 1) / 8 ;  // reduce range from -2048+2048 to 0-255
            analogWrite(pinLRRate, lrRate);
            digitalWrite(pinRight, LOW);
            digitalWrite(pinLeft, HIGH);
            Serial.print("Right ");
            Serial.println(lrRate);
          } else {
            Serial.println("Centre");
            digitalWrite(pinRight, LOW);
            digitalWrite(pinLeft, LOW);
          }

          if (yInput < 0) {                   // up
            udRate = (abs (yInput) - 1) / 8;  // reduce range from -2048+2048 to 0-255
            analogWrite(pinUDRate, udRate);
            digitalWrite(pinDown, LOW);
            digitalWrite(pinUp, HIGH);
            Serial.print("Up ");
            Serial.println(udRate);
          } else if (yInput > 0) {            // down
            udRate = (abs (yInput) - 1) / 8;  // reduce range from -2048+2048 to 0-255
            analogWrite(pinUDRate, udRate);
            digitalWrite(pinUp, LOW);
            digitalWrite(pinDown, HIGH);
            Serial.print("Down ");
            Serial.println(udRate);
          } else {
            Serial.println("Middle");
            analogWrite(pinUDRate, lrRate);   // if tilt axis not in use, use speed controller for pan axis in single-speed mode
            digitalWrite(pinUp, LOW);
            digitalWrite(pinDown, LOW);
          }
        }
      }
    }
  }
}

void joyControl() {
  int xVal = 0;
  int yVal = 0;
  int xShifted;
  int yShifted;
  float xScalefactor;
  float yScalefactor;
  int xOutput = 0;
  int yOutput = 0;

  xVal = analogRead(joyX);
  yVal = analogRead(joyY);

  if (xVal < joyXdefault - 10) {
    // Move left
    xShifted = xVal - joyXdefault;
    xScalefactor = float(anaMax) / float(joyXdefault);
    xOutput = abs(xScalefactor * xShifted / anaScalefactor);

    Serial.print("joyXdefault: ");
    Serial.println(joyXdefault);
    Serial.print("xVal: ");
    Serial.println(xVal);
    Serial.print("xShifted: ");
    Serial.println(xShifted);
    Serial.print("xScalefactor: ");
    Serial.println(xScalefactor);
    Serial.print("xOutput: ");
    Serial.println(xOutput);

    analogWrite(pinLRRate, xOutput);  // pinLRrate when separate
    digitalWrite(pinRight, LOW);
    digitalWrite(pinLeft, HIGH);
  } else if (xVal > joyXdefault + 10) {
    // Move right
    xShifted = xVal - joyXdefault;
    xScalefactor = float(anaMax) / (float(anaMax) - float(joyXdefault));
    xOutput = abs(xScalefactor * xShifted / anaScalefactor);

    Serial.print("joyXdefault: ");
    Serial.println(joyXdefault);
    Serial.print("xVal: ");
    Serial.println(xVal);
    Serial.print("xShifted: ");
    Serial.println(xShifted);
    Serial.print("xScalefactor: ");
    Serial.println(xScalefactor);
    Serial.print("xOutput: ");
    Serial.println(xOutput);

    analogWrite(pinLRRate, xOutput);  // pinLRRate when separate
    digitalWrite(pinLeft, LOW);
    digitalWrite(pinRight, HIGH);
  } else {
    analogWrite(pinLRRate, 0);
    digitalWrite(pinLeft, LOW);
    digitalWrite(pinRight, LOW);
  }

  if (yVal < joyYdefault - 15) {
    // Move down
    yShifted = yVal - joyYdefault;
    yScalefactor = float(anaMax) / float(joyYdefault);
    yOutput = abs(yScalefactor * yShifted / anaScalefactor);

    Serial.print("joyYdefault: ");
    Serial.println(joyYdefault);
    Serial.print("yVal: ");
    Serial.println(yVal);
    Serial.print("yShifted: ");
    Serial.println(yShifted);
    Serial.print("yScalefactor: ");
    Serial.println(yScalefactor);
    Serial.print("yOutput: ");
    Serial.println(yOutput);

    analogWrite(pinUDRate, yOutput);
    digitalWrite(pinUp, LOW);
    digitalWrite(pinDown, HIGH);
  } else if (yVal > joyYdefault + 15) {
    // Move up
    yShifted = yVal - joyYdefault;
    yScalefactor = float(anaMax) / (float(anaMax) - float(joyYdefault));
    yOutput = abs(yScalefactor * yShifted / anaScalefactor);

    Serial.print("joyYdefault: ");
    Serial.println(joyYdefault);
    Serial.print("yVal: ");
    Serial.println(yVal);
    Serial.print("yShifted: ");
    Serial.println(yShifted);
    Serial.print("yScalefactor: ");
    Serial.println(yScalefactor);
    Serial.print("yOutput: ");
    Serial.println(yOutput);

    analogWrite(pinUDRate, yOutput);  // pinLRRate when separate
    digitalWrite(pinDown, LOW);
    digitalWrite(pinUp, HIGH);
  } else {
    analogWrite(pinUDRate, xOutput);   // if tilt axis not in use, use speed controller for pan axis in single-speed mode
    digitalWrite(pinUp, LOW);
    digitalWrite(pinDown, LOW);
  }
}

/* Debugging code from here onwards */

// print the command data from the packet in hex
void printCmdData(const byte* data, const int len )
{

  Serial << "[";
  for (int i = 0; i < len; i++)
  {
    if (data[i + 7] < 0x10u)
      Serial.print("0");

    Serial.print(data[i + 7], HEX);

    if (i != len - 1)
      Serial.print(" ");
  }
  Serial << "]" << endl;

}

// print the type of the operation
void printType(const int type)
{
  switch (type)
  {
    case 0:
      Serial << "void\t";
      break;

    case 1:
      Serial << "int8\t";
      break;

    case 2:
      Serial << "int16\t";
      break;

    case 3:
      Serial << "int32\t";
      break;

    case 4:
      Serial << "int64\t";
      break;

    case 5:
      Serial << "string\t";
      break;

    case 128:
      Serial << "fixed16\t";
      break;

    default:
      Serial << "Unknown Type\t";
  }
}

// print byte array data as decimal
void printDataDec(const byte* data, const int len)
{
  for (int i = 0; i < len; i++)
  {
    Serial << data[i];
    if (i != len - 1)
      Serial << " ";

  }
  Serial.print("\n");
}

// print byte array data as hexidecimal
void printDataHex(const byte* data, const int len)
{
  for (int i = 0; i < len; i++)
  {
    if (data[i] < 0x10u)
      Serial.print("0");

    Serial.print(data[i], HEX);
    if (i != len - 1)
      Serial << " ";

  }
  Serial.print("\n");
}
