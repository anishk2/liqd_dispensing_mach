/*
   Semi-Automatic Bottle filling machine
   Allows user calibration of 3 modes
   Resolution +/- 2grams at 10SPS for medianCount n = 3 and pump Vf = 12V

   Version 1.3
   IMP: Update version in void.setup() after every change

   Written By:
   Anish Krishnakumar
   28 April 2021
   Updated on 2 May 2021 to include inspect contents feature and running median

   Press and hold both buttons while switching on to enter calibration mode
   Press and hold any one of the buttons while switching on to inspect EEPROM contents

   Press MODE button to select mode. Each mode is associated with a certain volume
   which can be changed in VOLUME[] array

   Press DISPENSE button to start dispensing

*/

#include <Arduino.h>
#include <HX711.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

#define DISPENSE 2
#define MODE 3
#define RELAY_PIN 4

void control(long localVal);
void updateMode(int localIndex);
void calibrateFunction(int localIndex);
void EEPROMWrite(int address, long value);
long EEPROMRead(long address);
int selection();
void inspectContents();
byte DebounceSwitch();


const int LOADCELL_DOUT = 5;
const int LOADCELL_SCK = 6;

const int rs = A1, en = A3, d4 = A4, d5 = A5, d6 = 7, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

HX711 scale;
//Initialize val[] to values;
long val[] = {220000, 240000, 250000, -1};
int address[] = {0, 4, 8};
int VOLUME[] = {200, 450, 900};
byte index = 0;
int selectedMode = 0;

void setup() {

  Serial.begin(9600);
  scale.begin(LOADCELL_DOUT, LOADCELL_SCK);
  pinMode(DISPENSE, INPUT_PULLUP);
  pinMode(MODE, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(A7, INPUT);
  pinMode(A6, INPUT);
  pinMode(A2, OUTPUT);
  digitalWrite(A2, LOW);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.println("Dispense machine  ");
  lcd.setCursor(0, 1);
  lcd.println("V1.3  ");
  delay(800);
  lcd.clear();

  //If both buttons are pressed while switching on, enter calibration mode
  if (digitalRead(DISPENSE) == 0 && digitalRead(MODE) == 0) {
    selectedMode = selection();
    Serial.print("Selected mode is ");
    Serial.print(selectedMode + 1);
    calibrateFunction(selectedMode);
  }
  //Set threshold value to the value saved in EEPROM 
  for (byte i = 0; i < 3; i++) {
    val[i] = EEPROMRead(address[i]);
    Serial.println(val[i]);
  }

  //If any one of the buttons is pressed while switching on, enter inspection mode
  if ((digitalRead(MODE)^digitalRead(DISPENSE)) == 1) {
    inspectContents();
  }
  updateMode(index);
}

void loop() {

  int switchState = DebounceSwitch();
  long reading = scale.read() * -1;
  Serial.print("HX711 reading: ");
  Serial.print(reading);
  Serial.print("\t");
  Serial.print("Mode:");
  Serial.print(index + 1);
  Serial.print("\t");
  Serial.println(val[index]);
  if (digitalRead(DISPENSE) == 0 && reading < val[index]) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dispensing");
    lcd.setCursor(0, 1);
    lcd.print(VOLUME[index]);
    lcd.print("               ");
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(RELAY_PIN, HIGH);
    control(val[index]);
  }
  if (switchState == 1) {
    index++;
    if (index > 3) {
      index = 0;
    }
    updateMode(index);
    switchState = 0;
  }
}
/*
  Turns off the relay and indicator light when scale reads value defined by the argument.
  The median value of 'n' readings is considered to avoid stray readings
  INPUTS:
    Scale reading after which relay must turn off
  OUTPUTS:
    Nil
*/
void control(long localVal) {
  long medianValue;
  long temp = 0;
  byte n = 3; // Enter the length of median array.Always use odd numbers
  long medianArray[n];
  if(localVal != -1){
    do {
    for (byte m = 0; m < n; m++) {
      //Log three readings into median array
      medianArray[m] = -1 * scale.read();
    }
    //Re-arrange median array in ascending order
    for (byte  s = 0; s < n - 1; s++) {
      for (byte t = 0; t < n - s - 1; t++) {
        if (medianArray[t] > medianArray[t + 1]) {
          temp = medianArray[t];
          medianArray[t] = medianArray[t + 1];
          medianArray[t + 1] = temp;
        }
      }
    }
    for (byte x = 0; x < n; x++) {
      Serial.print("Array ");
      Serial.print(x);
      Serial.print(":\t");
      Serial.println(medianArray[x]);
    }
    //The median value is the (n+1)/2th term of the array
    medianValue = medianArray[(n + 1) / 2];
    Serial.print("Median : ");
    Serial.print(medianValue);
    Serial.print("\tDifference: ");
    Serial.println(localVal - medianValue);
    } while (localVal - medianValue > 0);     //If the scale reads less than threshold
  }
  
  else{
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Manual mode");
    lcd.setCursor(0,1);
    lcd.print("Dispensing");
    while(digitalRead(DISPENSE) == 0){};
  }
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  lcd.clear();
  updateMode(index);
}

