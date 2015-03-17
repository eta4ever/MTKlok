/*
	Библиотека для работы Linkit One 
	с железным модулем AY-Serial (Atmega8 + AY-3-8912) https://github.com/eta4ever/ay-serial
	Распространяется по лицензии GNU GPL V3. eta4ever, 2015
*/

#ifndef AYSerial_h
#define AYSerial_h

#include <Arduino.h>

// для работы с SD-картой
#include <LTask.h>
#include <LStorage.h>
#include <LSD.h>

#define Drv LSD // использовать SD карту, а не внутренний флеш

class AYSerial
{
public:
	AYSerial(); // конструктор
	void init(byte resetPin); // открыть последовательный порт, выполнить инициализацию, назначить RESET
	void close(); // закрыть порт
	void play(String filePath); // воспроизвести YM-файл
	void stop(); // прекратить воспроизведение

private:
	String readNTString(); // прочитать null-terminated строку из файла
	long readLong(); // прочитать long (4 байта) из файла
	int readInt(); // прочитать int (2 байта) из файла
	void readSkip(int numBytes); // пропустить некоторое количество байт при последовательном чтении файла
	void silence(); // заглушить, при открытом соединении!

	LFile _YMFile; // экземпляр класса для работы с файлом YM
	unsigned long _frameCount; // количество фреймов в файле
	byte _resetPin; // пин сброса AY-Atmega
};

#endif