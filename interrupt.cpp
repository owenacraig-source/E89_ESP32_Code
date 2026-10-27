/*
  Arduino Nano ESP32 (ESP32-S3) - External Interrupt Example

  Wiring:
    - Connect a push button between pin D2 and GND.
    - The internal pull-up resistor is enabled, so the pin reads HIGH
      when idle and LOW when the button is pressed.

  Behavior:
    - On a FALLING edge (HIGH -> LOW), the ISR sets a flag.
    - The main loop sees the flag and prints a message to Serial.
*/

#include <Arduino.h>

// ---------- Configuration ----------
const uint8_t INTERRUPT_PIN = D2;           // Digital input pin for interrupt
const uint32_t DEBOUNCE_MS  = 50;           // Debounce time in milliseconds

// ---------- Shared variables (ISR <-> loop) ----------
volatile bool     interruptFlag   = false;  // Set by ISR, cleared by loop
volatile uint32_t interruptCount  = 0;      // Total accepted interrupts
volatile uint32_t lastInterruptMs = 0;      // For debouncing

// Spinlock for safely accessing shared variables on the dual-core ESP32
portMUX_TYPE isrMux = portMUX_INITIALIZER_UNLOCKED;

// ---------- Interrupt Service Routine ----------
// IRAM_ATTR places the ISR in internal RAM so it runs fast and reliably.
// Keep ISRs short: no Serial.print(), no delay(), no heavy work.
void IRAM_ATTR handleInterrupt() {
  uint32_t now = millis();

  // spinlock to protect shared variables
  portENTER_CRITICAL_ISR(&isrMux);
  if (now - lastInterruptMs >= DEBOUNCE_MS) {
    lastInterruptMs = now;
    interruptCount++;
    interruptFlag = true;
  }
  portEXIT_CRITICAL_ISR(&isrMux);
}

void setup() {
  Serial.begin(115200);

  // Nano ESP32 uses native USB; wait briefly for the Serial Monitor to connect
  uint32_t start = millis();
  while (!Serial && (millis() - start < 3000)) {
    delay(10);
  }

  Serial.println();
  Serial.println("Arduino Nano ESP32 - Interrupt Example");

  // Configure the pin as input with internal pull-up
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);

  // Attach the interrupt. Options: RISING, FALLING, CHANGE, ONLOW, ONHIGH
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), handleInterrupt, FALLING);

  Serial.print("Interrupt attached to pin D2. Waiting for events...");
  Serial.println();
}

void loop() {
  bool     triggered = false;
  uint32_t count     = 0;
  uint32_t timeMs    = 0;

  // spinlock to protect shared variables
  portENTER_CRITICAL(&isrMux);
  if (interruptFlag) {
    interruptFlag = false;
    triggered = true;
    count     = interruptCount;
    timeMs    = lastInterruptMs;
  }
  portEXIT_CRITICAL(&isrMux);

  // Do the slow work (Serial printing) outside the ISR
  if (triggered) {
    Serial.print("Interrupt detected! Count: ");
    Serial.print(count);
    Serial.print("  |  Time: ");
    Serial.print(timeMs);
    Serial.println(" ms");
  }

  // Other non-blocking code can go here
}