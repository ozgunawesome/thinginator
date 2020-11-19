#include <MCP3XXX.h>
#include <MIDI.h>
#include <LiquidCrystal.h>
#include <Wire.h>

MIDI_CREATE_DEFAULT_INSTANCE();

LiquidCrystal lcd(6, 7, 2, 3, 4, 5); // Arguments: RS, enable pin, D4, D5, D6, D7. R/W pin shorted to ground.
MCP3004 adc; // default address on the SPI bus

static uint8_t const midiChannel = 13; // MIDI channel to send values on.
static uint8_t const midiControlCodes[10] = {22, 27, 23, 28, 24, 29, 25, 30, 26, 31};  // these CC msgs will be sent

static uint8_t const slim_one_char[] = {1, 3, 1, 1, 1, 1, 3, 0};  // squeezed up number 1 character 
static uint8_t const play_char[] = {0, 8, 12, 14, 14, 12, 8, 0}; // small play icon

static char const bling_char = B11011111; // characters for metronome. look em up at :
static char const upper_char = B10100001; // http://www.handsonembedded.com/wp-content/uploads/2018/04/char_codes.png

static uint16_t u[10]; // raw values from A/D readings (10 bit)
static uint8_t m[10]; // MIDI send value (7-bit)
static uint8_t m_last[10]; // last seen midi value

static uint8_t midiClockCount = 0; // metronome counter - 24 clocks per quarter beat in MIDI

inline void printToLcdAt(int const col, int const row, char* const value) {
  lcd.setCursor(col, row);
  lcd.print(value);  
}

inline void writeToLcdAt(int const col, int const row, byte const value) {
  lcd.setCursor(col, row);
  lcd.write(value);
}

inline void updateLCD(int const i) {
  volatile uint8_t row = i % 2, col = i / 2;
  static char v[4];
  sprintf(v, "%3d", m[i]);
  if (v[0] == '1') v[0] = byte(1); // slim '1' character defined in setup()
  printToLcdAt(1 + col * 3, row, v);
}

inline void mainADCLoop() {
  updateInternalADCValues();
  updateExternalADCValues();  
}

inline void updateInternalADCValues() {
  for (byte i = A0, j = 0; j < 6; i++, j++) {
    sendMIDIAndUpdateLCD(j, analogRead(i));
  }
}

inline void updateExternalADCValues() {
  for (byte i = 0, j = 6; i < 4; i++, j++) {
    sendMIDIAndUpdateLCD(j, adc.analogRead(i));
  }
}

inline void sendMIDIAndUpdateLCD(uint8_t const index, uint16_t const value) {
  u[index] = value;
  m[index] = value >> 3;
  if (m_last[index] == m[index]) {
    return;
  }
  MIDI.sendControlChange(midiControlCodes[index], m[index], midiChannel);
  updateLCD(index);
  m_last[index] = m[index];
}

inline void showSplashScreen() {
  lcd.clear();
  printToLcdAt(0, 0, "    t  h  e    ");
  printToLcdAt(0, 1, "  THINGINATOR  ");
  delay(1500);
  lcd.clear();
}

inline void metronome() {
    switch (midiClockCount) {
    case 48:
      midiClockCount = 0;
    case 0: case 24:
      writeToLcdAt(0, 0, ' ');
      break;
    case 12:
      writeToLcdAt(0, 0, bling_char);
      break;
    case 36:
      writeToLcdAt(0, 0, upper_char);
      break;
  }
}

void midiClockReceived() {
  midiClockCount++;
  metronome();  
}

void midiStartReceived() {
  writeToLcdAt(0, 1, byte(2)); // this is the small 'play' icon
}

void midiStopReceived() {
  writeToLcdAt(0, 1, ' ');
  writeToLcdAt(0, 0, ' ');
  midiClockCount = 0;
}

inline void enter_dfu_mode() {
  // if all 10 pots are at max position at boot, we switch the MIDI interface
  // to 115.2 kbps USB->serial so it can still be flashed via Arduino IDE
  for (int i = 0; i < 10; i++) {
    if (m[i] != 127) {
      return;
    }
  }
  lcd.clear();
  printToLcdAt(0, 0, "Rebooting into");
  printToLcdAt(0, 1, "   DFU mode   ");
  Serial.write("\xF0\x77\x77\x77\x09\xF7", 6); // magic value for USB MIDI to USB serial switch
  delay(500);
  printToLcdAt(0, 0, "You are now in");
  while(true);
}

void setup() {
  lcd.begin(16, 2);  // start LCD display and show splash screen..
  
  lcd.createChar(1, slim_one_char); // this is a slim number '1' that only takes up the right side of the block
  lcd.createChar(2, play_char); // this is a small 'play' icon

  showSplashScreen();

  adc.begin(); // start external A/D converter

  MIDI.begin(MIDI_CHANNEL_OMNI);             // initialize MIDI listening on all channels
  MIDI.turnThruOff();                        // don't repeat received messages
  MIDI.setHandleClock(midiClockReceived);    // small metronome when clock signal in 
  MIDI.setHandleStart(midiStartReceived);    // small "play" icon when playing
  MIDI.setHandleContinue(midiStartReceived); // same
  MIDI.setHandleStop(midiStopReceived);      // clear the play icon and metronome
  
  memset(m_last, 0xFF, 10); // all last seen MIDI values are Set to garbage 
                            // so every value is sent once on first boot

  for (byte i = A0 ; i <= A5; i++) {
    pinMode(i, INPUT); // set internal analog pins to input mode
  }
  mainADCLoop();
  enter_dfu_mode();
}

void loop() {
  MIDI.read();
  mainADCLoop();
}
