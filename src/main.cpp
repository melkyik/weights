#include  "ESP8266WebServerSecure.h"
#include <DNSServer.h>
#include <Arduino.h>
#include <ModbusIP_ESP8266.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

/*
Программа запускает опрос весов HD-150 по Softwareserial на пинах D1 D2
К пинам подключен TTL-232 преобразователь  на микросхеме MAX3232 
Опрос весов идет по упрощеному протоколу CAS AD посылкой команды 05 11 
В ответе помимо системных регистров приходит значение веса в строковом виде в кодировке ASCII

Дополнительно 
подключен менеджер WIfi (библиотекой WiFiManager.h) для настройки и сохранения в память данных о wifi подключении
Для перехода в режим настройки нужно замкнуть землю на D7
подключен Modbus Slave <ModbusIP_ESP8266.h>  где в регистрах 01 02 функции F03 выдается значение веса Float32
в регистре 03 - флаг стабильного веса, в регистре 04 - вес*100 в масштабированом значении INT16

Реализован веб сервер библиотекой   "ESP8266WebServerSecure.h" который выдает строку с весом по запросу /weight 
и в конрневом доступе выдает страницу c текущим весом и обновлением 200мс через Javascript 

*/
#define TRIGGER_PIN D7 //пин сброса вайфай и перехода в режим точки доступа с настройкой WiFi подключения
#define SERIAL_BUFER_SIZE 40

//протокол весов
#define WSOH 1                           //заголовок ответа
#define WSTX 2                           //начало текста
#define WSTA 0x53                        //константа стабильного веса
#define WUSTA 0x55                       //константа нестабильного веса
#define WEOT 4                            //конец текста
#define WETX 3                           //конец ответа
//___________________________________________________

ESP8266WebServer server(80);          //обьявление веб сервера
SoftwareSerial S(D2, D1);             //порт Serial на пинах 
ModbusIP mb;                          //экземпляр модбас


uint8_t scomand[2] = {0x05,0x11};     //комада запроса по протоколу весов


char sbuffer[SERIAL_BUFER_SIZE];      //строка ответа от порта
bool weightstable;                    //стабильный вес флаг
char weightstr[10];                   //строка для веса
float weightval;                      //вес float
uint16_t* mbWeight = reinterpret_cast<uint16_t*>(&weightval); //указатель на weightval но на выходе - массив uint16_t[2] для передачи в модбас

//вспомогательные переменные
uint8_t c;
uint8_t bufLen;           //длина буфера полученых данных 
uint8_t ind;

#define MBLEN 8            //регистров в модбасе
//#define  DEBUG           //ОТЛАДКА MODBUS
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

//буферные переменные
uint8_t counter;        //пульс
ulong lasttick;         //таймер тиков команд 
ulong lastCharTime = 0; //таймаут опроса порта

#define TICK_TIME 500   //время посылки команды в весы


//#################################################
//WEB SERVER HANDLERS

void handleRoot() {           // Обработчик запроса клиента по корневому адресу, настройка интерфейса
#define BUFFER_SIZE     1000 //c запасом пока 
  char temp[BUFFER_SIZE];
snprintf(temp, BUFFER_SIZE-1,
"<html><head><style>.c{display:flex;justify-content:center;align-items:center;height:100vh;}"\
".v{font-size:80px;}</style></head><body><div class=\"c\"><div class=\"v\" id=\"w\"></div></div>"\
"<script>function f(){fetch('/weight').then(r=>r.text()).then(d=>{document.getElementById('w').innerText=d;});}"\
"f();setInterval(f,1e2);</script></body></html>"
);
  server.send(200, F("text/html; charset=utf-8"), temp);
} 


void handleNotFound() { // Обрабатываем  ошибку 404
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
  // Устанавливаем обработчики веб. Можно сделать двумя способами:

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
                      //реализация таймаута ответа от порта 200мс
                      while ((S.available() > 0) ||  ((millis() - lastCharTime) < 200) && (ind < SERIAL_BUFER_SIZE-1)) {
                          if (S.available() > 0) {
                              sbuffer[ind++] = S.read();
                              lastCharTime = millis();             // timer restart
                          }
                      }
                      bufLen=ind;
      
                //вывод строки ответа в HEX
               for(int i=0; (i<SERIAL_BUFER_SIZE) && (sbuffer[i]!=0) ;i++){
                              Serial.print(sbuffer[i],HEX);
                              Serial.print(" ");
                              }
             //парсинг строки ответа от весов но только при длинном ответе
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
   