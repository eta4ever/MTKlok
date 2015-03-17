#include <LDateTime.h> // для линкитовых часов
#include <LWiFi.h> // для вайфайки
#include <LWiFiUdp.h> // для вайфайки UDP

// для работы с SD-картой
#include <LTask.h>
#include <LSD.h>
#include <LStorage.h>

#include <nixie.h>

// экземпляр класса работы с модулем часов
// Data pin 2, CLK pin 3
Nixie nixie(2, 3); 

// Текущая отображаемая минута. Сравнивать с минутой текущего времени,
// если не совпадает - обновлять индикацию
byte displayingMin;

// это "местный" currentTime, не путать его с тем, что в объекте
datetimeInfo currentTime;

bool effectDisplayed = false; // флаг показанного эффекта рандомных цифр
bool ntpSynced = false; // флаг успешно проведенной синхронизации

void setup()
{
    // загрузить конфигурацию с SD
    nixie.loadConfig("/config/main.txt");

    // подключиться к Wi-Fi
    nixie.wifiConnect();

    // начальная NTP-синхронизация, долбиться до победного
    bool sync_success = nixie.NTPSync();
    while (!sync_success){
        delay(10000);
        sync_success = nixie.NTPSync();
    }

    // прочитать время в переменную currentTime
    LDateTime.getTime(&currentTime);
}

void loop()
{
    displayingMin = currentTime.min; // сохранить отображаемую минуту
    nixie.outputTime(currentTime); // вывести время

    // пока отображаемая минута совпадает с реальной, проверять реальные раз в секунду
    while( displayingMin == currentTime.min )
    {
        LDateTime.getTime(&currentTime); // прочитать время в переменную currentTime    
        delay(1000);
    }
    
    // раз в 10 минут один раз прокрутить эффект рандомных цифр
    if ( ( ( (currentTime.min % 10) == 5) ) && !effectDisplayed ){
        nixie.digitShuffle(20,100);
        effectDisplayed = true; // эффект покан
    }
    
    if ( (currentTime.min % 10) == 6){ 
        effectDisplayed = false; // через минуту сбросить флаг показа эффекта
    }

    // раз в 1 час пытаемся синхронизировать время, 5 попыток
    if ( ( ( currentTime.min == 0) ) && !ntpSynced ){
        bool sync_success = nixie.NTPSync();
        for (char tryCount = 0; tryCount<5; tryCount++)
        {
            sync_success = nixie.NTPSync();
            if (sync_success) break;
            delay(2500);
        }
        ntpSynced = true; // установить флаг удачной синхронизации
    }
    
    if ( currentTime.min == 1){ // через минуту сбросить флаг удачной синхронизации
        ntpSynced = false;  
    }


}
