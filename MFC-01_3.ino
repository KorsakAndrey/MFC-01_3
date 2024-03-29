//#pragma GCC optimize ("-O")

//I/O
#define UP_PIN 6
#define DOWN_PIN 5
#define SET_PIN 4
#define CLK 3
#define DIO 2
//POWER
#define POWER 7
#define VREF 1.1     //V
#define DIV_R1 9750  //betwen "+" and POWER_SENS
#define DIV_R2 2700  //betwen "-" and POWER_SENS
#define POWER_SENS A3
#define LOW_BAT 3.60  //V
//Const
#define ON 127
#define OFF 0
#define blink_delay 500  //ms
#define SHIFT_MEMORY 1

#include "GyverButton.h"
#include "GyverTM1637.h"
#include <EEPROM.h>
#include <MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();
constexpr float VoltFactor = ((float)(DIV_R1 + DIV_R2) / DIV_R2) * VREF / 1024;

//Global var
byte MODE = 1; //Global work mode
byte PRESET = 0; //Preset number
bool powerOn = true; //Device Status
float voltage = 4.0; //Onboard voltage

//Flags
bool sendFlag = false; //For send programms
bool transmitFlag = false;
bool sw1_Flag = false;
bool sw2_Flag = false;
bool muteFlag = false;
bool editFlag = false;
bool voltFlag = false; //Show bort voltage
bool batFlag = true;  //Battery is OK
bool shutFlag = false; //Shutoff device
bool refreshFlag = true;


//Static vallue in memory
struct {
  byte channel;
  byte bright;
  byte shiftPreset;
  bool autoSend;
  byte switchMode;
  byte command_1;
  byte command_2;
  byte command_mute;
  byte preset_1;
  byte preset_2;
  byte tip;
  byte ring;
} memory;

//Settings
const byte settings_names[][4] PROGMEM = {
  {0, _C, _h, 0}, {0, _B, _r, 0},
  {_S, _h, _f, _t}, {_A, _u, _t, _o},
  {0x33, 0x27, _o, _d},
  {0, _S, _1, 0}, {0, _S, _2, 0},
  {0x33, 0x27, _u, _t},
  {0, _P, _1, 0}, {0, _P, _2, 0},
  {0, _t, _i, _P}, {0, _r, _i, _n}
};
const byte condition[][4] PROGMEM = {
  {0, _O, _f, _f}, {0, 0, _O, _n}
};
const byte change[][4] PROGMEM = {
  {0, 0, _P, _C}, {0, 0, _C, _C},
  {_S, 0, _C, _H}, {0, _P, _1, 0},
  {0, _P, _2, 0}, {0, _P, _C, 0}
};
const byte max_val[] = {16, 7, 127, 1, 2, 127, 127, 127, 127, 127, 127, 127};
byte item = 0; //Settings menu item


//Display initialization
GyverTM1637 seg_display(CLK, DIO);


//Buttons initialization
GButton Up(UP_PIN);
GButton Down(DOWN_PIN);
GButton Set(SET_PIN);

void button_event(); //processing press of buttons
void display_send(); //send data to display
void timer_flag(bool &flag, const int &t_delay); //invert flag by timer
void setting(byte* max_val); //change devices settings
void readMemory();
void writeMemory();
void bat_stat(); //check battery
void save_bat();
byte* readFlash(const byte & pointer);


void setup() {
  //Start
  pinMode(POWER, OUTPUT);
  digitalWrite(POWER, powerOn);

  //Set VREF 1.1v
  analogReference(INTERNAL);

  //Read values from eeprom
  readMemory();

  //Configuration protocoll and display
  MIDI.begin(memory.channel); //MIDI
  Serial.begin(31250); //Serial 9600 ,MIDI 31250
  seg_display.clear();
  seg_display.brightness(memory.bright);



  //Buttons config
  Up.setDebounce(50); // настройка антидребезга (по умолчанию 80 мс)
  Up.setTimeout(300); // настройка таймаута на удержание (по умолчанию 500 мс)
  Up.setClickTimeout(300); // настройка таймаута между кликами (по умолчанию 300 мс)
  Down.setDebounce(50);
  Down.setTimeout(300);
  Down.setClickTimeout(300);
  Set.setDebounce(50);
  Set.setTimeout(1500);
  Set.setClickTimeout(300);

  seg_display.intro(60);
}

void loop() {
  Up.tick();
  Down.tick();
  Set.tick();

  button_event();
  if (not refreshFlag && not voltFlag) {
    save_bat();
  }
  display_send();
}

