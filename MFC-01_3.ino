#define UP_PIN 6
#define DOWN_PIN 5
#define SET_PIN 4
#define CLK 3
#define DIO 2
#define IS_ON 7
#define VREF 1.1
/*((DIV_R1 + DIV_R2) / DIV_R2) = 4.6
DIV_R1 9750  betwen "+" and POWER_SENS
DIV_R2 2700  betwen "-" and POWER_SENS
*/
#define POWER_COEFF 4.6
#define POWER_SENS A3
#define LOW_BAT 3.60  //V
#define ON 127
#define OFF 0
#define blink_delay 500  //ms


#include "GyverButton.h"
#include "GyverTM1637.h"
#include <EEPROM.h>
#include <MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();

//Global var
byte MODE = 1; //Global work mode
byte PRESET = 0; //Preset number
bool is_on = false; //Device Status
float voltage; //Onboard voltage

//Flags
bool sendFlag = false; //For send programms
bool transmitFlag = false;
bool sw1_Flag = false;
bool sw2_Flag = false;
bool muteFlag = false;
bool editFlag = false;
bool voltFlag = false;
bool batFlag = true;
bool shutFlag = false;


//Static vallue in memory
byte channel; //Channel
byte bright; //Bright
byte max_preset; //max value preset
byte sw1_command;
byte sw2_command;
byte mute_command;
byte auto_send;
byte switch_mode;
byte p1_programm;
byte p2_programm;

//Settings
byte settings_names[][4] = {
  {0, _C, _h, 0}, {0, _B, _r, 0},
  {_E, _n, _d, 0}, {0, _S, _1, 0},
  {0, _S, _2, 0}, {0x33, 0x27, _u, _t},
  {_A, _u, _t, _o}, {0x33, 0x27, _o, _d},
  {0, _P, _1, 0},{0, _P, _2, 0}
};
byte* settings[] = {&channel, &bright, &max_preset, &sw1_command,
                    &sw2_command, &mute_command, &auto_send,
                    &switch_mode, &p1_programm, &p2_programm
                   };
byte max_val[] = {16, 7, 127, 127, 127, 127, 1, 1, 127, 127};
byte item = 0;


//Display initialization
GyverTM1637 seg_display(CLK, DIO);


//Buttons initialization
GButton Up(UP_PIN);
GButton Down(DOWN_PIN);
GButton Set(SET_PIN);


void setup() {
  //Start
  pinMode(IS_ON, OUTPUT);
  digitalWrite(IS_ON, is_on);

  //Set VREF 1.1v
  analogReference(INTERNAL);

  //Read values from eeprom
  EEPROM.get(1, channel);
  EEPROM.get(2, bright);
  EEPROM.get(3, max_preset);
  EEPROM.get(4, sw1_command);
  EEPROM.get(5, sw2_command);
  EEPROM.get(6, mute_command);
  EEPROM.get(7, auto_send);
  EEPROM.get(8, switch_mode);
  EEPROM.get(9, p1_programm);
  EEPROM.get(10, p2_programm);

  //Configuration protocoll and display
  MIDI.begin(channel); //MIDI
  Serial.begin(31250); //Serial 9600 ,MIDI 31250
  seg_display.clear();
  seg_display.brightness(bright);


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
}


void button_event(); //processing click of buttons
void display_send(); //send data to display
bool timer_flag(bool &flag, const int &t_delay); //invert flag by timer
void setting(byte** setting, byte* max_val); //change devices settings
void bat_stat(float &voltage); //check battery
void save_bat();


void loop() {
  Up.tick();
  Down.tick();
  Set.tick();

  button_event();
  if(not voltFlag){
    save_bat();
    } 
  display_send();
}


void button_event() {
  //Change preset
  if ( MODE == 1) {
    if ((Up.isClick() || Up.isStep()) && not muteFlag) {
      (PRESET < max_preset - 1) ? PRESET++ : PRESET = 0;
      sendFlag = true;
    }
    if ((Down.isClick() || Down.isStep()) && not muteFlag) {
      (PRESET > 0) ? PRESET-- : PRESET = max_preset - 1;
      sendFlag = true;
    }
    if(sendFlag && auto_send) {
      MIDI.sendProgramChange(PRESET, channel);
      sendFlag = false;
      }
    if (Set.isSingle() && sendFlag) {
      MIDI.sendProgramChange(PRESET, channel);
      sendFlag = false;
      transmitFlag = true;
    }
    if (Set.isDouble() && not muteFlag) {
      MODE = 2;
    }
    if (Set.isTriple() && not muteFlag) {
      MODE = 3;
      voltFlag = true;
    }
    if (Set.isHolded()) {
      muteFlag ? muteFlag = false : muteFlag = true;
      if (muteFlag) {
        MIDI.sendControlChange(mute_command, ON, channel);
      }
      else {
        MIDI.sendControlChange(mute_command, OFF, channel);
      }
    }
  }
  //Send control command
  if (MODE == 2) {
    static byte sw1 = 0;
    static byte sw2 = 0;

    if(not switch_mode){
      if (Up.isClick()) {
        sw2 == OFF ? sw2 = ON : sw2 = OFF;
        MIDI.sendControlChange(sw2_command, sw2, channel);
        sw2_Flag = (sw2 == OFF) ? false : true;
      }
      if (Down.isClick()) {
        sw1 == OFF ? sw1 = ON : sw1 = OFF;
        MIDI.sendControlChange(sw1_command, sw1, channel);
        sw1_Flag = (sw1 == OFF) ? false : true;
      }
    }
    else {
      if (Down.isClick()) {
        MIDI.sendProgramChange(p1_programm-1, channel);
        PRESET = p1_programm-1;
      }
      if (Up.isClick()) {
        MIDI.sendProgramChange(p2_programm-1, channel);
        PRESET = p2_programm-1;
      }
    }
    if (Set.isDouble()) {
      MODE = 1;
      Set.resetStates();
    }
  }
  //Shutdown
  if (MODE == 3 && voltFlag) {
    if(shutFlag) {
      timer_flag(is_on, 1000);
      }
    if(is_on){
        digitalWrite(IS_ON, is_on);
        }
    if(Up.isClick() && Down.isClick()){
      shutFlag = true;      
      }
    }
  //Settings
  else if (MODE == 3 && not voltFlag) {
    if (Set.isTriple()) {
      MODE = 1;
      editFlag = false;
      item = 0;
      Set.resetStates();
      return;
    }
    if (not editFlag) {
      if (Set.isSingle()) {
        editFlag = true;
      }
      if (Up.isClick()) {
        item == sizeof(max_val) - 1 ? item = 0 : item++;
      }
      if (Down.isClick()) {
        item == 0 ? item = sizeof(max_val) - 1 : item-- ;
      }
    }
    else {
      setting(settings, max_val);
    }
  }
}


