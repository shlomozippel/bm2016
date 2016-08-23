#include <avr/sleep.h>
#include <RF24.h>
#include <FastLED.h>
#include <EEPROM.h>
#include "btn.h"
#include "timer.h"
#include "sparkle_receiver.h"

//----------------------------------------------------------------------------------
// General stuff
//----------------------------------------------------------------------------------

#define NUM_LEDS            100
#define FPS                 120
#define INITIAL_BRIGHTNESS  200
#define PATTERN_CYCLE_TIME  12000

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

//----------------------------------------------------------------------------------
// Controller specific definitions
//----------------------------------------------------------------------------------

// uncomment just one of these
//#define LED_KEYCHAIN
#define PRO_MINI_WEARABLE
//#define ARTBOAT

#ifdef LED_KEYCHAIN
#define LEDS_PIN       11
#define BUTTON1_PIN   3
#define BUTTON2_PIN   2
#define BUTTON3_PIN   0
#define BUTTON4_PIN   1
#define MODE_PIN      BUTTON4_PIN
#define LED_EN_PIN 5  // PC6
#define USB_POWERED
#endif

#ifdef PRO_MINI_WEARABLE  
#define LEDS_PIN          9
#define MODE_PIN          10
#define BRIGHTNESS_PIN    A0
#define USB_POWERED
#endif

#ifdef ARTBOAT
#define LEDS_PIN          15
#define MODE_PIN          16
#define HAS_RADIO  
#endif


//----------------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------------

CRGB leds[NUM_LEDS];
CRGB * leds2 = (leds + (NUM_LEDS/2));
Btn btn_mode(MODE_PIN);

#ifdef LED_KEYCHAIN
Btn btn_brightness_up(BUTTON2_PIN);
Btn btn_brightness_down(BUTTON3_PIN);
#endif

#ifdef HAS_RADIO
RF24 radio(9, 8);
#endif

uint8_t g_brightness = INITIAL_BRIGHTNESS;
uint32_t g_now = 0;

//----------------------------------------------------------------------------------
// LED Keychain
//----------------------------------------------------------------------------------
#ifdef LED_KEYCHAIN
void leds_wake_up() { }
void leds_sleep() {
    for (int j = g_brightness; j > 0; j-=2) {
        FastLED.setBrightness(j);
        FastLED.show();
    }
    FastLED.clear();
    pinMode(LEDS_PIN, INPUT);
    digitalWrite(LED_EN_PIN, LOW);

    USBCON |= _BV(FRZCLK);  //freeze USB clock
    PLLCSR &= ~_BV(PLLE);   // turn off USB PLL
    USBCON &= ~_BV(USBE);   // disable USB

    delay(500);

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    attachInterrupt(0, leds_wake_up, LOW);
    sleep_mode();

    /* SLEEP */
    sleep_disable();
    detachInterrupt(0);

    /* ON */
    sei();
    USBDevice.attach();
    delay(100);
    FastLED.clear();
    digitalWrite(LED_EN_PIN, HIGH);
    pinMode(LEDS_PIN, OUTPUT);
}
#endif
//----------------------------------------------------------------------------------
// Patterns
//----------------------------------------------------------------------------------

#include "blinkypants_patterns.h"
#include "fastled_patterns.h"
#include "tinybee_patterns.h"
#include "artemiid_patterns.h"

//----------------------------------------------------------------------------------
// Mode / state
//----------------------------------------------------------------------------------

typedef int (*SimplePatternList[])();
uint8_t g_current_pattern = 0;
MillisTimer pattern_timer;

SimplePatternList patterns = {
  collision,
  moving_palette,
  rainbow,
  confetti,
  mode_yalda,
};


void next_pattern()
{
    g_current_pattern = (g_current_pattern + 1) % ARRAY_SIZE( patterns);
    write_state();
    FastLED.clear();
}

void enable_autocycle() {
    if (pattern_timer.running()) return;    
    pattern_timer.start(PATTERN_CYCLE_TIME, true);
    next_pattern();  
}

void disable_autocycle() {
    if (!pattern_timer.running()) return;
    pattern_timer.stop();
    write_state();
}

void brightness_up() {
 switch (g_brightness) {
      case 0 ... 15: g_brightness += 1; break;
      case 16 ... 100: g_brightness += 15; break;
      case 101 ... (0xff - 30): g_brightness += 30; break;
      case (0xff - 30 + 1) ... 254: g_brightness = 255; break;
      case 255: break;
  } 
}

void brightness_down() {
  switch (g_brightness) {
      case 0: break;
      case 1 ... 15: g_brightness -= 1; break;
      case 16 ... 100: g_brightness -= 15; break;
      case 101 ... 255: g_brightness -= 30; break;
  }            
}