void button_event() {
  //Change preset
  switch (MODE) {
    case 1: {
        {
          if (not muteFlag) {
            if (Up.isClick() || Up.isStep()) {
              (PRESET < 127) ? PRESET++ : PRESET = 0;
              sendFlag = true;
              refreshFlag = true;
            }
            if (Down.isClick() || Down.isStep()) {
              (PRESET > 0) ? PRESET-- : PRESET = 127;
              sendFlag = true;
              refreshFlag = true;
            }
            if ((Set.isSingle() || memory.autoSend) && sendFlag) {
              MIDI.sendProgramChange(PRESET, memory.channel);
              sendFlag = false;
              transmitFlag = !memory.autoSend;
              refreshFlag = true;
            }
            if (Set.isDouble()) {
              MODE = 2;
              refreshFlag = true;
            }
            if (Set.isTriple()) {
              MODE = 3;
              voltFlag = true;
              refreshFlag = true;
            }
          }
          if (Set.isHolded()) {
            muteFlag = !muteFlag;
            if (muteFlag) {
              MIDI.sendControlChange(memory.command_mute, ON, memory.channel);
            }
            else {
              MIDI.sendControlChange(memory.command_mute, OFF, memory.channel);
            }
            refreshFlag = true;
          }
        }
        break;
      }
    case 2: {   //Send control command
        {
          switch (memory.switchMode) {
            case 0: {
                if (Down.isClick()) {
                  PRESET = memory.preset_1;
                  MIDI.sendProgramChange(PRESET, memory.channel);
                  refreshFlag = true;
                }
                if (Up.isClick()) {
                  PRESET = memory.preset_2;
                  MIDI.sendProgramChange(PRESET, memory.channel);
                  refreshFlag = true;
                }
                break;
              }
            case 1: {
                if (Up.isClick()) {
                  sw2_Flag = !sw2_Flag;
                  MIDI.sendControlChange(memory.command_2, sw2_Flag ? ON : OFF, memory.channel);
                  refreshFlag = true;
                }
                if (Down.isClick()) {
                  sw1_Flag = !sw1_Flag;
                  MIDI.sendControlChange(memory.command_1, sw1_Flag ? ON : OFF, memory.channel);
                  refreshFlag = true;
                }
                break;
              }
            case 2: {
                if (Up.isClick()) { //Green
                  MIDI.sendControlChange(memory.tip, ON, memory.channel); //Tip
                  MIDI.sendControlChange(memory.ring, ON, memory.channel); //Ring
                  refreshFlag = true;
                  transmitFlag = true;
                }
                if (Down.isClick()) { //Red
                  MIDI.sendControlChange(memory.tip, OFF, memory.channel); //Tip
                  MIDI.sendControlChange(memory.ring, OFF, memory.channel); //Ring
                  refreshFlag = true;
                  transmitFlag = true;
                }
                if (Set.isSingle()) { //Blue
                  MIDI.sendControlChange(memory.tip, ON, memory.channel); //Tip
                  MIDI.sendControlChange(memory.ring, OFF
                                         , memory.channel); //Ring
                  refreshFlag = true;
                  transmitFlag = true;
                }
                break;
              }
          }

          if (Set.isDouble()) {
            MODE = 1;
            Set.resetStates();
            refreshFlag = true;
          }
        }
        break;
      }
    case 3: {   //Shutdown
        {
          //Check battery
          if (voltFlag) {
            if (shutFlag) {
              timer_flag(powerOn, 1000); //1
            } else {
              timer_flag(voltFlag, 2000); //2
            }
            if (!powerOn) {
              digitalWrite(POWER, powerOn);
            }
            if (Up.isClick() && Down.isClick()) {
              shutFlag = true;
              refreshFlag = true;
            }
          }
          //Settings
          if (not voltFlag) {
            if (Set.isTriple()) {
              MODE = 1;
              editFlag = false;
              refreshFlag = true;
              item = 0;
              Set.resetStates();
              return;
            }
            if (not editFlag) {
              if (Set.isSingle()) {
                editFlag = true;
                refreshFlag = true;
              }
              if (Up.isClick()) {
                item == sizeof(max_val) - 1 ? item = 0 : item++;
                refreshFlag = true;
              }
              if (Down.isClick()) {
                item == 0 ? item = sizeof(max_val) - 1 : item-- ;
                refreshFlag = true;
              }
            }
            else {
              setting(max_val);
            }
          }
        }
        break;
      }
  }
}


