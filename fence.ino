#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include "htmls.h"
#include <WebSocketsServer.h>
#include "base64_utils.h"

#define DEBUG_ESP_WIFI
#define DHTTYPE DHT11   // DHT 11
#define SHORT_PULSE 500
#define LONG_PULSE 5000
#define LEN_BUF 1024
#define TCP_PORT 14550
#define WS_PORT 14551
#define SERIAL_BAUDRATE 115200
#define SW_SERIAL_BAUDRATE 38400 // do not exceed 38400 on esp8266
#define HTTP_PORT 80
#define SW_TTY_TX 12
#define SW_TTY_RX 13

#define UP HIGH
#define DW LOW

#define HELP "\n\
resource  | output                | args\n\
getstate  | json status           | no args \n\
setstate  | manual rele & status  | 1st:\"on\" switch on, \"off\" switch off\n\
WifiSET   | set new ssid & pass   | 1st=ssid, 2nd=password\n\
You can connect a telnet client at TCP:14550\n"


const char* def_ssid = "SetupThing";
const char* def_password = "SetupThing";

ESP8266WebServer server(HTTP_PORT);
WiFiServer serverTCP( TCP_PORT );
WebSocketsServer webSocket = WebSocketsServer(WS_PORT);
SoftwareSerial sw_tty;

const int readpin = 4;
const int writepin= 5;
int mtempi=0;
int TIME=30000;
float mtemp[8];
char achBuf[LEN_BUF];
size_t sBytesAvailible;
size_t sBytesRead;
int currentWS=-1;


void setup() {
	char	*buf;
	char	curr_ssid[32];
	char	curr_password[32];
	char	*ssid=curr_ssid, *password=curr_password;
	int	addr=0, cnt;

	memset(ssid, 0, 32);
	memset(password, 0, 32);
	mtempi=0;
	for (int i=0;i<8;i++) mtemp[i]=0;
	Serial.begin(SERIAL_BAUDRATE);
	sw_tty.begin(SW_SERIAL_BAUDRATE, SWSERIAL_8N1, SW_TTY_RX, SW_TTY_TX, false);
	delay(10);
	pinMode(writepin,OUTPUT);
	pinMode(readpin,INPUT);
	EEPROM.begin(512);
	if (EEPROM.read(511)==0xaa) {
		Serial.println("eeprom wifi conf exists!");
		while (EEPROM.read(addr)!='\0') {
			curr_ssid[addr]=EEPROM.read(addr);
			addr++;
			}
		curr_ssid[addr]='\0';    
		addr=32;
		while (EEPROM.read(addr)!='\0') {
			curr_password[addr-32]=EEPROM.read(addr);
			addr++;
			}
		curr_password[addr-32]='\0';
		password=curr_password;
		ssid=curr_ssid;
		} else {
			Serial.println("eeprom wifi conf does nor exist!");
			ssid=(char *)def_ssid;
			password=(char *)def_password;
			}
	Serial.println("Settings:");     
	Serial.println(ssid);
	Serial.println(password);
	buf=(char *)ssid;

	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);
	Serial.flush();
	WiFi.begin(ssid, password);

	cnt=0;
	while (WiFi.status() != WL_CONNECTED) {
		if (cnt==65) {
			buf=(char *)def_ssid;
			WiFi.begin(def_ssid, def_password);
			Serial.println("Bakcup default");
			}
		delay(500);
		Serial.print(".");
		cnt++;
		}
	Serial.println("");
	Serial.println("WiFi connected");
	serverTCP.begin();
	server.begin();
	Serial.println("Web server running. Waiting for the ESP IP...");
	delay(10000);

	Serial.println(WiFi.localIP());
	IPAddress x=WiFi.localIP();
	char *buf2=(char *)calloc(20,1);
	for (int i=0;i<4;i++) {itoa(x[i],buf2+strlen(buf2),10);buf2[strlen(buf2)]='.';}
	buf2[strlen(buf2)-1]='\x00';
	Serial.println(buf2);
	delay(2000);
	server.on("/help", MainHelp);
	server.on("/getstate", getstate);
	server.on("/setstate", button);
	server.on("/WifiSET", WifiSET);

	server.on("/on.gif", serveon);
	server.on("/off.gif", serveoff);
  server.on("/wsconsole", servewsconsole);
	server.on("/setup", servesetup);
	server.on("/", servehome);
	server.on("/css", servecss);
	server.onNotFound(handleNotFound);
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void serve_static(const char *f, int fsize, const char *content_type){
	Serial.println("Serving static content:");
	server.sendHeader("Content-Encoding", "gzip");
	server.send_P(200, content_type, f, fsize);
}

void serve_static_img(const char *f, int fsize){
	Serial.println("Serving static img:");
	server.send_P(200, "image/gif", f, fsize);
}

