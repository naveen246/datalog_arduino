#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <DueTimer.h>

#define DATA_BUF_SIZE 2048

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x10, 0xCB };
IPAddress ip(169,254,101,102); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80

byte tick = 0;

byte buf[DATA_BUF_SIZE];
byte logBuf[DATA_BUF_SIZE];

int logBufIndex = 0;

File dataFile, logFile;

String dataBuf = "";
int lCount = 0, fCount = 1;

const int maxLineCount = 60000;
const int maxFileCount = 4;

byte canDownload[maxFileCount] = { 0 };

EthernetClient client;

byte getHiByte() {
    byte data = 0, val;
    for(int pin = 22; pin <= 29; pin++) {
        val = digitalRead(pin);
        data |= val << (pin - 22);
    }
    return data;
}

byte getLowByte() {
    byte data = 0, val;
    for(int pin = 30; pin <= 37; pin++) {
        val = digitalRead(pin);
        data |= val << (pin - 30);
    }
    return data;
}

int tempDelay() {
    int data = 0, val;  
    for(int i = 0; i < 6; i++) {
        val = 1;
        data |= val << (i);
    }
    return data;
}


void logFastData() {
    char fileName[9] = { "log1.txt" };
    byte hiByte, loByte;
    if(lCount == 0) {
        if(fCount == 1) {
            logBufIndex = 0; 
        }
        fileName[3] = fCount + 48;
        logFile = SD.open(fileName, FILE_WRITE);
    }
    
    if(lCount < maxLineCount) {
        lCount++;
        String temp = "";
        for(int i = 0; i < 4; i++) {
            logBuf[logBufIndex] = getHiByte();
            logBufIndex++;
            logBuf[logBufIndex] = getLowByte();
            logBufIndex++;
            //delay code follows
            for(int i = 0; i < 3; i++) {
                temp = String(hiByte) + "\t" + tempDelay(); 
            }
            //logFile.write(logBuf[logBufIndex]);
            //logFile.write(logBuf[logBufIndex+1]);
        }
        if(logBufIndex >= DATA_BUF_SIZE || lCount >= maxLineCount) {
            logFile.write(logBuf, logBufIndex);
            logBufIndex = 0;
        }
    }else {
        logFile.close();
        canDownload[fCount - 1] = 1;
        lCount = 0;
        fCount++;
        if(fCount > maxFileCount) {
            Timer3.detachInterrupt();
            Serial.println("detaching interrupt ");
        }
        Serial.println("closing logFile ");
    }
}

void sendFastLog() {
    char fileName[9] = { "log1.txt" };
    for(byte fileCount = 1; fileCount <= maxFileCount; fileCount++) {
        while(canDownload[fileCount - 1] == 0){
           Serial.println("cannot download " + String(fileCount));
           delay(2000);
        }   
        Serial.println("can download " + String(fileCount));
        fileName[3] = fileCount + 48;
        dataFile = SD.open(fileName);     
        if (dataFile) {
            int bytesAvailable = 0;
            while(bytesAvailable = dataFile.available()) {
                int i;
                for(i = 0; i < DATA_BUF_SIZE && i < bytesAvailable; i++) {
                    buf[i] = dataFile.read();
                }
                client.write(buf, i);
                //client.write(dataFile.read()); 
            }
            dataFile.close();
        }
    } 
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
    
    for(int pin = 22; pin <= 37; pin++) {
        pinMode(pin, INPUT);      
    }
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
                    String request = client.readStringUntil('#');
                    Serial.println(request);
                    //while(client.findUntil("log", "\n\r"))
                        //char logType = client.read(); // f or n
                
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type: text/html");
                    client.println("Content-disposition: attachment; filename=log1.txt");
                    client.println("Connection: close");
                    client.println();
                    
                    //send data
                    if(request.indexOf("log=f") != -1) {
                        deleteLogFiles();
                        lCount = 0; 
                        fCount = 1;
                        for(int i = 0; i < maxFileCount; i++) {
                            canDownload[i] = 0;  
                        }
                        Timer3.attachInterrupt(logFastData).start(500);
                        delay(1000);
                        sendFastLog();  
                    }
                    break;
                }
            } 
        } 
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}