void display_send() {
  switch (MODE) {
    case 1: {
        if (not batFlag) {
          seg_display.point(0, true);
        }
        else {
          seg_display.point(0, false);
        }
        if (not sendFlag && not muteFlag) {
          seg_display.brightness(bright);
          if (transmitFlag) {
            seg_display.twistByte(0, 0x70, 15);
            transmitFlag = false;
          }
          if(auto_send){
            seg_display.displayByte(0 , _A);
            }
          else {
            seg_display.displayByte(0 , 0x70);
            }
          seg_display.display(1, (PRESET + 1) / 100 ? 1 : 10);
          seg_display.display(2, PRESET + 1 < 10 ? 10 : ((PRESET + 1) % 100) / 10);
          seg_display.display(3, ((PRESET + 1) % 100) % 10);
        }
        else {
          if (sendFlag && not muteFlag) {
            static byte temp_bright;
            static bool blinkFlag = true;
            temp_bright = bright ? 0 : -1;
            timer_flag(blinkFlag, blink_delay);
            blinkFlag ? seg_display.brightness(bright)
            : seg_display.brightness(temp_bright);
            seg_display.displayInt(PRESET + 1);
          }
          if (muteFlag) {
            seg_display.displayByte(0x33, 0x27, _u, _t);
          }
        }

        break;
      }

    case 2: {
        static byte to_display[4] = {0};
        if (not batFlag) {
          seg_display.point(0, true);
        }
        else {
          seg_display.point(0, false);
        }
        if(not switch_mode) {
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
        else {
          if(p1_programm == PRESET+1) {
            seg_display.displayByte(0, _P, _1, 0);
          }
          else if(p2_programm == PRESET+1) {
            seg_display.displayByte(0, _P, _2, 0);
          }
          else {
            seg_display.displayByte(0, _P, _C, 0);
          } 
           
        }
        break;
      }

    case 3: {
        seg_display.brightness(bright);
        if(shutFlag){
          seg_display.displayByte(_O, _f, _f, 0);
          }
        else if (voltFlag) {
          seg_display.displayFloat(voltage);
          timer_flag(voltFlag, 2000);
        }
        else {
          if (not editFlag) {
            seg_display.displayByte(settings_names[item]);
          }
          else {
            if(item == 6){
              (*settings[item]) ?
              seg_display.displayByte(0, 0, _O, _n) :
              seg_display.displayByte(0, _O, _f, _f);
              }
            else if(item == 7){
              (*settings[item]) ?
              seg_display.displayByte(0, 0, _P, _C) :
              seg_display.displayByte(0, 0, _C, _C);
              }  
            else {  
              seg_display.displayInt(*settings[item]);
            }
          }
        }
        break;
      }
    default: {
        seg_display.displayByte(_E, _r, _r, 0);
        break;
      }
  }
}


bool timer_flag(bool &flag, const int &t_delay) {
  static bool reset = true;
  static bool condit;
  static bool* p;
  static uint32_t timer;

  if (true == reset || p != &flag ) {
    timer = millis();
    reset = false;
    p = &flag;
    condit = !flag;
  }
  if (millis() - timer > t_delay) {
    flag = condit;
    reset = true;
    return true;
  }
}


void setting(byte** setting, byte* max_val) {
  if (Set.isSingle()) {
    editFlag = false;
  }
  if (Up.isClick()) {
    *setting[item] == max_val[item] ? *settings[item] = 0 :
        *setting[item] = *setting[item] + 1;
  }
  if (Down.isClick()) {
    *setting[item] == 0 ? *settings[item] = max_val[item] :
                                            *setting[item] = *setting[item] - 1;
  }


  EEPROM.put(item + 1, *settings[item]);
}


void bat_stat(float &voltage) {
  double temp = (double)analogRead(POWER_SENS) * VREF * POWER_COEFF / 1024;
  voltage = (float)temp;
}



void save_bat() {
  bat_stat(voltage);
  if (voltage < LOW_BAT) {
    batFlag = false;
    bright = 0;
  }
  if (voltage > LOW_BAT + 0.1) {
    batFlag = true;
    EEPROM.get(2, bright);
  }
}