/*
  Prints the currently set volume to the LCD. This function displays the 'home' screen
  INPUTS:
    Index of currently set mode
  OUTPUTS:
    Nil
*/
void updateMode(int localIndex) {
  if(localIndex == 3){
    lcd.setCursor(0,0);
    lcd.print("Manual Mode     ");
    lcd.setCursor(0,1);
    lcd.print("Press to Change");
  }
  else{
    lcd.setCursor(0, 0);
    lcd.print("Volume: ");
    lcd.print(VOLUME[localIndex]);
    lcd.print("  mL      ");
    lcd.setCursor(0, 1);
    lcd.print("Press to change");
  }
  
}
/*
  Records the scale reading while user fills the container to desired level, and saves this value to
  the EEPROM location indexed by the argument.
  INPUTS:
    Index of the currently set mode
  OUTPUTS:
    Nil
*/
void calibrateFunction(int localIndex) {
  long localValue = val[localIndex];
  int flag = 0;
  lcd.setCursor(0, 0);
  lcd.print("Begin Calibration");
  lcd.setCursor(0, 1);
  lcd.print("Volume: ");
  lcd.print(VOLUME[localIndex]);
  delay(2000);
  lcd.clear();
  lcd.println("Place container   ");
  delay(2000);
  lcd.clear();
  lcd.print("Press VOL button   ");
  lcd.setCursor(0, 1);
  lcd.print("to fill     ");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Release    ");
  lcd.setCursor(0, 1);
  lcd.print("to save value  ");
  delay(2000);
  while (flag == 0) {
    while (digitalRead(MODE) == 0) {
      digitalWrite(LED_BUILTIN, HIGH);
      digitalWrite(RELAY_PIN, HIGH);
      localValue = scale.get_units(5) * -1;
      Serial.println(localValue);
      if (flag == 0) {
        flag = 1;
      }
    }
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(RELAY_PIN, LOW);
  }
  lcd.clear();
  lcd.print("Done");
  lcd.setCursor(0, 1);
  lcd.print("Saved: ");
  lcd.print(localValue);
  Serial.print("Value saved is: ");
  Serial.println(localValue);
  delay(2500);
  lcd.clear();
  EEPROMWrite(address[localIndex], localValue);
  //return localValue;
}
/*
  Writes the value of long datatype to EEPROM location whose address is defined by the argument
  INPUTS:
    Address of the EEPROM location to which data should be saved
    Value of 'long' datatype which should be saved to EEPROM
  OUTPUTS:
    Nil
*/

void EEPROMWrite(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}
/*
  Reads the value of long datatype at EEPROM location whose address is defined by the argument
  INPUTS:
    Address of the EEPROM location of which data should be read
  OUTPUTS:
    Value saved at EEPROM location defined by argument
*/

long EEPROMRead(long address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}
/*
  Select the mode which should be calibrated
  INPUTS:
    Nil
  OUTPUTS:
    The mode which should be calibrated
*/
int selection() {
  int selectionFlag = 0;
  int selectionIndex = 0;
  byte localSwitchState = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Entering Calib");
  while (digitalRead(DISPENSE) == 0 || digitalRead(MODE) == 0) {};
  lcd.setCursor(0, 1);
  lcd.print("Choose Volume");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press DISPENSE ");
  lcd.setCursor(0, 1);
  lcd.print("to confirm ");
  delay(2000);
  updateMode(selectionIndex);
  while (selectionFlag == 0) {
    localSwitchState = DebounceSwitch();
    if (digitalRead(DISPENSE) == 0) {
      selectionFlag = 1;
    }
    updateMode(selectionIndex);
    if (localSwitchState == 1) {
      //delay(15);
      selectionIndex++;
      if (selectionIndex > 2) {
        selectionIndex = 0;
      }
    }
    localSwitchState = 0;
  }
  lcd.clear();
  return selectionIndex;
}

/*
  Inspect the contents of the EEPROM and see scale reading
  INPUTS:
    Nil
  OUTPUTS:
    Nil
*/
void inspectContents() {
  int inspectFlag = 0;
  int inspectIndex = 0;
  byte inspectSwitchState = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inspect Contents ");
  delay(2000);
  lcd.setCursor(0, 0);
  lcd.print("Use VOL button   ");
  lcd.setCursor(0, 1);
  lcd.print("to toggle");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Use DISP button");
  lcd.setCursor(0, 1);
  lcd.print("to exit");
  delay(2000);
  lcd.clear();
  while (inspectFlag == 0) {
    inspectSwitchState = DebounceSwitch();
    if (inspectSwitchState == 1) {
      inspectIndex++;
      if (inspectIndex > 3) {
        inspectIndex = 0;
      }
    }
    if (inspectIndex < 3) {
      lcd.setCursor(0, 0);
      lcd.print("VOLUME: ");
      lcd.print(VOLUME[inspectIndex]);
      lcd.print("         ");
      lcd.setCursor(0, 1);
      lcd.print(val[inspectIndex]);
      lcd.print("     ");
    }
    else {
      lcd.setCursor(0, 0);
      lcd.print("Current val:     ");
      lcd.setCursor(0, 1);
      lcd.print(scale.read() * -1);
      lcd.print("        ");
    }
    if (digitalRead(DISPENSE) == 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Exiting...");
      delay(500);
      inspectFlag = 1;
    }
    inspectSwitchState = 0;
  }
  lcd.clear();
}

//Debounce Mode Switch
//Method found in https://my.eng.utah.edu/%7Ecs5780/debouncing.pdf
byte DebounceSwitch() {
  static uint16_t State = 0; // Current debounce status
  State = (State << 1) | !digitalRead(MODE) | 0xe000;
  if (State == 0xf000)return 1;
  return 0;
}


