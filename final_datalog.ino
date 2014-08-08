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

File dataFile, logFile;

const int maxLineCount = 30000;

boolean canDownload = false;

unsigned short getDigitalData() {
    unsigned short data = 0, val;  
    for(int pin = 22; pin <= 37; pin++) {
        val = digitalRead(pin);
        data |= val << (pin - 22);
    }
    return data;
}

int tempDelay() {
    int data = 0, val;  
    for(int pin = 22; pin <= 37; pin++) {
        val = 1;
        data |= val << (pin - 22);
    }
    return data;
}

void logData() {
    static int lineCount = 0;
    if(lineCount < maxLineCount) {
        lineCount++;
        //char data[13] = { '\0' };
        String data = "";
        String temp = "";
        int dataIndex = 0;
        for(int i = 0; i < 4; i++) {
            unsigned short digitalData = getDigitalData(); 
            data = data + digitalData;
            if(i < 3)
              data += ',';
        }
        
        //the following loop is for delay
        for(int i = 0; i < 8; i++) {
            temp = String(data) + "\t" + tempDelay(); 
        }
        logFile.println(data);
    } else {
        logFile.close();
        Serial.println("closing logFile");
        Timer3.detachInterrupt();
        canDownload = true; 
    }
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
    SD.remove("log1.txt");
    delay(500);
    Serial.println("creating log file");
    logFile = SD.open("log1.txt", FILE_WRITE);
    
    for(int pin = 22; pin <= 37; pin++) {
        pinMode(pin, INPUT);      
    }
    delay(1000);
    Serial.println("calling interrupt");
    Timer3.attachInterrupt(logData).start(500);
    delay(1000);
}

void loop()
{
    EthernetClient client = server.available();  // try to get client

    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank && canDownload) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type: text/html");
                    //client.println("Content-disposition: attachment; filename=log1.txt");
                    client.println("Connection: close");
                    client.println();
                    // send web page
                    dataFile = SD.open("log1.txt");        
                    if (dataFile) {
                        int bytesAvailable = 0;
                        while(bytesAvailable = dataFile.available()) {
                            int i;
                            for(i = 0; i < DATA_BUF_SIZE && i < bytesAvailable; i++) {
                                buf[i] = dataFile.read();
                            }
                            client.write(buf, i);
                            //client.write(dataFile.read()); 
                            c = client.read(); // to read commands while sending file
                        }
                        dataFile.close();
                    }
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } 
        } 
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}
