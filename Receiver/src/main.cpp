#include <Arduino.h>

#include <SPI.h>
#include <LoRa.h>

const int csPin = 5;     // LoRa radio chip select
const int resetPin = 14; // LoRa radio reset
const int irqPin = 2;    // change for your board; must be a hardware interrupt pin

#include "credential.h"

#define DEBUG
#define _DISABLE_TLS_
#include <WiFi.h>
#include <ThingerESP32.h>

ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);
String lora_device_name(LORA_DEVICE_NAME);

unsigned long liter;
int batt;
int prssi;
int rssi;
float psnr;
long pfe;
bool newmsg = false;

void onReceive(int packetSize) {
  int pipe = 0;
  String incoming = "";
  String head = "";
  String sliter = "";
  String sbatt = "";

  // read packet
  for (int i = 0; i < packetSize; i++) {
    char c = (char)LoRa.read();

    incoming += c;
    if (c == '|') {
      pipe++;
    } else {
      switch (pipe) {
      case 0:
        head += c;
        break;
      case 1:
        sliter += c;
        break;
      case 2:
        sbatt += c;
        break;
      }
    }
  }

  prssi = LoRa.packetRssi();
  rssi = LoRa.rssi();
  psnr = LoRa.packetSnr();

  Serial.print("Message:[");
  Serial.print(incoming);
  Serial.print("] pRSSI:");
  Serial.print(prssi);
  Serial.print(" RSSI:");
  Serial.print(rssi);
  Serial.print(" SNR:");
  Serial.println(psnr);

  if (head.compareTo(lora_device_name) == 0) {
    liter = atoi(sliter.c_str());
    batt = atoi(sbatt.c_str());
    Serial.print("Message:[");
    Serial.print(incoming);
    Serial.print("] liter:");
    Serial.print(liter);
    Serial.print(" batt: ");
    Serial.println(batt);
    newmsg = true;
  }
}

void setup() {

  Serial.begin(9600); // initialize serial
  while (!Serial)
    ;

  LoRa.setPins(csPin, resetPin, irqPin); // set CS, reset, IRQ pin
  LoRa.setSpreadingFactor(12);
  if (!LoRa.begin(868E6))  { // initialize ratio at 868 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ; // if failed, do nothing
  }
  LoRa.onReceive(onReceive);

  // put the radio into receive mode
  Serial.println("LoRa init succeeded.");
  thing.add_wifi(WIFI_SSID, WIFI_PWD);
  LoRa.receive();
}

void loop() {
  thing.handle();

  if (newmsg) {
    Serial.println("Sending data to Thinger.io...");
    pson data;
    data["liter"] = liter;
    data["batt"] = batt;
    data["prssi"] = prssi;
    data["rssi"] = rssi;
    data["psnr"] = psnr;
    newmsg = !thing.write_bucket(BUCKET_ID, data);

    Serial.print("Data sent [");
    Serial.print(newmsg);
    Serial.println("]");
  }
}
