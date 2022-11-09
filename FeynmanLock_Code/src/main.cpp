#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <EEPROM.h>
#include <Adafruit_Fingerprint.h>

#define RST_PIN 9 // Configurable, see typical pin layout above
#define SS_PIN 10 // Configurable, see typical pin layout above
#define Servo_PIN 6
#define Button_PIN 4
#define LockButton_PIN 8
#define UnlockButton_PIN 7
#define AdminLED_PIN 5

#define Unlocked_State 1
#define Lock_Angle 175
#define Unlock_Angle 50
#define Rest_Angle 120
int ValidFingerIDs[25] ={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,22,23,25,27,30};
int AdminFingrIDs[20]={2,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
bool adminProtocol = false;
//23: Jonathan, 27: Adam, 30: Bryce

#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
// For UNO and others without hardware serial, we must use software serial...
// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
// Set up the serial port to use softwareserial..
SoftwareSerial mySerial(2, 3);

#else
// On Leonardo/M0/etc, others with hardware serial, use hardware serial!
// #0 is green wire, #1 is white
#define mySerial Serial1

#endif

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
Servo lockMotor;
bool evaluatingCard=false;

void setup()
{
  pinMode(Button_PIN, INPUT_PULLUP);
  pinMode(LockButton_PIN, INPUT_PULLUP);
  pinMode(UnlockButton_PIN, INPUT_PULLUP);
  pinMode(AdminLED_PIN, OUTPUT);
  adminProtocol=EEPROM.read(1);
  if (adminProtocol)
    digitalWrite(AdminLED_PIN,HIGH);
  else
    digitalWrite(AdminLED_PIN, LOW);
  lockMotor.attach(Servo_PIN);
  int initState = EEPROM.read(0);
  int initAngle =initState;
  if(initState ==0 || initState == 1){
    if(initState == 0){
      initAngle = Unlock_Angle;
      lockMotor.write(initAngle);
    }
    if(initState==1){ //If in memory it stored 1 it's going to lock, if not it's going to unlock
      initAngle=Lock_Angle;
      lockMotor.write(initAngle);
    }
    delay(1000);
    EEPROM.update(0,3);
    Serial.println("Finalized writing to memory 3");
    lockMotor.write(Rest_Angle);
    delay(600);
  }
  if(initState == 3){
    initAngle = Rest_Angle;
    lockMotor.write(initAngle);
  }
  delay(1000);

  Serial.begin(9600); 
  /*------------- Fingerprint Setup ---------------*/
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
  }
  finger.getParameters();//Get paramters, printing them is optional
  finger.getTemplateCount();//To check if the fingerprint sensor contains data
  if (finger.templateCount == 0) {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  }
  /*-----------------RFID Setup-------------------*/
  // Initialize serial communications with the PC
  while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();			// Init SPI bus
  mfrc522.PCD_Init();		// Init MFRC522
  delay(4);				// Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
}

String GetUID(MFRC522::Uid uid)
{
  String rec_uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    if (mfrc522.uid.uidByte[i] < 0x10)
    {
      rec_uid += F(" 0");
    }
    else
    {
      rec_uid += F(" 0");
    }
    rec_uid += mfrc522.uid.uidByte[i];
  }
  return rec_uid;
}

bool AuthorizeAccess(String uid)
{
  if (uid==" 0227 0119 09 022" || uid==" 0209 057 0163 068")
  {
    return true;
  }
  else
  {
    return false;
  }
}

void MoveLock(int pressed, bool unlock)
{
  lockMotor.attach(Servo_PIN);
  if(pressed==Unlocked_State && !unlock){ //If it's unlocked and the request is to lock
    EEPROM.update(0,1);
    Serial.println("Finalized writing to memory 1");
    lockMotor.write(Lock_Angle);
    delay(1000);
    EEPROM.update(0,3);
    Serial.println("Finalized writing to memory 3");
    lockMotor.write(Rest_Angle);
    delay(600);
    lockMotor.detach();
  }
  else if(pressed!=Unlocked_State && unlock){ //If it's locked and the request is to unlock
    EEPROM.update(0,0);
    Serial.println("Finalized writing to memory 0");
    lockMotor.write(Unlock_Angle);
    delay(1000);
    EEPROM.update(0,3);
    Serial.println("Finalized writing to memory 3");
    lockMotor.write(Rest_Angle);
    delay(600);
    lockMotor.detach();
  }
  else if((pressed==Unlocked_State && unlock)||(pressed!=Unlocked_State && !unlock)){
    Serial.print("Request already done");
  }
  evaluatingCard=false;
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}
//NOTE: Must find alternative to button press since lock doesn't fully press it
bool PressedButton =false;
void loop()
{
  /*-------------------- Admin Button Checks------------------------*/
  int lockCommand = digitalRead(LockButton_PIN);
  int unlockCommand = digitalRead(UnlockButton_PIN);
  if(lockCommand==0 && unlockCommand==1){
    Serial.print("\nLocking Door");
    int buttonPress = digitalRead(Button_PIN);
    MoveLock(buttonPress, false);//Locking door so false unlock
    return;
  }
  if(unlockCommand==0 && lockCommand==1){
    Serial.print("\nUnlocking door");
    // int button2Press = digitalRead(Button_PIN);
    MoveLock(0, true);
    return;
  }
  if(unlockCommand==0 && lockCommand==0 && PressedButton==false){
    PressedButton=true;
    Serial.print("\nAdmin Protocol Toggle\n");
    adminProtocol=!adminProtocol;
    EEPROM.update(1,adminProtocol);
    if (adminProtocol)
      digitalWrite(AdminLED_PIN,HIGH);
    else
      digitalWrite(AdminLED_PIN, LOW);
    return;
  }
  if(unlockCommand==1 && lockCommand==1 && PressedButton==true){
    PressedButton=false; 
  }
  /*---------------------Check For Fingerprint----------------*/
  int FingerId = getFingerprintIDez();
  if(FingerId != -1){
    bool valid = false;
    adminProtocol=EEPROM.read(1);
    Serial.print("\nAdminProtocol: ");
    if(adminProtocol==false){
      Serial.print("Inactive\n");
      for(int k=0; k<sizeof(ValidFingerIDs)/sizeof(int); k++){
        if(FingerId == ValidFingerIDs[k]){
          valid=true;
          break;
        }
      }
    }
    if(adminProtocol==true){
      Serial.print("Active\n");
      for(int k=0; k<sizeof(AdminFingrIDs)/sizeof(int); k++){
        if(FingerId == AdminFingrIDs[k]){
          valid=true;
          break;
        }
      }
    }
    if(valid==true){
      Serial.print("\nWelcome boss :)\n");
        MoveLock(0, true);
    }
  }
  /*-----------------------Check For Card -------------------*/
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    delay(50);
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    delay(50);
    return;
  }
  Serial.print("Card\n");
  if(evaluatingCard){
    Serial.print("Wait evaluating card\n");
    delay(1000);
    return;
  }
  evaluatingCard=true;
  Serial.print(F("Card UID:"));
  String rec_uid = GetUID(mfrc522.uid);
  Serial.print(rec_uid);
  bool auth = AuthorizeAccess(rec_uid);
  if (auth)
  {
    Serial.print("\nWelcome boss baby :)");
    int buttonPress = digitalRead(Button_PIN);
    MoveLock(0, true);
  }
  else
  {
    Serial.print("\nGet out Eric!\n");
  }
  delay(50);
}