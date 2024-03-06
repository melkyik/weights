/**
 * OnDemandConfigPortal.ino
 * example of running the configPortal AP manually, independantly from the captiveportal
 * trigger pin will start a configPortal AP for 120 seconds then turn it off.
 * 
 */
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

// select which pin will trigger the configuration portal when set to LOW
#define TRIGGER_PIN D5

int timeout = 120; // seconds to run for

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  WiFi.begin();
  Serial.begin(115200);
  Serial.println("\n Starting");
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  while (WiFi.status() != WL_CONNECTED) { // Ожидаем подключения к Wi-Fi
    delay(500);
    Serial.print(".");
  }
  
  // Выводим информацию о подключении
  Serial.println("");
  Serial.print("Подключено к ");
  Serial.println(WiFi.SSID());
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());

}

void loop() {
  // is configuration portal requested?
  if ( digitalRead(TRIGGER_PIN) == LOW) {
    WiFiManager wm;    

    //reset settings - for testing
    //wm.resetSettings();
  
    // set configportal timeout
    wm.setConfigPortalTimeout(timeout);

    if (!wm.startConfigPortal("")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

  }

  // put your main code here, to run repeatedly:
}