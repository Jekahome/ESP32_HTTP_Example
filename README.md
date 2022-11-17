
## ESP32 HTTP Server

```
Настройка WiFi static IP

HTTP Server GET/POST 

Проверка Аутентификации 

Картинки,favicon в HTML разметке

Обработка запросов от javascript клиента (зажечь LED)
https://esp32tutorials.com/esp32-web-server-esp-idf/

Парсинг JSON запросов

HTTP Authentication

MQTT Password Authentication ?
```

```
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

```