void display_send() {
  if (refreshFlag) {
    switch (MODE) {
      case 1: { //PRESET
          if (not batFlag) {
            seg_display.point(0, true);
          }
          else {
            seg_display.point(0, false);
          }
          if (not sendFlag && not muteFlag) {
            seg_display.brightness(memory.bright);
            if (transmitFlag) {
              seg_display.twistByte(0, 0x70, 15);
              transmitFlag = false;
            }
            if (memory.autoSend) {
              seg_display.displayPreset(_A, PRESET + memory.shiftPreset);
            }
            else {
              seg_display.displayPreset(0x70, PRESET + memory.shiftPreset);
            }
            refreshFlag = false;
          }
          else {
            if (sendFlag && not muteFlag) {
              static byte temp_bright;
              static bool blinkFlag = true;
              temp_bright = memory.bright ? 0 : -1;
              timer_flag(blinkFlag, blink_delay); //3
              blinkFlag ? seg_display.brightness(memory.bright)
              : seg_display.brightness(temp_bright);
              seg_display.displayInt(PRESET + memory.shiftPreset);
            }
            if (muteFlag) {
              seg_display.displayByte(readFlash(settings_names[7]));//Mut
              refreshFlag = false;
            }
          }
          break;
        }

      case 2: {  //MODE
          static byte to_display[4] = {0};
          seg_display.brightness(memory.bright);
          if (not batFlag) {
            seg_display.point(0, true);
          }
          else {
            seg_display.point(0, false);
          }
          switch (memory.switchMode) {
            case 0: {
                {
                  if (memory.preset_1 == PRESET) {
                    seg_display.displayByte(readFlash(change[3]));//P1
                  }
                  else if (memory.preset_2 == PRESET) {
                    seg_display.displayByte(readFlash(change[4]));//P2
                  }
                  else {
                    seg_display.displayByte(readFlash(change[5]));//PC
                  }
                }
                break;
              }
            case 1: {
                {
                  if (sw1_Flag) {
                    to_display[0] = 0x5c;
                  }
                  else {
                    to_display[0] = 0x08;
                  }
                  if (sw2_Flag) {
                    to_display[3] = 0x5c;
                  }
                  else {
                    to_display[3] = 0x08;
                  }
                  seg_display.displayByte(to_display);
                }
                break;
              }
            case 2: {
                {
                  if (transmitFlag) {
                    seg_display.scrollByte(1, 0x08, 100);
                    delay(100);
                    seg_display.displayByte(1, 0x00);
                    transmitFlag = false;
                  }
                  else {
                    seg_display.displayByte(readFlash(change[2]));//S CH
                  }
                }

                break;
              }
          }
          refreshFlag = false;
          break;
        }

      case 3: {  //SETTINGS
          seg_display.brightness(memory.bright);
          if (shutFlag) {
            seg_display.displayByte(_O, _f, _f, 0);
          }
          else if (voltFlag) {
            seg_display.displayFloat(voltage);
          }
          else {
            if (not editFlag) {
              seg_display.displayByte(readFlash(settings_names[item])); //Names
            }
            else {
              if (item == 3) {
                seg_display.displayByte(readFlash(condition[*((bool*)&memory + item)]));
              }
              else if (item == 4) {
                seg_display.displayByte(readFlash(change[*((bool*)&memory + item)]));
              }
              else {
                seg_display.displayInt(*((byte*)&memory + item));
              }
            }
            refreshFlag = false;
          }
          break;
        }
      default: {
          seg_display.displayByte(_E, _r, _r, 0);
          break;
        }
    }
  }
}


void timer_flag(bool & flag, const int &t_delay) {
  static bool condit;
  static bool* p;
  static uint32_t timer = 0;

  if (timer == 0 || p != &flag ) {
    timer = millis();
    p = &flag;
    condit = !flag;
    return;
  }
  if (millis() - timer > t_delay) {
    flag = condit;
    timer = 0;
  }
}


void setting(byte * max_val) {
  byte* pointer = (byte*)&memory + item;
  if (Set.isSingle()) {
    editFlag = false;
    refreshFlag = true;
    writeMemory();
  }
  if (Up.isClick()) {
    *pointer == max_val[item] ? *pointer = 0 : (*pointer)++;
    refreshFlag = true;
  }
  if (Down.isClick()) {
    *pointer == 0 ? *pointer = max_val[item] : (*pointer)--;
    refreshFlag = true;
  }
}


void readMemory() {
  EEPROM.get(SHIFT_MEMORY, memory);
}

void writeMemory() {
  EEPROM.put(SHIFT_MEMORY, memory);
}

void bat_stat() {
  static int currentTime = 0;
  if (millis() - currentTime > 5000) {
    voltage = (float)analogRead(POWER_SENS) * VoltFactor;
    currentTime = millis();
  }
}


void save_bat() {
  bat_stat();
  if (voltage < LOW_BAT) {
    batFlag = false;
    memory.bright = 0;
  }
  if (!batFlag && (voltage > LOW_BAT + 0.1)) {
    batFlag = true;
    EEPROM.get((byte)&memory.bright - (byte)&memory + SHIFT_MEMORY, memory.bright);
  }
  refreshFlag = true;
}

byte* readFlash(const byte& pointer) {
  static byte temp[4];
  for (byte i = 0 ; i < 4 ; i++) {
    temp[i] = (byte)pgm_read_byte(pointer + i);
  }
  return temp;
}