void serveoff(){
	Serial.println("serve off");
	serve_static_img(off_icon, sizeof(off_icon));
}

void serveon(){
	Serial.println("serve on");
	serve_static_img(on_icon, sizeof(on_icon));
}

void servehome(){
	Serial.println("serve home");
	serve_static(home_html, sizeof(home_html), "text/html");
}


void servecss(){
	Serial.println("serve CSS");
	serve_static(style_css, sizeof(style_css), "text/css");
}


void servesetup(){
  Serial.println("serve setup");
  serve_static(setup_html, sizeof(setup_html), "text/html");
}
void servewsconsole(){
  Serial.println("serve wsconsole");
  serve_static(wsconsole_html, sizeof(wsconsole_html), "text/html");
}

void MainHelp()
{
	server.send(200, "text/plain",HELP);
}


void WifiSET(){
	//Serial.println("WifiSET");
	String ssid = server.arg(0);
	String password = server.arg(1);
	String s="OK";

	if ((ssid.length()<32)&&(password.length()<32)&&(ssid.length()>5)&&(password.length()>5)){
		for (int i=0; i<32;i++){
			if (i<ssid.length()) {
				EEPROM.write(i, ssid[i]);
				} else { 
					EEPROM.write(i, 0);
					}
			}
		for (int i=0; i<32;i++){
			if (i<password.length()) {
				EEPROM.write(i+32, password[i]);
				} else { 
					EEPROM.write(i+32, 0);
					}
			}
		EEPROM.write(511, 0xaa);
		EEPROM.commit();
		server.send(200, "text/plain",s);
		delay(1500);
		ESP.restart();
		} else {
			s="invalid\n";
			server.send(200, "text/plain",s);
			}
}

void handleNotFound(){
	server.send(404, "text/plain","You hit a page that does not exist.\n");
}

void button(){
	Serial.println("button");
	String stat = server.arg(0);
	String s;
	s="not supported!\n";
	if (stat=="pulse") {//
		s="sent pulse";
		digitalWrite(writepin, UP);
		delay(SHORT_PULSE);
		digitalWrite(writepin, DW);
		}
	if (stat=="force") {
		s="sent force";
		digitalWrite(writepin, UP);
		delay(LONG_PULSE);
		digitalWrite(writepin, DW);
		}
	server.send(200, "text/plain",s);
}


void getstate(){
	//Serial.println("getstate");
	String s;
	int val;

	val = digitalRead(readpin);
	if (val==1) s="off"; 
		else s="on";
	server.send(200, "text/plain",s);
}

void raw_telnet(){
	WiFiClient client = serverTCP.available();
	if (client) {
		Serial.println("client attached");
		while ( client.connected() ) {
			server.handleClient();
			if ( ( sBytesAvailible = client.available() ) > 0 ) {
				if ( sBytesAvailible > LEN_BUF ) sBytesAvailible = LEN_BUF;
					sBytesRead = client.readBytes( &achBuf[0], sBytesAvailible );
					sw_tty.write( &achBuf[0], sBytesRead );
					}
				if ( ( sBytesAvailible = sw_tty.available() ) > 0 ) {
					if (sBytesAvailible > LEN_BUF) sBytesAvailible = LEN_BUF;
					sBytesRead = sw_tty.readBytes( &achBuf[0], sBytesAvailible );
					client.write( &achBuf[0], sBytesRead );
					}
				}
    Serial.println("client disconnected");
		}
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

Serial.printf("webSocketEvent num=%d, type=%d\n", num, type);
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            currentWS=-1;
            break;
        case WStype_CONNECTED:{
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            webSocket.sendTXT(num, "Connected");
            currentWS=num;
            }
            break;
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);
            sw_tty.write(payload, length);
            sw_tty.write("\n", 1);
            break;
        case WStype_BIN:
            Serial.printf("[%u] get binary length: %u\n", num, length);
            break;
    }
}
void wssend(){
  char b64_buf[512];
  int  b64sz;
  
    if (currentWS>=0){
          if ( ( sBytesAvailible = sw_tty.available() ) > 0 ) {
               if (sBytesAvailible > LEN_BUF-1) sBytesAvailible = LEN_BUF-1;
               sBytesRead = sw_tty.readBytes( &achBuf[0], sBytesAvailible );
               achBuf[sBytesRead-1]='\0';
               Serial.println("#1");
               Serial.println(achBuf);
               b64sz=b64_encode(b64_buf, achBuf, sBytesRead);
               Serial.println("#2");
               Serial.println(b64_buf);
               String t = String(b64_buf);
               Serial.println("#3");
               Serial.println(t);
               Serial.printf("[%u] get Text: %s\n", currentWS, achBuf);
               webSocket.sendTXT(currentWS, t );
               }
          }
}

void loop() {
	server.handleClient();
	raw_telnet();
  webSocket.loop();
  wssend();
}
