/*
	Библиотека для работы Linkit One 
	с железным модулем AY-Serial (Atmega8 + AY-3-8912) https://github.com/eta4ever/ay-serial
	Распространяется по лицензии GNU GPL V3. eta4ever, 2015
*/

#include <Arduino.h>

// для работы с SD-картой
#include <LTask.h>
#include <LStorage.h>
#include <LSD.h>

#include <AYSerial.h>

LFile _YMFile;

// почему-то объявление большого массива в функции (когда уже будет известен его объем)
// приводит к зависанию. А так ок, даже если здесь дать размер в разы больше нужного
// даем тут "400 Кб хватит всем". По факту, редко бывают распакованные треки и больше. 
// Они в пролете.
byte registerDumpInterleaved[400000]; 

// флаг остановки воспроизведения.
bool stopFlag = false; 

byte _resetPin; // пин сброса AY-atmega

// конструктор
AYSerial::AYSerial()
{

}

// инициализация
void AYSerial::init(byte resetPin)
{
	Serial1.begin(38400); // открыть соединение Serial1 (пины 0-1)
	_resetPin = resetPin;
	pinMode(_resetPin, OUTPUT); // /RESET pin

	// SD
	pinMode(10, OUTPUT); // так в примере было, пока не трогаем
    LTask.begin();
    Drv.begin();
}

// закрыть соединение
void AYSerial::close()
{
	Serial1.end();
}

// открыть файл, прочитать в толстый массив, воспроизвести
void AYSerial::play(String filePath)
{
    
	// Serial.begin(9600);
	// while(!Serial.available());

	// Serial.println("start");

    //открыть файл для чтения
    
	// сконвертировать String в char[]
    int filePathLength = filePath.length();
    char filePathChar[filePathLength+1];
    filePath.toCharArray(filePathChar, filePathLength+1);

    // а теперь уже открыть
    _YMFile = Drv.open(filePathChar);

    // Serial.println(filePathChar);

    //разобрать заголовок

    readSkip(12); // пропустить ID файла и проверочную строку
    _frameCount = readLong(); // кол-во фреймов
	readSkip(4); // какие-то атрибуты
	int digidrumCount = readInt(); // кол-во семплов digidrum
    readSkip(12); // частота YM, частота обновления, loop frame, кол-во доп.байт заголовка

	//пропустить семплы digidrums
	for (int digidrum=0; digidrum < digidrumCount; digidrum++)
	{
		long samplesize = readLong(); // размер сэмпла
		readSkip(samplesize); // сэмпл
	}

	String trackName = readNTString(); // название трека
	String trackAuthor = readNTString(); // автор
	String trackComment = readNTString(); // комментарий

	// Serial.println(trackName);

	// прочитаем все подряд в массив, чередование будет на выводе
	//byte registerDumpInterleaved[_frameCount*16]; // НЕ РАБОТАЕТ!

	for (unsigned int currByte = 0; currByte < _frameCount*16; currByte++)
	{
		registerDumpInterleaved[currByte] = _YMFile.read();
	}

	// Serial.println("Playing");

	// ресетить Atmega-AY
	digitalWrite(_resetPin, LOW);
	delay(500);
	digitalWrite(_resetPin, HIGH);
	delay(500);

	//и записывать по 16 байт в Serial1
	for (unsigned int currFrame = 0; currFrame < _frameCount; currFrame++)
	{
		for (int currRegister = 0; currRegister <16; currRegister++)
		{
			byte registerState = registerDumpInterleaved[currFrame + currRegister * _frameCount];
			Serial1.write(registerState); // не забывать! write - двоично, print - символьно!!!
		}
		delay(20);

		// проверить флаг остановки
		if (stopFlag)
		{
			break;
		}

	}

	// заглушить по окончанию воспроизведения
	silence();

	// сбросить флаг
	stopFlag = false;	

}

String AYSerial::readNTString()
// прочитать Null-terminated String из файла
{
	String currString = "";
	byte currChar = _YMFile.read();

	while (currChar != 0)
	{
		currString += char(currChar);
		currChar = _YMFile.read();
	}
	return currString;
}

long AYSerial::readLong()
// прочитать long (4 байта) из файла
{
	long result = 0;
	for (int byteCount = 3; byteCount >= 0; byteCount--)
	{
		byte currByte = _YMFile.read();
		result += (currByte << (byteCount * 8));
	}

	return result;
}

int AYSerial::readInt()
// прочитать int (2 байта) из файла
{
	int result = 0;
	for (int byteCount = 1; byteCount >= 0; byteCount--)
	{
		byte currByte = _YMFile.read();
		result += (currByte << (byteCount * 8));
	}

	return result;
}

void AYSerial::readSkip(int numBytes)
// пропустить некоторое количество байт при последовательном чтении файла
{
	_YMFile.seek(_YMFile.position() + numBytes);
}

void AYSerial::silence()
// записать тишину
{
	byte zeroState = 0;
	for (int currRegister=0; currRegister<16; currRegister++) 
	{
		Serial1.write(zeroState);
	}
}