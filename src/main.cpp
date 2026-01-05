#include <Arduino.h>
#include <Bounce2.h>

// Pin definitions
const int SWITCH_PIN = D2;  // GPIO switch pin
const int LED_PIN = D10;    // GPIO LED pin

// Debounce object
Bounce2::Button button = Bounce2::Button();

// Variable to track LED state
bool ledState = false;

void setup() {
  // Initialize Serial for logging
  Se