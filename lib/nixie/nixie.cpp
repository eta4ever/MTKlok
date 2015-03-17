/*
Библиотека для работы Linkit One
с модулем индикации на базе ИН-12 с последовательным входом
Использует Wi-Fi для NTP-синхронизации и настройки на SD-карте
Распространяется по лицензии GNU GPL V3. eta4ever, 2015
*/
#include <Arduino.h>
#include <LDateTime.h> // для линкитовых часов
#include <LWiFi.h> // для вайфайки
#include <LWiFiUdp.h> // для вайфайки UDP

// для работы с SD-картой
#include <LTask.h>
#include <LSD.h>
#include <LStorage.h>

#include <nixie.h>

//------------------ДЛЯ РАБОТЫ С SD КАРТОЙ----------------------------
#define Drv LSD // использовать SD карту, а не внутренний флеш
LFile configFile; // экземпляр класса для работы с файлом конфига
//--------------------------------------------------------------------

//-------------------ДЛЯ WIFI NTP------------------------------------
// char ssid[] = "";      // название сети, задается в конфиге на SD
// char pass[] = "";   // пароль сети, задается в конфиге на SD
unsigned int localPort = 2390;      // локальный порт для NTP
// IPAddress timeServer; //  IP адрес NTP-сервера, задается в конфиге на SD
const int NTP_PACKET_SIZE = 48; // время NTP в первых 48 байтах пакета
byte packetBuffer[NTP_PACKET_SIZE]; // буфер вход. и выход. пакетов
LWiFiUDP Udp; // экземпляр класса для работы с UDP
//--------------------------------------------------------------------

nixieConfigType _nixieConfig; // переменная конфигурации

byte _nixieData; // пин линии данных модуля
byte _nixieCLK; // пин тактирования модуля

datetimeInfo _currentTime; // текущее время

// Конструктор. Инициализация пинов данных и тактирования,
// инициализация переменных.
Nixie::Nixie(byte nixieData, byte nixieCLK)
{
	_nixieData = nixieData;
	_nixieCLK = nixieCLK;
	pinMode(_nixieData, OUTPUT);
	pinMode(_nixieCLK, OUTPUT);
	digitalWrite(nixieCLK, LOW);
}

// загрузка конфигурации с SD-карты
void Nixie::loadConfig(String configPath)
{
	// преобразовать String в char[]
	char configPathChar[configPath.length()];
	configPath.toCharArray(configPathChar,configPath.length());

	//------------ЗАГРУЗИТЬ КОНФИГУ С SD-------------------
    pinMode(10, OUTPUT); // так в примере было, пока не трогаем
    LTask.begin();
    Drv.begin();

    //открыть файл для чтения
    configFile = Drv.open(configPathChar);
    if (configFile) {
        // Serial.println("Loading config..."); //дебаг
        // отправиться в начало
        configFile.seek(0);
        // и читать, пока файл не закончится
        while (configFile.available()) {            
            
            //прочитать символ, добавить его к буферной строке
            char symbol = configFile.read();
            String fileString = "";
            fileString += symbol;

            // заполнить буферную строку
            while ( (symbol != '\n') && configFile.available() )
            {
                // прочитать символ из файла
                symbol = configFile.read();
                // если символ не EOL, добавить его к строке
                if (symbol != '\n') fileString += symbol; 
            }

            // выделить из строки имя и значение параметра
            String paramName = getParamName(fileString);
            String paramValue = getParamValue(fileString);

            // поскольку мы не можем юзать switch по стрингам, будет
            // такое вот if-else. Это заполнение структуры
            // конфигурации системы

            if (paramName == "timezone") 
            {
                _nixieConfig.timezone = paramValue.toInt();
            }
            else if (paramName == "ssid")
            {
                _nixieConfig.ssid=paramValue;
            }
            else if (paramName == "password")
            {
                _nixieConfig.password=paramValue;
            }
            else if (paramName == "timeserver")
            {
                _nixieConfig.timeserver = stringToIP(paramValue);
            }

        }
        // закрыть файл:
        configFile.close();
    } else {
        // в случае проблемы с файлом не делаем ничего
        // Serial.println("Loading config FAILED"); //дебаг
    }
}

// подключиться к сети
void Nixie::wifiConnect()
{
	LWiFi.begin();

    // Тут вот какая штука. LWiFi.connectWPA String не хочет, ему
    // char подавай. А сделать char сразу в структуре не получилось,
    // т.к. была какая-то мутная бяка, связанная с длиной char в структуре
    // и длиной буфера при конвертации из String при разборке файла.
    // Так что, в структуре String, а здесь конвертим, точно зная,
    // сколько нужно символов.

    char ssid [_nixieConfig.ssid.length()];
    char password [_nixieConfig.password.length()];
    _nixieConfig.ssid.toCharArray(ssid,_nixieConfig.ssid.length());
    _nixieConfig.password.toCharArray(password,_nixieConfig.password.length());

    // Serial.println(ssid);
    // Serial.println(password);
    
    // Подключиться к Wi-Fi, долбиться до победного
    while (!LWiFi.connectWPA(ssid, password))
    {
      delay(1000);
      // Serial.println("Trying Wi-Fi...");
    }
    delay(10000);
    Udp.begin(localPort);
}

