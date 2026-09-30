#include "config.h"
#include "wifi.h"
#include "coin.h"
#include "relay.h"

void setup() {
  Serial.begin(115200);
  setupPins();
  setupWifi();
}

void loop() {
  checkCoin();
  checkTimer();
}
