/*
Библиотека для работы Linkit One
с модулем индикации на базе ИН-12 с последовательным входом
Использует Wi-Fi для NTP-синхронизации и настройки на SD-карте
Распространяется по лицензии GNU GPL V3. eta4ever, 2015
*/
#ifndef nixie_h
#define nixie_h

#include <Arduino.h>

#include <LDateTime.h> // для линкитовых часов
#include <LWiFi.h> // для вайфайки
#include <LWiFiUdp.h> // для вайфайки UDP

// для работы с SD-картой
#include <LTask.h>
#include <LSD.h>
#include <LStorage.h>

// тип данных конфигурации
typedef struct {int timezone;
                String ssid;
                String password;
                IPAddress timeserver;} nixieConfigType;

class Nixie
{
public:
	Nixie(byte pinData, byte pinCLK); // конструктор
	void loadConfig(String configPath); // загрузка конфигурации с SD-карты
	void wifiConnect(void); // подключиться к сети
	bool NTPSync(void); // синхронизировать время, возвращает успех/неудачу

	void outputTime(datetimeInfo currentTime); // вывод времени
	void digitShuffle(int iterations, int iterationMs); //эффект рандомных цифр
	void digitCycle(int iterationMs); //эффект прокрутки цифр от 0 до 9

private:
	
	// для разбора конфигурации
	String getParamName(String source); // выделить из строки имя параметра (до разделителя)
	String getParamValue(String source); // выделить из строки значение параметра (после разделителя)
	IPAddress stringToIP(String source); // преобразовать строку вида 192.168.0.1 в IPAddress
	
	// сформировать NTP пакет и отправить его по указанному адресу
	unsigned long sendNTPPacket(IPAddress& address);

	nixieConfigType _nixieConfig; // переменная конфигурации
	byte _pinData; // пин линии данных модуля
	byte _pinCLK; // пин тактирования модуля
	datetimeInfo _currentTime; // текущее время
};

#endif