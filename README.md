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
