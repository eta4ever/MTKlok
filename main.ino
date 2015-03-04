#include <LDateTime.h> // для линкитовых часов
#include <LWiFi.h> // для вайфайки
#include <LWiFiUdp.h> // для вайфайки UDP

//-------------------ДЛЯ WIFI NTP------------------------------------
char ssid[] = "";  //  your network SSID (name)
char pass[] = "";       // your network password
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
LWiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP
//--------------------------------------------------------------------


// пины для последовательного вывода времени и тактирования
char nixieData = 2;
char nixieCLK = 3;

// время
datetimeInfo currentTime;
char timezone = 3; // часовой пояс
char displayingMin; // хранить отображаемую минуту, сравнивать с текущей, при несовпадении обновлять

// флаг показанного эффекта
bool effectDisplayed = false;

// флаг произведенной синхронизации
bool ntpSynced = false;

void setup()
{
	//-------------WIFI------------------------------------
	// attempt to connect to Wifi network:
	LWiFi.begin();
	while (!LWiFi.connectWPA(ssid, pass))
	{
	  delay(1000);
	}
    delay(10000);
	Udp.begin(localPort);
	//----------------------------------------------------


	// установить режимы выходов
	pinMode(nixieData, OUTPUT);
	pinMode(nixieCLK, OUTPUT);
	digitalWrite(nixieCLK, LOW);

        // начальное значение часов
        LDateTime.getTime(&currentTime);
        currentTime.hour = 0;
        currentTime.min = 0;
        LDateTime.setTime(&currentTime);
        outputTime(currentTime);

        // начальная NTP-синхронизация, долбимся до победного
        bool sync_success = NTPsync();
        while (!sync_success){
        	delay(10000);
        	sync_success = NTPsync();
        }


}

void loop()
{
	displayingMin = currentTime.min; // сохранить отображаемую минуту
	outputTime(currentTime); // вывести время

	// пока отображаемая минута совпадает с реальной, проверять реальные раз в секунду
	while( displayingMin == currentTime.min )
	{
		LDateTime.getTime(&currentTime); // прочитать время в переменную currentTime	
		delay(1000);
	}
	
	// раз в 10 минут один раз прокрутить эффект рандомных цифр
	if ( ( ( (currentTime.min % 10) == 5) ) && !effectDisplayed ){
		digitShuffle();
		effectDisplayed = true;
	}
	
	if ( (currentTime.min % 10) == 1){
		effectDisplayed = false;  
	}

	// раз в 1 час пытаемся синхронизировать время, 5 попыток
	if ( ( ( currentTime.min == 0) ) && !ntpSynced ){
		bool sync_success = NTPsync();
		for (char tryCount = 0; tryCount<5; tryCount++)
		{
			sync_success = NTPsync();
			if (sync_success) break;
			delay(2500);
		}
		ntpSynced = true;
	}
	
	if ( currentTime.min == 1){
		ntpSynced = false;  
	}


}

// вывод времени -----------------------------------------
void outputTime(datetimeInfo currentTime){
	
	// так уж схемотехнически странно получилось. На старших пинах регистров
	// сидят старшие разряды. Поэтому порядок запихивания битов такой:
	// старший полубайт часов, младший часов, старший минут, младший минут.

	char serialHalfBytes[] = {
		currentTime.hour / 10,
		currentTime.hour % 10,
		currentTime.min / 10,
		currentTime.min % 10,
	}

	for (char halfByteCounter = 0; halfBytecounter < 4; halfBytecounter++)
	{
		for (char bitCounter = 0; bitCounter < 4; bitCounter++)
		{
			//подать нужный бит на вход регистра
			digitalWrite(nixieData, bitRead(serialHalfBytes[halfBytecounter],bitCounter));
			
			// такт загрузки
			delay(1);
			digitalWrite(nixieCLK, HIGH);
			delay(1);
			digitalWrite(nixieCLK, LOW);
			delay(1);

		}
	}
}

// эффект рандомных цифр по всем разрядам в течение 2 секунд -----------------------
void digitShuffle(){
	
	datetimeInfo fakeTime;

	for (char iter=0; iter<20; iter++){
		fakeTime.hour=random(100);
		fakeTime.min=random(100);
		outputTime(fakeTime);
		delay(100);    
	}
}

// эффект прокрутки цифр по всем разрядам от 0 до 10 в течение секунды-------------
void digitCycle(){
	
	datetimeInfo fakeTime;

	for (char iter=0; iter<10; iter++){
		fakeTime.hour=iter*10 + iter;
		fakeTime.min=fakeTime.hour;
		outputTime(fakeTime);
		delay(100);    
	}
}


// сформировать NTP пакет и отправить его по указанному адресу-------------
unsigned long sendNTPpacket(IPAddress& address)
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

// -----------------------------------------------------------------
// отправить пакет, получить ответ, распарсить и установить время
// при успешном получении пакета еще прокрутим циферки от 0 до 9 по всем разрядам
// возвращает true, если пакет обработан, false - если нет
bool NTPsync(){

  bool result = false;
  sendNTPpacket(timeServer); // отправить NTP пакет 
  // подождать ответа
  delay(1000);
  if ( Udp.parsePacket() ) // если получен ответный пакет
  {
    memset(packetBuffer, 0xcd, NTP_PACKET_SIZE);
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // прочитать пакет в буфер

    // timestamp - начинается с 40 байта пакета, длина 4 байта (2 слова)
    // сначала читаем эти два слова 
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // потом объединяем в long и получаем секунды с 1 января 1900
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    // Unix time - это секунды с 1 января 1970. Т.е., для получения UnixTime вычитаем 70 лет
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
    // и вытаскиваем из Unix Time часы и минуты с поправкой на часовой пояс,
    // с коррекцией перехода через полночь при прибавлении пояса (%24)
    currentTime.hour = ((epoch  % 86400L) / 3600 + timezone) % 24; // (86400 секунд в дне)
    currentTime.min = (epoch  % 3600) / 60; // (3600 секунд в минуте)
    
    LDateTime.setTime(&currentTime); // устанавливаем время

    digitCycle(); // эффект прокрутки циферок
    result = true; // флаг обработанного пакета
  }
  return result;
}
