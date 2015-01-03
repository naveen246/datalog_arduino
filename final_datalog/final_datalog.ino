#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <DueTimer.h>

#define DATA_BUF_SIZE 2048
#define BUSY_PIN 13
#define MAX_FILE_COUNT 1
#define FAST_LOG_LINES_PER_SEC 2000

//change maxLineCount to get more data. for 2 min data maxLineCount = 240000
int maxLineCount = 10000;

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x10, 0xCB };
IPAddress ip(169,254,101,102); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80

byte dataWriteBuf[DATA_BUF_SIZE];
byte dataLogBuf[DATA_BUF_SIZE];

int logBufIndex = 0, lineCount = 0, fileCount = 1, milliVolt_plusOneDigit;

File dataFile, logFile;

byte canDownload[MAX_FILE_COUNT] = { 0 };

EthernetClient client;

int tempDelay() {
    int data = 0, val;  
    for(int i = 0; i < 6; i++) {
        val = 1;
        data |= val << (i);
    }
    return data;
}

byte getByte(int lowPin, int hiPin, int adjacentPinsGap) {
    byte data = 0, val;
    for(int pin = lowPin; pin <= hiPin; pin+=adjacentPinsGap) {
        val = digitalRead(pin);
        data |= val << (pin - lowPin) / adjacentPinsGap;
    }
    return data;
}

byte getHiByte() {
    int lowPin = 39, hiPin = 51, adjacentPinsGap = 2;
    return getByte(lowPin, hiPin, adjacentPinsGap);
}

byte getLowByte() {
    int lowPin = 23, hiPin = 37, adjacentPinsGap = 2;
    return getByte(lowPin, hiPin, adjacentPinsGap);
}

void fillDataBuffer(int dataCountPerLine) {
    int analogValue;
    String temp;
    for(int i = 0; i < dataCountPerLine; i++) {
        while(digitalRead(BUSY_PIN) == HIGH);
        analogValue = (getHiByte() << 8) | getLowByte();
        milliVolt_plusOneDigit = (analogValue * 2279.0 / 32767.0) * 10;
        dataLogBuf[logBufIndex] = milliVolt_plusOneDigit >> 8; //hibyte
        logBufIndex++;
        dataLogBuf[logBufIndex] = milliVolt_plusOneDigit & 0xFF; //lobyte
        logBufIndex++;
        //delay code follows
        for(int i = 0; i < 3; i++) {
            temp = String(analogValue) + "\t" + tempDelay(); 
        }
    }
}

void logFastData() {
   char fileName[9] = { "log1.txt" };
    if(lineCount == 0) {
        if(fileCount == 1) {
            logBufIndex = 0; 
        }
        fileName[3] = fileCount + 48;
        logFile = SD.open(fileName, FILE_WRITE);
    }
    
    if(lineCount < maxLineCount) {
        lineCount++;
        String temp = "";

        int dataCountPerLine = 4;
        fillDataBuffer(dataCountPerLine);

        if(logBufIndex >= DATA_BUF_SIZE || lineCount >= maxLineCount) {
            logFile.write(dataLogBuf, logBufIndex);
            logBufIndex = 0;
        }
    } else {
        logFile.close();
        canDownload[fileCount - 1] = 1;
        lineCount = 0;
        fileCount++;
        if(fileCount > MAX_FILE_COUNT) {
            Timer0.detachInterrupt();
            Serial.println("detaching interrupt ");
        }
        Serial.println("closing logFile ");
    }
}

void sendFastLog() {
    char fileName[9] = { "log1.txt" };
    for(byte fCount = 1; fCount <= MAX_FILE_COUNT; fCount++) {
        while(canDownload[fCount - 1] == 0){
           Serial.println("cannot download " + String(fCount));
           delay(2000);
        }   
        Serial.println("can download " + String(fCount));
        fileName[3] = fCount + 48;
        dataFile = SD.open(fileName);     
        if (dataFile) {
            int bytesAvailable = 0;
            while(bytesAvailable = dataFile.available()) {
                int i;
                for(i = 0; i < DATA_BUF_SIZE && i < bytesAvailable; i++) {
                    dataWriteBuf[i] = dataFile.read();
                }
                client.write(dataWriteBuf, i);
                //client.write(dataFile.read()); 
            }
            dataFile.close();
        }
    } 
}

void sendNormalLog() {
    logBufIndex = 0;
    int dataCountPerLine = 16;
    fillDataBuffer(dataCountPerLine);
    client.write(dataLogBuf, logBufIndex);
}

void deleteLogFiles() {
    SD.remove("log1.txt");
    SD.remove("log2.txt");
    SD.remove("log3.txt");
    SD.remove("log4.txt");
    SD.remove("log5.txt");
    SD.remove("log6.txt");
    SD.remove("log7.txt");
    SD.remove("log8.txt");
    delay(1000);  
}

void setup()
{
    Serial.begin(115200);       // for debugging
    delay(500);
    Ethernet.begin(mac, ip);  // initialize Ethernet device
    delay(1000);
    server.begin();           // start to listen for clients
    delay(1000);
    
    // initialize SD card
    Serial.println("Initializing SD card...");
    if (!SD.begin(4)) {
        Serial.println("ERROR - SD card initialization failed!");
        return;    // init failed
    }
    Serial.println("SUCCESS - SD card initialized.");
    
    for(int pin = 23; pin <= 53; pin+=2) {
        pinMode(pin, INPUT);      
    }
        
    pinMode(BUSY_PIN, INPUT); 
    delay(1000);
}

void loop()
{
    client = server.available();  // try to get client

    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                if( client.find("GET ") ) {
                    String request = client.readStringUntil(';');
                    while(client.read() != -1);
                    Serial.println(request);
                
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type: text/html");
                    client.println("Content-disposition: attachment; filename=log1.txt");
                    client.println("Connection: close");
                    client.println();
                    
                    //send data
                    if(request.indexOf("log=f") != -1) {
                        if(request.indexOf("seconds=") != -1) {
                            int startIndex = request.lastIndexOf('=') + 1;
                            int seconds = request.substring(startIndex).toInt();
                            Serial.println(startIndex);
                            Serial.println(seconds);
                            maxLineCount = seconds * FAST_LOG_LINES_PER_SEC;
                        }
                        deleteLogFiles();
                        lineCount = 0; 
                        fileCount = 1;
                        for(int i = 0; i < MAX_FILE_COUNT; i++) {
                            canDownload[i] = 0;  
                        }
                        Timer0.attachInterrupt(logFastData).start(500);
                        delay(1000);
                        sendFastLog();  
                    } else if(request.indexOf("log=n") != -1) {
                        Timer1.attachInterrupt(sendNormalLog).setFrequency(2).start();
                        while(1) {

                        }
                    }
                    break;
                }
            } 
        } 
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}