// отправить пакет, получить ответ, распарсить и установить время
// при успешном получении пакета еще прокрутим циферки от 0 до 9 по всем разрядам
// возвращает true, если пакет обработан, false - если нет
bool Nixie::NTPSync()
{
  bool result = false;
  sendNTPPacket(_nixieConfig.timeserver); // отправить NTP пакет 
  // подождать ответа
  delay(1000);
  if ( Udp.parsePacket() ) // если получен ответный пакет
  {
    memset(packetBuffer, 0xcd, NTP_PACKET_SIZE);
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // прочитать пакет в буфер

    // timestamp - начинается с 40 байта пакета, длина 4 байта (2 слова)
    // сначала читать эти два слова 
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // потом объединить в long, получаются секунды с 1 января 1900
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    // Unix time - это секунды с 1 января 1970. Т.е., 
    // для получения UnixTime вычесть 70 лет
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
    // и вытащить из Unix Time часы и минуты с поправкой на часовой пояс,
    // с коррекцией перехода через полночь при прибавлении пояса (%24)
    _currentTime.hour = ((epoch  % 86400L) / 3600 + _nixieConfig.timezone) % 24; // (86400 секунд в дне)
    _currentTime.min = (epoch  % 3600) / 60; // (3600 секунд в минуте)
    
    LDateTime.setTime(&_currentTime); // установить время

    digitCycle(100); // эффект прокрутки циферок
    result = true; // флаг обработанного пакета
  }
  return result;
}

// сформировать NTP пакет и отправить его по указанному адресу (для NTPSync)
unsigned long Nixie::sendNTPPacket(IPAddress& address)
{
  // обнулить буфер
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Сформировать ручками пакет
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // а теперь отправить 
  Udp.beginPacket(address, 123); // на указанный адрес, порт 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// вывод времени
void Nixie::outputTime(datetimeInfo currentTime)
{
    
    // так уж схемотехнически странно получилось. На старших пинах регистров
    // сидят старшие разряды. Поэтому порядок запихивания битов такой:
    // старший полубайт часов, младший часов, старший минут, младший минут.

    char serialHalfBytes[] = {_currentTime.hour / 10,
                              _currentTime.hour % 10,
                              _currentTime.min / 10,
                              _currentTime.min % 10};

    for (char halfByteCounter = 0; halfByteCounter < 4; halfByteCounter++)
    {
        for (char bitCounter = 0; bitCounter < 4; bitCounter++)
        {
            //подать нужный бит на вход регистра
            digitalWrite(_nixieData, bitRead(serialHalfBytes[halfByteCounter],bitCounter));
            
            // такт загрузки
            delay(1);
            digitalWrite(_nixieCLK, HIGH);
            delay(1);
            digitalWrite(_nixieCLK, LOW);
            delay(1);
        }
    }
}

// эффект рандомных цифр по всем разрядам
void Nixie::digitShuffle(int iterations, int iterationMs)
{
    
    datetimeInfo fakeTime;

    for (int iter=0; iter<iterations; iter++){
        fakeTime.hour=random(100);
        fakeTime.min=random(100);
        outputTime(fakeTime);
        delay(iterationMs);    
    }
}

// эффект прокрутки цифр по всем разрядам от 0 до 10
void Nixie::digitCycle(int iterationMs)
{
    
    datetimeInfo fakeTime;

    for (char iter=0; iter<10; iter++){
        fakeTime.hour=iter*10 + iter;
        fakeTime.min=fakeTime.hour;
        outputTime(fakeTime);
        delay(iterationMs);    
    }
}

//--------------ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ЗАГРУЗКИ КОНФИГУРАЦИИ------

// выделить из строки имя параметра (до разделителя)
String Nixie::getParamName(String source)
{
    char delimiter = '=';
    // найти первый разделитель, считать его единственным
    int delimiterPos = source.indexOf(delimiter);
    // обрезать строку 
    return (source.substring(0,delimiterPos));
}

// выделить из строки значение параметра (после разделителя)
String Nixie::getParamValue(String source)
{
    char delimiter = '=';
    // найти первый разделитель, считать его единственным
    int delimiterPos = source.indexOf(delimiter);
    // обрезать строку 
    return (source.substring(delimiterPos+1));
}

//преобразовать строку вида 192.168.0.1 в IPAddress
IPAddress Nixie::stringToIP(String source)
{
    String oct[4]; // 4 октета
    int firstDotPos = source.indexOf('.'); // позиция первой точки слева
    oct[3] = source.substring(0,firstDotPos); // старший (левый) октет
    int secondDotPos = source.indexOf('.', firstDotPos+1); // позиция второй точки
    oct[2] = source.substring(firstDotPos+1, secondDotPos); // второй октет
    int thirdDotPos = source.indexOf('.', secondDotPos+1); // позиция третьей точки
    oct[1] = source.substring(secondDotPos+1, thirdDotPos); // первый октет
    oct[0] = source.substring(thirdDotPos+1); // младший (правый) октет

    return IPAddress(oct[3].toInt(), oct[2].toInt(), oct[1].toInt(), oct[0].toInt());
}
