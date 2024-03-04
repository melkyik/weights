#include  "ESP8266WebServerSecure.h"
#include <DNSServer.h>
#include <Arduino.h>
#include <ModbusIP_ESP8266.h>
//#include <SoftwareSerial.h>
//SoftwareSerial S(13, 15);

const char* ssid = "Laba"; // Указываем имя существующей точки доступа
const char* password = "iFarmiFarm"; // Указываем пароль существующей точки доступа

ESP8266WebServer server(80);
ModbusIP mb;
#define LEN 2 //регистров в модбасе
//#define  DEBUG
#ifdef DEBUG
uint16_t cbRead(TRegister* reg, uint16_t val) {
  Serial.print("Read. Reg RAW#: ");
  Serial.print(reg->address.address);
  Serial.print(" Old: ");
  Serial.print(reg->value);
  Serial.print(" New: ");
  Serial.println(val);
  return val;
}
uint16_t cbWrite(TRegister* reg, uint16_t val) {
  Serial.print("Write. Reg RAW#: ");
  Serial.print(reg->address.address);
  Serial.print(" Old: ");
  Serial.print(reg->value);
  Serial.print(" New: ");
  Serial.println(val);
  return val;
}



bool cbConn(IPAddress ip) {
  Serial.println(ip);
  return true;
}
#endif


uint8_t counter;
ulong lasttick;
#define TICK_TIME 1000

void handleRoot() { // Обработчик запроса клиента по корневому адресу
  #define BUFFER_SIZE     1000
  char temp[BUFFER_SIZE];
snprintf(temp, BUFFER_SIZE-1,
"<html><head><style>.c{display:flex;justify-content:center;align-items:center;height:100vh;}"\
".v{font-size:80px;}</style></head><body><div class=\"c\"><div class=\"v\" id=\"w\"></div></div>"\
"<script>function f(){fetch('/weight').then(r=>r.text()).then(d=>{document.getElementById('w').innerText=d;});}"\
"f();setInterval(f,1e2);</script></body></html>"
);
  server.send(200, F("text/html; charset=utf-8"), temp);
}


void handleNotFound() { // Обрабатываем небезызвестную ошибку 404
  String message = "Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}



//#################################################3
void setup(void) {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA); // Устанавливаем Wi-Fi модуль в режим клиента (STA)
  WiFi.begin(ssid, password); // Устанавливаем ssid и пароль от сети, подключаемся
  
  while (WiFi.status() != WL_CONNECTED) { // Ожидаем подключения к Wi-Fi
    delay(500);
    Serial.print(".");
  }
  
  // Выводим информацию о подключении
  Serial.println("");
  Serial.print("Подключено к ");
  Serial.println(ssid);
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());

#ifdef DEBUG
  mb.onConnect(cbConn); 
#endif

  mb.server();

  if (!mb.addHreg(0, 0xF0F0, LEN)) Serial.println("Error"); // Add Hregs
  #ifdef DEBUG
  mb.onGetHreg(0, cbRead, LEN); // Add callback on Coils value get
  mb.onSetHreg(0, cbWrite, LEN);
  #endif
  // Устанавливаем обработчики. Можно сделать двумя способами:

  server.on("/", handleRoot);

  server.on("/weight", []() {

  server.send(200, F("text/plain"), String(counter)+"kg"); 

  });

  server.onNotFound(handleNotFound); // Вызывается, когда обработчик не назначен

  // Запускаем сервер
  server.begin();
  Serial.println("HTTP-сервер запущен");
}

void loop(void) {
  server.handleClient();
  if (millis() - lasttick >TICK_TIME ) {
        counter++; 
        mb.Hreg(1,counter);
        lasttick=millis();}
 
     mb.task();
     delay(10);
}