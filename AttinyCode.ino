/*Eddie Farr 2018*/

#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

//bytes
# define Start_Byte 0x7E
# define Version_Byte 0xFF
# define Command_Length 0x06
# define End_Byte 0xEF
# define Acknowledge 0x00 //Returns info with command 0x41 [0x01: info, 0x00: no info]

//assign pins. pins 1 and 5 are still free in this setup
const int rx = 3; //3 on attiny
const int tx = 4; //4 on attiny
const int reedPinOne = 0; //0 on attiny just splice a bunch of reed switches together to this one pin 
const int photoPin = A1; //A1 on attiny
const int ledPin = 1;

//constant params
const int numReadings = 5;
const int wakeValue = 150; //set this to sunset 
const int vol = 30; // 0 - 30

//non constant params
int photoValue = 0;
int readings[numReadings];
int readIndex = 0;
int total = 0;
int average = 0;

bool isPlaying;

SoftwareSerial mySerial(rx, tx);

//gets called on wakeup
ISR(WDT_vect) {
}

void setup() {
  //set pin modes
  pinMode(rx, INPUT);
  pinMode(tx, OUTPUT);
  pinMode(reedPinOne, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(photoPin, INPUT);

  //house keeping
  initArray();
  isPlaying = false;

  //serial
  executeCMD(0x3F, 0, 0);
  mySerial.begin(9600);
  while (mySerial.available() < 10)
    delay(50);
  setVolume(vol);
  delay(150);

  //Power down various bits of hardware to lower power usage
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //Power down everything, wake up from WDT
  sleep_enable();
}

void loop() {
  ADCSRA &= ~(1 << ADEN); //Disable ADC, saves ~230uA
  setup_watchdog(9);
  sleep_mode();

  ADCSRA |= (1 << ADEN);

  averageVal();
  int proximity = digitalRead(reedPinOne); //LOW = closed

  if (average > wakeValue && proximity == LOW) {

    wdt_disable();

    digitalWrite(ledPin, HIGH);

    if (isPlaying) {
      executeCMD(0x03, 0, 1);
      isPlaying = false;
    } else {
      isPlaying = true;
      executeCMD(0x03, 0, 1);
    }
    digitalWrite(ledPin, LOW);
  }
}
/* Functions */
//set volume
void setVolume(int volume) {
  executeCMD(0x06, 0, volume); // Set the volume (0x00~0x30)
}

// DF serial command code, executes the command
void executeCMD(byte CMD, byte Par1, byte Par2) {
  // Calculate the checksum (2 bytes)
  int16_t checksum = -(Version_Byte + Command_Length + CMD + Acknowledge + Par1 + Par2);

  // Build the command line
  byte Command_line[10] = { Start_Byte, Version_Byte, Command_Length, CMD, Acknowledge, Par1, Par2, checksum >> 8, checksum & 0xFF, End_Byte};

  //Send the command line to the module
  for (byte k = 0; k < 10; k++)
  {
    mySerial.write( Command_line[k]);
  }
}

//initialize the photoresistor array to 0
void initArray() {
  for (int i = 0; i < numReadings; ++i) {
    readings[i] = 0;
  }
}

//average the values of photoresistor
void averageVal() {
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(photoValue);
  total = total + readings[readIndex];
  readIndex = readIndex + 1;

  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  average = total / numReadings;
  delay(150);
}

//sets watchdog timer to wake us up, but not reset
void setup_watchdog(int timerPrescaler) {

  if (timerPrescaler > 9 ) timerPrescaler = 9; //Correct incoming amount if need be

  byte bb = timerPrescaler & 7;
  if (timerPrescaler > 7) bb |= (1 << 5); //Set the special 5th bit if necessary

  //This order of commands is important and cannot be combined
  MCUSR &= ~(1 << WDRF); //Clear the watch dog reset
  WDTCR |= (1 << WDCE) | (1 << WDE); //Set WD_change enable, set WD enable
  WDTCR = bb; //Set new watchdog timeout value
  WDTCR |= _BV(WDIE); //Set the interrupt enable, this will keep unit from resetting after each int
}
