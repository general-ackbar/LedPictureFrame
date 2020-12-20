//WIFI & connectivity
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <WiFiSettings.h>

//Matrix
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include "wifi.h"


#define SERIAL_OUT 1
#define localPort  8888

// Define matrix data pin, width, height & brightness.
#define PIN 0
#define mw 8
#define mh 8
#define BRIGHTNESS 24



WiFiUDP Udp;
Adafruit_NeoMatrix *matrix = new Adafruit_NeoMatrix(mw, mh, PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_RIGHT +
  NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1];    //buffer to hold incoming packet,
char ReplyBuffer[] = "OK\r";                      // a string to send back

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  matrix->begin();
  matrix->setBrightness(BRIGHTNESS);
  displayReady();

  startManager();
}

void loop() {
  MDNS.update();
 
  int packetSize = Udp.parsePacket();
  if (packetSize) {

    // read the packet into packetBufffer
    int n = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    packetBuffer[n] = 0;
    if(SERIAL_OUT) Serial.print("Length: ");
    if(SERIAL_OUT) Serial.println(n);

    if(packetBuffer[0] == 'L' && packetBuffer[1] == 'M' && packetBuffer[2] == 'I') 
    {
      readLMI(packetBuffer, n);
    } else if(packetBuffer[0] == 'c' && packetBuffer[1] == 'm' && packetBuffer[2] == 'd') {
      //CMD1 = clear, CMD2XX = set brightness in hex
      if(packetBuffer[3] == '0')  { sendInfo(); }
      if(packetBuffer[3] == '1')  { matrix->clear(); matrix->show(); }
      if(packetBuffer[3] == '2')  {  matrix->setBrightness( ( (char2int(packetBuffer[4])<<4) | (char2int(packetBuffer[5])) )); matrix->show(); }      
    } else if(packetBuffer[0] == 'p' && packetBuffer[1] == 'o' && packetBuffer[2] == 's') {
      //POS0[XX][YY][RR][GG][BB] or POS1[XX][YY][RGB656]
      int x = (char2int(packetBuffer[4])<<4) | (char2int(packetBuffer[5]));                    
      int y = (char2int(packetBuffer[6])<<4) | (char2int(packetBuffer[7])); 

      if(packetBuffer[3] == '0') { //Colorspace in RGB888
        uint8_t colorR = (char2int(packetBuffer[8])<<4) | (char2int(packetBuffer[9])); 
        uint8_t colorG = (char2int(packetBuffer[10])<<4) | (char2int(packetBuffer[11])); 
        uint8_t colorB = (char2int(packetBuffer[12])<<4) | (char2int(packetBuffer[13]));
        matrix->drawPixel(x, y, matrix->Color(colorR,colorG,colorB)); 
      }
      else if(packetBuffer[3] == '1') { //Colorspace in RGB565
        uint8_t hiByte = (char2int(packetBuffer[8])<<4) | (char2int(packetBuffer[9])); 
        uint8_t loByte = (char2int(packetBuffer[10])<<4) | (char2int(packetBuffer[11]));
        uint16_t rgb565 = ((uint16_t)hiByte << 8) | loByte;
        matrix->drawPixel(x, y, rgb565); 
      }
      matrix->show();
    } 
    
    // send a reply, to the IP address and port that sent us the packet we received
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(ReplyBuffer);
    Udp.endPacket();
  }

}


static inline uint8_t char2int(char Ch)
{
    Ch = toupper(Ch);
    return(Ch>='0'&&Ch<='9')?(Ch-'0'):(Ch-'A'+10);
}

void sendInfo()
{
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  char displaySize[3];
  itoa(mw, displaySize, 10);
  
  Udp.write(displaySize); 
  Udp.write("\r\n"); 
  Udp.endPacket();
  
  }

void displayReady()
{
  matrix->clear();
  matrix->drawPixel(0, 0, matrix->Color(255,0,0));
  matrix->show();
  delay(500);
  matrix->drawPixel(0, 0, matrix->Color(0,255,0));
  matrix->show();
  delay(500);
  matrix->drawPixel(0, 0, matrix->Color(0,0,255));
  matrix->show();
  delay(1000);
  matrix->clear();
  matrix->show();
  }

void wifiReady()
{
  matrix->clear();
  matrix->drawRGBBitmap(
         (matrix->width()  - wifi_w ) / 2,
         (matrix->height() - wifi_h) / 2,
         wifi, wifi_w, wifi_h);
  matrix->show();  
  delay(1000);
  matrix->clear();
  matrix->show(); 
}
  