void mode_button() {
    if (pattern_timer.running()) {
      disable_autocycle();
    } else {
      next_pattern();
    }
}

void read_state() {
    uint8_t buffer = 0;
    // autocycle
    EEPROM.get(0, buffer);
    if (buffer) {
        pattern_timer.start(PATTERN_CYCLE_TIME, true);
    }
    
    // current pattern
    EEPROM.get(1, buffer);
    if (buffer == 255) {
      g_current_pattern = 0;
    } else {
      g_current_pattern = buffer % ARRAY_SIZE( patterns);
    }

    // brightness
    EEPROM.get(2, buffer);
    if (buffer == 255) {
      g_brightness = INITIAL_BRIGHTNESS;
    } else {
      g_brightness = buffer;
    }
    
}

void write_state() {
    EEPROM.update(0, pattern_timer.running());
    EEPROM.update(1, g_current_pattern);
    EEPROM.update(2, g_brightness);
}

//----------------------------------------------------------------------------------
// Setup & loop
//----------------------------------------------------------------------------------

void setup() {
   // Leds
  FastLED.addLeds<WS2811, LEDS_PIN, RGB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  #ifdef USB_POWERED
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
  #endif

  #ifdef LED_KEYCHAIN
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(LED_EN_PIN, OUTPUT);
  digitalWrite(LED_EN_PIN, LOW);
  delay(2000);
  digitalWrite(LED_EN_PIN, HIGH);
  #endif 

  #ifdef HAS_RADIO
  radio.begin();
  radio.setChannel(2);
  radio.setPayloadSize(4);
  radio.setAutoAck(false);
  radio.setCRCLength(RF24_CRC_8);
  radio.openReadingPipe(1,0xE7E7E7E7E7LL);
  radio.startListening();
  #endif

  read_state();
}



void loop() {
  g_now = millis();
  
  // mode button. long press enables auto_cycle, short press changes to next pattern
  btn_mode.poll(
    []() {
      mode_button();
    },
    []() {
       enable_autocycle();
    }
  );

  if (pattern_timer.fired()) {
    next_pattern();
  }

  // brightness pot
  #ifdef BRIGHTNESS_PIN
  uint16_t brightness_raw = analogRead(BRIGHTNESS_PIN);
  g_brightness = map(brightness_raw, 0, 1023, 0, 255);
  #endif

  // radio stuff
  #ifdef HAS_RADIO
  if (radio.available()) {
     radio.read(&event, sizeof(event));

     // sparkle stuff
     if (event.button == 1 || event.button == 15) {
       if (event.event == 1) {
          receive_sparkle(event.pendant_id);
       }
       if (event.event == 2) {
          clear_sparkle(event.pendant_id);
       }
     }

     // keyfob stuff
     // right
     if (event.button == 23) {
        mode_button();
     }
     // left
     if (event.button == 27) {
        enable_autocycle();
     }
     // down
     if (event.button == 29) {
        brightness_down();     
     }
     // up
     if (event.button == 30) {
        brightness_up();
     }
  }
  #endif
     

  // LED Keychain only buttons
  #ifdef LED_KEYCHAIN
  btn_brightness_up.poll(
        /* Brightness UP pressed */
        brightness_up,
        /* Brightness UP held */
        []() {
            if (g_brightness < 0xff) {
                g_brightness++;
                
            }
        }
    );

   btn_brightness_down.poll(
        /* Brightness DOWN pressed */
        brightness_down,
        /* Brightness DOWN held */
        []() {
            if (g_brightness > 0) {
                g_brightness--;
                
            }
        }
    );
  
  // Check for power off
  if (!digitalRead(BUTTON1_PIN)) {
     delay(10);
     if (!digitalRead(BUTTON1_PIN)) {
       leds_sleep();
     }
  }
  
  #endif  

  uint8_t brightness = g_brightness;
  if (pattern_timer.running()) {
    if (pattern_timer.sinceStart() < 300) {
        brightness = scale8(pattern_timer.sinceStart() * 256 / 300, g_brightness);
    }
    if (pattern_timer.untilDone() < 300) {
        brightness = scale8(pattern_timer.untilDone() * 256 / 300, g_brightness);
    }
  }
  FastLED.setBrightness(brightness);

  uint32_t d = (patterns)[g_current_pattern]();

  // add sparkle?
  #ifdef HAS_RADIO
  prune_sparkles(g_now - 1100);
  for (int i=0; i<number_of_sparkles(); i++) {
    if (random8() < 100) {
        leds[ random16(NUM_LEDS) ] += CRGB::White;
    }
  }
  #endif
  
  FastLED.show();
  if (d == 0) d = 1000/120;
  FastLED.delay(d);
}
