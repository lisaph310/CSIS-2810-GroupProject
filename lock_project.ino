#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices

LiquidCrystal_I2C lcd(0x3F, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {2, 3, 4, 5}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {6, 7, 8}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

boolean match = false;          // initialize card match to false
boolean programMode = false;  // initialize programming mode to false

uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader

byte readCard[4];   // Stores scanned ID read from RFID Module
byte storedCard[4];   // Stores an ID read from EEPROM

// Create MFRC522 instance.
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);

  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware

  lcd.init();                      // initialize the lcd
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Menu: Press '*' to wipe records, '1' to enter read mode, or '2' to enter program mode.");

  Serial.println(F("Access Control v0.1"));
  ShowReaderDetails(); // Show details of PCD - MFRC522 Card Reader details
  char key = keypad.waitForKey();
  if (key == '*') {  // when button pressed pin should get low, button connected to ground
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    lcd.setCursor(0,0);
    lcd.print("Wipe Mode:")
    lcd.setCursor(0,1);
    lcd.print("This action will remove all records and cannot be undone");
    lcd.print("To confirm, press '3');
    key = keypad.waitForKey();
    if (key == '3') {    // If button still be pressed, wipe EEPROM
      lcd.setCursor(0,0);
      lcd.print("Wiping records . . .");
      Serial.println(F("Starting Wiping EEPROM"));
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
        if (EEPROM.read(x) == 0) {              //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          EEPROM.write(x, 0);       // if not write 0 to clear, it takes 3.3mS
        }
      }
      Serial.println(F("EEPROM Successfully Wiped"));
      lcd.print("Records successfully wiped");
    }
    else {
      Serial.println(F("Wiping Cancelled"));
      lcd.setCursor(0,0);
      lcd.print("Wipe cancelled");
    }
  } else if (key == '2') {
    lcd.setCursor(0,0);
    lcd.print("Program Mode:");
    lcd.setCursor(0,1);
    lcd.print("Scan a device to ADD or REMOVE");
    Serial.println(F("Entering Program Mode"));
    Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
    programMode = true;
  } else if (key == '1'){
    lcd.setCursor(0,0);
    lcd.print("Read Mode: Scan friend and enter.");
    Serial.println(F("Entering Read Mode"));
  }
}

void loop() {
  do {
    successRead = getID();
  } while (!successRead);
  if (programMode) {
    if ( findID(readCard) ) { // If scanned card is known delete it
      lcd.setCursor(0,0);
      lcd.print("I know this device, removing . . . ");
      Serial.println(F("I know this PICC, removing..."));
      deleteID(readCard);
      lcd.setCursor(0,1);
      lcd.print("Scan a device to ADD or REMOVE");
      Serial.println("-----------------------------");
      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
    }
    else {                    // If scanned card is not known add it
      lcd.setCursor(0,0);
      lcd.print(""I do not know this PICC, adding...");
      Serial.println(F("I do not know this PICC, adding..."));
      writeID(readCard);
      Serial.println(F("-----------------------------"));
      lcd.setCursor(0,1);
      lcd.print("Scan a device to ADD or REMOVE");
      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
    }
  } else{
     if ( findID(readCard) ) { // If not, see if the card is in the EEPROM
       lcd.setCursor(0,0);
       lcd.print("You shall pass. Welcome.");
       Serial.println(F("Welcome, You shall pass"));
       // granted(300);         // Open the door lock for 300 ms
      }
      else {      // If not, show that the ID was not valid
       lcd.setCursor(0,0);
       lcd.print("You shall not pass.");
        Serial.println(F("You shall not pass"));
       // denied();
      }
  }
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    while (true);
  }
}

uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

boolean findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}

void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != 0 )      // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    //   failedWrite();      // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    //    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    //    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    //    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}


