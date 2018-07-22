#include "FastLED.h"
#include <EEPROM.h>
#include <LiquidCrystal.h>

#define MAX_COUNT 400
CRGB leds[MAX_COUNT]; 

LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

#define DATA_PIN 13
#define CLOCK_PIN 12

#define ADDR_MODE 0
#define ADDR_COUNT 1


/*
 * Pushbutton constants
 */

#define NUM_BUTTONS 3
#define STATE_UP 0
#define STATE_DOWN 1
#define STATE_MODE 2

int button_pins[NUM_BUTTONS] = { A0, A1, A2};
int button_states[NUM_BUTTONS] = { HIGH, HIGH, HIGH }; // the current reading from the input pin
int last_button_states[NUM_BUTTONS] = { HIGH, HIGH, HIGH };   // the previous reading from the input pin
unsigned long last_debounce_times[NUM_BUTTONS] = { 0 };  // the last time the output pin was toggled
unsigned long debounce_delay = 20; // the debounce time increase if the output flickers


/*
 * Constants for the press and hold for up and down led count
 */

unsigned long hold_start_time = 0;
unsigned long holding = false; 

#define INCREMENT 300
int increment_time = INCREMENT;
int increment_shrink_by = 50;

byte led_count;


/*
 * Constants for LED types
 */

#define NUM_LED_TYPES 3
#define TYPE_WS2812B 0
#define TYPE_APA102C 1
#define TYPE_WS2811 2

char type_strings[NUM_LED_TYPES][8] = { "WS2812B\0", "APA102C\0", "WS2811 \0", };

#define NUM_SWITCH_PINS 4
int switch_pins[NUM_SWITCH_PINS] = { 8, 9, 10, 11 };

int led_type;


/*
 * Constants for LED Modes
 */

#define NUM_LED_MODES 3
#define MODE_RGB 0
#define MODE_HUE 1
#define MODE_WHTE 2

char mode_strings[NUM_LED_MODES][5] = { "RGB \0", "HUE \0", "WHTE\0" }; 
bool mode_holding = false; 
byte led_mode;

// rgb animation
#define RGB_NUM_COLORS 3
#define RGB_CHANGE_DURATION 1000
unsigned long rgb_last_change = 0;
int rgb_color_num = 0; 
CRGB rgb_colors[RGB_NUM_COLORS] = {CRGB::Red, CRGB::Green, CRGB::Blue}; 

//hue animation
int current_hue = 0;
#define HUE_CHANGE_DURATION 1
unsigned long hue_last_change = 0;

/*
 * System variables
 */

bool need_reset = false;
bool need_display_update = true; 

void setup() {
  
  led_mode = EEPROM.read(ADDR_MODE);
  led_count = EEPROM.read(ADDR_COUNT);

  Serial.begin(9600);
  lcd.begin(16, 2);

  for (int i = 0; i < NUM_BUTTONS; i++){
    pinMode(button_pins[i], INPUT);
  }

  for (int i = 0; i < NUM_SWITCH_PINS; i++){
    pinMode(switch_pins[i], INPUT);
    if (digitalRead(switch_pins[i])) {
      led_type = i;
      break;
    }
  }

  switch (led_type) {
    
    case TYPE_WS2812B :
      FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, MAX_COUNT);
      break;

    case TYPE_APA102C : 
      FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, MAX_COUNT);
      break;

    case TYPE_WS2811 :
      FastLED.addLeds<WS2811, DATA_PIN>(leds, MAX_COUNT);
      break;

    default :
      need_reset = true;
      break;
  }
  
}

void loop() {

  for (int i = 0; i < NUM_SWITCH_PINS; i++){
    if (digitalRead(switch_pins[i])) {
      if (i != led_type) {
         need_reset = true;
      }
    }
  }

  if (need_reset) {
    lcd.setCursor(0, 0);
    lcd.print("Bad Switch Pos. ");
    lcd.setCursor(0, 1);
    lcd.print("Move, Reset Now!");
    return; 
  }

  process_debounce();

  // led count
  if ((button_states[STATE_UP] == LOW) || (button_states[STATE_DOWN] == LOW)) {

    int increment = false;

    if (holding == false) {
      int increment_time = 500;
      holding = true;
      hold_start_time = millis();
      increment = true;
    }

    unsigned long hold_time = millis() - hold_start_time;

    if (hold_time > increment_time) {
      increment = true;
      if (increment_time > increment_shrink_by) {
        increment_time -= increment_shrink_by;
      }
      hold_start_time = millis();
    }
    
    if (increment) {
      if (button_states[STATE_UP] == LOW) {
        if (led_count != MAX_COUNT) {
          led_count++;
        }
      } 
      
      else if (button_states[STATE_DOWN] == LOW) {
        if (led_count != 0) {
          led_count--;
        }
      }
      need_display_update = true;
      
    }
  }
  else if ((button_states[STATE_UP] == HIGH) || (button_states[STATE_DOWN] == HIGH)) {
    holding = false;
    increment_time = INCREMENT;
  }

  // led mode
  if (button_states[STATE_MODE] == LOW) {
    if (mode_holding == false) {
      mode_holding = true;
      led_mode++;
      if (led_mode >= NUM_LED_MODES) {
        led_mode = 0; 
      }
      need_display_update = true;
    }
  }
  else {
    mode_holding = false; 
  }
   
  /*
  * Make changes to the animations
  */

  switch (led_mode) {
  
    case MODE_RGB :       
      if (millis() - rgb_last_change > RGB_CHANGE_DURATION) {
        rgb_color_num++;
        if (rgb_color_num >= RGB_NUM_COLORS) {
          rgb_color_num = 0;
        }
        rgb_last_change = millis();
      }
      
      for (int i = 0; i < led_count; i++) {
        leds[i] = rgb_colors[rgb_color_num];
      }
      break;

    case MODE_HUE : 
      if (millis() - hue_last_change > HUE_CHANGE_DURATION) {
        current_hue++;
        if (current_hue >= 255) {
          current_hue = 0;
        }
        hue_last_change = millis();
      }
      
      for (int i = 0; i < led_count; i++) {
        leds[i] = CHSV(current_hue, 255, 255);
      }
      break;

    case MODE_WHTE :
      for (int i = 0; i < led_count; i++) {
        leds[i] = CRGB::White;
      }
      break; 

    default :
      break;
  }

  // turn the rest of the LEDs off
  for (int i = led_count; i < MAX_COUNT; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
   
  /*
   * Update the LCD
   */

  if (need_display_update) {

    EEPROM.write(ADDR_MODE, led_mode);
    EEPROM.write(ADDR_COUNT, led_count);
    
    lcd.setCursor(0, 0);
    lcd.print(type_strings[led_type]);
  
    lcd.setCursor(8, 0);
    char led_count_str[8];
    sprintf(led_count_str, "LEDs:%03d", led_count);
    lcd.print(led_count_str);
  
    lcd.setCursor(0, 1);
    lcd.print("Mode:");
    lcd.print(mode_strings[led_mode]);
  }

  need_display_update = false; 
  
}

void process_debounce() {
  
  for (int i = 0; i < NUM_BUTTONS; i++) {

    int reading = digitalRead(button_pins[i]);
  
    if (reading != last_button_states[i]) {
      last_debounce_times[i] = millis();
    }
    
    if ((millis() - last_debounce_times[i]) > debounce_delay) {
      if (reading != button_states[i]) {
        button_states[i] = reading;
      }
    }
  
    last_button_states[i] = reading;
  }
}


