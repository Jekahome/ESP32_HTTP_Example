
## ESP32 HTTP Server

#### 1.Настройка WiFi static IP

#### 2.HTTP Server GET/POST 

#### 3.HTTP Authentication 

#### 4.Работа с image,favicon в HTML разметке

#### 5.Обработка запросов javascript клиента (зажечь LED)
     
(https://esp32tutorials.com/esp32-web-server-esp-idf/)

#### 6.Парсинг JSON запросов

#### 7.Настроить HTTPS

#### 8.Настроить MQTT Password Authentication

```
HTTPS

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
$ mosquitto -c custom.conf

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
$ mosquitto -c custom.conf

Файл конфигурации custom.conf:
    listener 1883 # 1883 default
    allow_anonymous false # аутентификация
    password_file /etc/mosquitto/auth_password
    max_packet_size 1000 # default максимальный размер сообщения в 268_435_456 байт

Подписка с хоста на '/topic/esp32_to_all':
$ mosquitto_sub -u Jeka -P 1234 -t '/topic/esp32_to_all'

Отправка сообщения с хоста в '/topic/all_to_esp32: 
$ mosquitto_sub -u Jeka -P 1234 -t '/topic/all_to_esp32' -m "hi"

Так же можно подписаться на topic из удаленного хоста
$ mosquitto_sub -h mqtt.eclipseprojects.io -t '/test'
```