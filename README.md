
## ESP32 HTTP Server

#### 1. Настройка WiFi static IP

#### 2. HTTP Server GET/POST 

#### 3. HTTP Authentication 

#### 4. Работа с image,favicon в HTML разметке

#### 5. [Обработка запросов javascript клиента (зажечь LED)](https://esp32tutorials.com/esp32-web-server-esp-idf/)

#### 6. Парсинг HTTP JSON запросов

#### 7. Настроить протокол HTTPS

#### 8. [Настроить протокол MQTT с Password Authentication](http://www.steves-internet-guide.com/mqtt-security-mechanisms/)

9. MQTT шифрование собщений
(Использовать ассиметричное шифрование (закрытый/открытый ключ) написать свой шифратор и дешифратор)

10. Настроить протокол [Modbus](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/modbus.html)

11. Настроить протокол [COAP](https://www.youtube.com/watch?v=pfG8uEDZj5g&list=PLuudOZcE9EgmUtYjYNZz0fhncQPbTBFLU&index=21)

------------------------------------------------------------------------------------------------------------

```
4. Работа с image,favicon в HTML разметке

Способ передать картинку(png,favicon,...) в html
1. Конвертация image в base64 https://www.base64-image.de/ и <img src="data:image/png;base64,iVBOR..."">
2. Включить в сбоку двоичный файл idf_component_register(... EMBED_FILES "winter.png" ...) и использовать asm(...)

Способ передать html разметку    
1. Формат текста  const char index_html[] = R"rawliteral(<!DOCTYPE HTML><html><head>...)rawliteral"; httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
2. Фoрмат base64 
   Конвертировать из строки в `HEX` https://www.rapidtables.com/convert/number/ascii-to-hex.html с разделителем `,0x`
   const unsigned char hex_html[] = {0x3C,0x21,0x44.....};
   httpd_resp_send(req,  (char*)hex_html, HTTPD_RESP_USE_STRLEN);

Конвертировать из строки в `HEX` https://www.rapidtables.com/convert/number/ascii-to-hex.html с разделителем `,0x`
Использовать формат HEX система более компактней чем строку. HEX это 16 ричная система и упаковывает набор байтов
так как один символ char это 1 байт т.е. 8 бит помещается 0-255 значений,а в одно значение HEX 2 байта т.е. 16 бит помещается 0-65536 значений
 
```
------------------------------------------------------------------------------------------------------------

```
7. Настроить HTTPS

Отключить защиту в Chrome
Chrome->Настройки->Конфиденциальность и безопасность ->Безопасность->Безопасный просмотр(Защита отключена)

Самоподписанный сертификат

$ openssl req -newkey rsa:2048 -nodes -x509 -days 3650 -keyout .../certs/prvtkey.pem -out .../certs/cacert.pem -subj "/CN=ESP32 HTTPS server example"

И в menuconfig Enable ESP HTTPS server component


openssl : это базовый инструмент командной строки для создания и управления сертификатами OpenSSL, ключами и другими файлами.

req : обнаружение субкоманды опасности, что мы хотим использовать управление запросами подписки на сертификаты X.509 (CSR). X.509 — это стандарт открытых ключей, наличие SSL и TLS для управления ключами и сертификатами. Вы хотите создать новый сертификат X.509, и поэтому рекомендуется использовать эту субкоманду.

-x509 : это добавляет изменяет предыдущую субкоманду, сообщает утилиту, что мы хотим создать самоподписанный сертификат, а не сгенерировать запрос на подпись сертификата, как обычно происходит.

-nodes : этот параметр ограничения OpenSSL пропускает проверку защиты сертификата с использованием пароля. Для чтения этого файла при объявлении сервера без вмешательства пользователя нам неизбежно Apache. Кодовая фраза может предотвратить это, потому что нам удается вызвать ее после каждого перезапуска.

-days 365 : указанный параметр назначения срока, в течение которого сертификат будет считаться действительным. Здесь мы фиксируем срок действия в один год.

-newkey rsa:2048 : событие, которое мы хотим создать новый сертификат и новый ключ вместе. Мы не встречаем требуемый ключ для подписки сертификата на походе, и поэтому нам необходимо создать вместе с сертификатом. Часть rsa:2048потрясений, что мы создаем ключ RSA длиной 2048 бит.

-keyout : эта ошибка OpenSSL, где мы разместим уничтоженный закрытый ключ.

-out : заданное ограничение OpenSSL, куда поместить запрещенный сертификат.

```
------------------------------------------------------------------------------------------------------------

```
8. Настроить MQTT Password Authentication

Брокер MQTT -  Mosquitto MQTT Broker

Использование облачного брокера позволяет нескольким устройствам IoT (например, платы ESP32/ESP8266) взаимодействовать друг с другом используя протокол MQTT, даже если они работают в разных сетях и разнесены в пространстве.

ДЛЯ ЧЕГО НУЖЕН ПРОТОКОЛ MQTT?
MQTT или Message Queue Telemetry Transport – это легкий, компактный и открытый протокол обмена данными созданный для передачи данных на удалённых локациях, где требуется небольшой размер кода и есть ограничения по пропускной способности канала.

Брокер MQTT в основном отвечает за получение всех сообщений, отправленных по этому протоколу, их фильтрацию, определение получателей и передачу их конечным устройствам.

Средний пакет MQTT имеет ~ 3 байта учета. Ответ HTTP-заголовка имеет размер >200 байт и обычно сообщается в КБ, а не в байтах.

Я считаю, что лучший способ - использовать шифрование полезной нагрузки.
http://www.steves-internet-guide.com/encrypting-the-mqtt-payload-python-example/
https://stevesnoderedguide.com/encrypting-decrypting-mqtt-payloads

----------------------------------------------------------------------------------
Install:
http://www.steves-internet-guide.com/install-mosquitto-linux/
http://www.steves-internet-guide.com/mqtt-username-password-example/
 
sudo apt-add-repository ppa:mosquitto-dev/mosquitto-ppa
sudo apt-get update
sudo apt-get install mosquitto
sudo apt-get install mosquitto-clients
sudo apt clean

----------------------------------------------------------------------------------
Команды:

$ man mosquitto.conf
$ sudo service mosquitto restart
$ sudo service mosquitto stop
$ sudo kill $(ps aux |awk '/mosquitto/ {print $2}')

Запуск своим конфигурационным файлом:
$ mosquitto -v -c custom.conf

Проверка работоспособности:
$ sudo netstat -tunlp
$ netstat -at
    tcp        0      0 localhost:1883          0.0.0.0:*               LISTEN 
    tcp6       0      0 ip6-localhost:1883      [::]:*                  LISTEN 
  
----------------------------------------------------------------------------------
Конфигурация /etc/mosquitto/mosquitto.conf 
Локальная конфигурация в /etc/mosquitto/conf.d/ (файлы .conf)
Бинарный файл /usr/sbin/mosquitto
 
----------------------------------------------------------------------------------
Аутентификация 
http://www.steves-internet-guide.com/mqtt-username-password-example/
https://www.v-elite.ru/node/38

файл с паролем auth_password:
Jeka:1234

...теперь шифруем ...
$ mosquitto_passwd -U auth_password
...получаем в файле auth_password
Jeka:$7$101$mxCut/f4xW/EZ...

...далее скопировать файл паролей auth_password в папку /etc/mosquitto
$ cp -a auth_password /etc/mosquitto

... и настроить /etc/mosquitto/mosquitto.conf...
per_listener_settings true
allow_anonymous false
password_file /etc/mosquitto/auth_password

...далее при внесении изменений в файл паролей следует перегрузить конфигурацию...
...найти PID mosquitto...
$ ps -a
$ kill-HUP <PID>

...добавление нового пользователя в файл паролей...
$ mosquitto_passwd -b passwordfile <user> <password>

...удалить пользователя...
mosquitto_passwd -D passwordfile <user>

Запуск брокера с файлом паролем
$ mosquitto -c custom.conf

Запуск прослушивания с вводом пароля
$ mosquitto_sub -u Jeka -P 1234 -t '/test'

Отправка сообщения
$ mosquitto_pub -u Jeka -P 1234 -t '/test' -m "Hi"

----------------------------------------------------------------------------------

Запуск брокера на хосте:
$ mosquitto -v -c custom.conf

Файл конфигурации custom.conf:
    listener 1883 # 1883 default
    allow_anonymous false # аутентификация
    password_file /etc/mosquitto/auth_password
    max_packet_size 1000 # default максимальный размер сообщения в 268_435_456 байт
    # use client --id-prefix
    clientid_prefixes J1- 

Подписка с хоста на '/topic/esp32_to_all':
$ mosquitto_sub -u Jeka -P 1234 -t '/topic/esp32_to_all' --id-prefix J1-

Отправка сообщения с хоста в '/topic/all_to_esp32: 
$ mosquitto_sub -t '/topic/esp32_to_all' -u Jeka -P 1234 --id-prefix J1- -m "hi"

Так же можно подписаться на topic из удаленного хоста
$ mosquitto_sub -h mqtt.eclipseprojects.io -t '/test'

-----------------------------------------------------------------------------------

Sniffing use Wireshark

Install Wireshark:
https://linuxhint.com/install_wireshark_ubuntu/
$ sudo apt update
$ sudo apt install -y wireshark # для запуска Wireshark без root-доступа
$ sudo usermod -aG wireshark $(whoami)
$ sudo reboot # перезагрузка системы


Логин и пароль видны в пакете в Info:Connect Command:

    №:141	
    Time:57.246318668	
    Source:192.168.1.166	
    Destination:192.168.1.168
    Protocol:MQTT	
    Length:94	
    Info:Connect Command
    Payload:
    HEX to TEXT ... 4a 31 2d 00 04 4a 65 6b 61 00 04 31 32 33 34   J1-..Jeka..1234
     

Сообщение в Info:Publish Message [/topic/esp32_to_all]:

    №:164	
    Time:20.608313553	
    Source:192.168.1.166	
    Destination:192.168.1.168	
    Protocol:MQTT	
    Length:94	
    Info:Publish Message [/topic/esp32_to_all]
    Payload:.... 2f 74 6f 70 69 63 2f 65 73 70 33 32 5f 74 6f 5f 61 6c 6c 31 31 31
        Потверждение HEX to TEXT Шестнадцатеричный код в текст
        https://www.hextotext.net/ru/convert-hex-to-text

        2f746f7069632f65737033325f746f5f616c6c313131 => /topic/esp32_to_all111



Пример отправки пакетов на ESP32 IP:192.168.1.166 с хост машины IP:192.168.1.168:

№   Time        Source          Destination     Protocol  Length  Info
43	9.323998232	192.168.1.166	192.168.1.168	MQTT	  94      Connect Command
payload:
...  4a 65 6b 61 00 04 31 32 33 34          Jeka..1234



№   Time        Source          Destination     Protocol  Length  Info
15	5.602416198	192.168.1.168	192.168.1.166	MQTT	  82	  Publish Message [/topic/all_to_esp32]
payload:
...  
2f 74 6f 70 69 63 2f 61 6c 6c 5f 74 6f 5f 65 73 70 33 32 34 34 34  /topic/all_to_esp32444
 
```
