#include <FastLED.h>

#define NUM_LEDS 64
#define DATA_PIN 11
#define CLOCK_PIN 13

// ปุ่ม (ต่อแบบ INPUT_PULLUP: ปุ่มชอร์ตลง GND)
#define BTN2 2
#define BTN3 3
#define BTN4 4

CRGB leds[NUM_LEDS];

// สำหรับกระพริบแบบไม่บล็อก
unsigned long lastToggle = 0;
const unsigned long blinkInterval = 900;  // ms
bool blinkOn = false;

// ฟังก์ชันช่วย
void showColor(const CRGB& c) {
  static CRGB last = CRGB::Black;
  if (last != c) {
    fill_solid(leds, NUM_LEDS, c);
    FastLED.show();
    last = c;
  }
}

void setup() {
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
  FastLED.setBrightness(50);

  pinMode(BTN2, INPUT);
  pinMode(BTN3, INPUT);
  pinMode(BTN4, INPUT);

  showColor(CRGB::Black);  // ดับไฟเริ่มต้น
  Serial.begin(9600);
}

void loop() {
  // อ่านปุ่ม (กด = LOW) แล้วกลับทิศให้เป็น true เมื่อ "กด"
  bool b2 = digitalRead(BTN2);
  bool b3 = digitalRead(BTN3);
  bool b4 = digitalRead(BTN4);

/*
  Serial.print(b2);
  Serial.print(",");
  Serial.print(b3);
  Serial.print(",");
  Serial.println(b4);
*/
  // จับคู่สถานะเหมือนโค้ดเดิมของคุณ
  if (!b2 && !b3 && !b4) {
    // 0 0 0 → ดับทั้งหมด
    showColor(CRGB::Black);
  } else if (!b2 && !b3 && b4) {
    // 0 0 1 → เขียว
    showColor(CRGB::Green);
  } else if (!b2 && b3 && !b4) {
    // 0 1 0 → เหลือง
    showColor(CRGB::Yellow);
  } else if (!b2 && b3 && b4) {
    // 0 1 1 → แดง
    showColor(CRGB::Red);
  } else if (b2 && !b3 && !b4) {
    // 1 0 0 → แดงกระพริบแบบไม่บล็อก
    unsigned long now = millis();
    if (now - lastToggle >= blinkInterval) {
      lastToggle = now;
      blinkOn = !blinkOn;
      showColor(blinkOn ? CRGB::Red : CRGB::Black);
    }
  } else {
    // เคสอื่น ๆ ที่ไม่ได้กำหนด → ดับไฟเพื่อความชัวร์
    showColor(CRGB::Black);
  }
}