int wait_x = 0, wait_y = 0;
void startManager(){
  
    WiFiSettings.onSuccess  = []() {
      Serial.print("Connected! IP address: ");
      Serial.println(WiFi.localIP());
      Serial.printf("UDP server on port %d\n", localPort);
      Udp.begin(localPort);
      startMDNS();
      wifiReady();
    };
    
    WiFiSettings.onFailure  = []() {
        Serial.println("Connection failed");
    };
    
    WiFiSettings.onWaitLoop = []() {
      Serial.print('.');
      matrix->drawPixel(wait_x, wait_y, matrix->Color(0,0,255));    
      matrix->show();  
      wait_x++;
      if(wait_x == mw) { wait_y++; wait_x=0; }    
      return 500; // Delay next function call by 500ms
    };

    String mdns_hostname = String("frame_") + String(ESP.getChipId(), HEX);

    
    
    WiFiSettings.string( "frame_name", mdns_hostname);    
    WiFiSettings.hostname = mdns_hostname;    
    WiFiSettings.connect(true, 30);
}

/*
bool startWiFi() { 
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  int x=0,y=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print('.');
    matrix->drawPixel(x, y, matrix->Color(0,0,255));    
    matrix->show();  
    x++;
    if(x == mw) { y++; x=0; }    
  }
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  matrix->clear();
  matrix->drawRGBBitmap(
              (matrix->width()  - wifi_w ) / 2,
              (matrix->height() - wifi_h) / 2,
              wifi, wifi_w, wifi_h);
  matrix->show();  
  delay(1000);
  matrix->clear();
  matrix->show(); 

  return true;
}
*/


bool startMDNS() {

  String uid = String(ESP.getChipId(), HEX).substring(3);
  String mdns_hostname = WiFiSettings.string("frame_name");
  //String mdns_hostname = String("frame_") + uid;
  //Serial.println(stored_name);
  /*
  if(stored_name != "")
    mdns_hostname = String(stored_name);
  */
  

  if (MDNS.begin(mdns_hostname.c_str())) {
    Serial.print("mDNS responder started: ");
    Serial.println(mdns_hostname);
    MDNS.addService("matrixframe", "udp", localPort);     
    /*
    matrix->setCursor(0, 0);
    matrix->print(uid.c_str()[0]);
    matrix->show();
    delay(1500);
    matrix->clear();
    matrix->setCursor(0, 0);
    matrix->print(uid.c_str()[1]);
    matrix->show();
    delay(1500);
    matrix->clear();
    matrix->setCursor(0, 0);
    matrix->print(uid.c_str()[2]);
    matrix->show();
    delay(1500);
    */
    matrix->clear();
    matrix->show();
  } else {
    Serial.println("Error setting up MDNS responder!");
    return false;
  }

  return true;
}

void readLMI(char *packetBuffer, int packetLength )
{
  if(SERIAL_OUT) Serial.println("[LMI data begin]");
      int headerSize = packetBuffer[4]; //Predefined for LMI
      int data_length = packetLength-headerSize-1;
      
      int w = (packetBuffer[5]<<4) | packetBuffer[6];                    
      int h = (packetBuffer[7]<<4) | packetBuffer[8]; 

      int frameLength = w*h; //a uint16_t is 2 bytes
      if(SERIAL_OUT) Serial.print("Frame length: ");
      if(SERIAL_OUT) Serial.println(frameLength);


      uint16_t *image;
      image = new uint16_t[frameLength];
      
      if(SERIAL_OUT) Serial.print("Dimensions: ");
      if(SERIAL_OUT) Serial.print(w);
      if(SERIAL_OUT) Serial.print("x");
      if(SERIAL_OUT) Serial.println(h);
      int offset, currentIndex = 0;
      
      for(int i = 0; i < data_length; i+=2){
          offset = i + headerSize;          
          uint16_t rgb565 = ((uint16_t)packetBuffer[offset] << 8) | packetBuffer[offset+1];          
          image[currentIndex] = rgb565;
          currentIndex++;

          if(currentIndex == frameLength)
          {            
            Serial.println("Frame done");            
            matrix->drawRGBBitmap(
              (matrix->width()  - w ) / 2,
              (matrix->height() - h) / 2,
              image, w, h);
            matrix->show();
            delete[] image;
            image = new uint16_t[frameLength];
            currentIndex = 0;
            
          }
      }
      
      if(SERIAL_OUT) Serial.println("[LMI data end]"); 
  }

/*
  test (shell/netcat):
  --------------------
    nc -u 192.168.esp.address 8888
    cat something.png | netcat -u server.ip.here 8888

*/
