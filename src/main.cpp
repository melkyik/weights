#include  "ESP8266WebServerSecure.h"
#include <DNSServer.h>
#include <Arduino.h>
#include <ModbusIP_ESP8266.h>
#include <SoftwareSerial.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#define TRIGGER_PIN D7 //пин сброса вайфай 
#define SERIAL_BUFER_SIZE 40
//протокол весов
#define WSOH 1         //заголовок ответа
#define WSTX 2         //начало текста
#define WSTA 0x53      //константа стабильного веса
#define WUSTA 0x55     //константа неытабильного веса

#define WEOT 4         //конец текста
#define WETX 3         //конец ответа
ESP8266WebServer server(80);
SoftwareSerial S(D2, D1);
uint8_t scomand[2] = {0x05,0x11}; //комада запроса


char sbuffer[SERIAL_BUFER_SIZE];//строка ответа от порта
bool weightstable;
char weightstr[10];
float weightval;
uint16_t* mbWeight = reinterpret_cast<uint16_t*>(&weightval);
uint8_t c;

uint8_t bufLen; //длина буфера полученых данных 
uint8_t ind;
ModbusIP mb;
#define MBLEN 8 //регистров в модбасе
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
ulong lastCharTime = 0;

#define TICK_TIME 500

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
  S.begin(9600);

  pinMode(TRIGGER_PIN, INPUT_PULLUP);//режим сброса вайфай
  WiFi.mode(WIFI_STA); // Устанавливаем Wi-Fi модуль в режим клиента (STA)
  WiFi.begin(); // Устанавливаем ssid и пароль от сети, подключаемся
  
  while (WiFi.status() != WL_CONNECTED) { // Ожидаем подключения к Wi-Fi
    delay(500);
    Serial.print(".");
  }
  
  // Выводим информацию о подключении
  Serial.println("");
  Serial.print("Подключено к ");
  Serial.println( WiFi.SSID()); // Устанавливаем ssid и пароль от сети, подключаем;
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());

#ifdef DEBUG
  mb.onConnect(cbConn); 
#endif

  mb.server();

  if (!mb.addHreg(0, 0xF0F0, MBLEN)) Serial.println("Error"); // пишем HREGS
  #ifdef DEBUG
  mb.onGetHreg(0, cbRead, LEN); // Add callback on Coils value get
  mb.onSetHreg(0, cbWrite, LEN);
  #endif
  // Устанавливаем обработчики. Можно сделать двумя способами:

  server.on("/", handleRoot);

  server.on("/weight", []() {

  server.send(200, F("text/plain"), String(weightstr)); 

  });

  server.onNotFound(handleNotFound); // Вызывается, когда обработчик не назначен

  // Запускаем сервер
  server.begin();
  Serial.println("HTTP-сервер запущен");
}

void loop(void) {
//обработчик сброса вайфай 
    if ( digitalRead(TRIGGER_PIN) == LOW) {
    WiFiManager wm;    
    //reset settings - for testing
    //wm.resetSettings();
      // set configportal timeout
    wm.setConfigPortalTimeout(120);

    if (!wm.startConfigPortal("")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      ESP.restart();
      delay(5000);
    }
    Serial.println("connected...yeey :)");
    }
    //конец обработчика сброса
  server.handleClient();
  if (millis() - lasttick >TICK_TIME ) {
        counter++; 
        ///передача регистров
        mb.Hreg(0,counter);
        mb.Hreg(1,mbWeight[1]);
        mb.Hreg(2,mbWeight[0]);
        mb.Hreg(3,weightstable);
        mb.Hreg(4,trunc(weightval*100));
        ////////////////
        lasttick=millis();
      
        S.write(scomand,2);
      //  Serial.println("send CMD..");
              }

  
    for(int i=0;i<SERIAL_BUFER_SIZE-1;i++ )  sbuffer[i]=0;

   // unsigned long lastCharTime = 0;
    ind = 0;
    if (S.available() > 0) {          
                      sbuffer[ind++] = S.read();    
                      lastCharTime = millis();           
                      while ((S.available() > 0) ||  ((millis() - lastCharTime) < 200) && (ind < SERIAL_BUFER_SIZE-1)) {
                          if (S.available() > 0) {
                              sbuffer[ind++] = S.read();
                              lastCharTime = millis();             // timer restart
                          }
                      }
                      bufLen=ind;
      

               for(int i=0; (i<SERIAL_BUFER_SIZE) && (sbuffer[i]!=0) ;i++){
                              Serial.print(sbuffer[i],HEX);
                              Serial.print(" ");
                              }

              if ((sbuffer[0]==0x06) && (bufLen>=16)){
                      weightstable=sbuffer[3]==WSTA;
                      c=4;
                      ind=0;
                        while((sbuffer[c]!=3) && (sbuffer[c]!=0)){
                        if (sbuffer[c]!=0x20) weightstr[ind++]=sbuffer[c]; //пропускаем пробелы
                        c++;
                      }
               weightstr[ind-1]=0;
               Serial.println();
               Serial.println(weightstr);
               weightval=atof(weightstr);
            
              // Serial.println(weightstr[strlen(weightstr)-1],DEC);
                Serial.println(weightval);
             }
                Serial.println();
     }

     mb.task();
    delay(5);
}
   