/*
 MIT License

Copyright (c) 2019 Phil Bowles <h4plugins@gmail.com>
   github     https://github.com/philbowles/esparto
   blog       https://8266iot.blogspot.com     
   groups     https://www.facebook.com/groups/esp8266questions/
              https://www.facebook.com/Esparto-Esp8266-Firmware-Support-2338535503093896/
                			  

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include<H4PCommon.h>
#include<H4P_WiFi.h>

#ifndef H4P_NO_WIFI
void H4P_WiFi::_hookIn(){ 
    SPIFFS.begin(); 
    WiFi.onEvent(_wifiEvent);
    H4PluginService::hookFactory([this](){ clear(); });
}

void H4P_WiFi::_greenLight() { 
    _cb[chiptag()]=_getChipID();
    _cb[boardtag()]=replaceAll(H4_BOARD,"ESP8266_","");
    getPersistentValue(devicetag(),"H4_");
    start();                     
}

void H4P_WiFi::_startAP(){
//    if(!_dnsServer){
        _dnsServer= new DNSServer;
//        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.enableSTA(false); // force AP only
        WiFi.softAP(CSTR(_cb[devicetag()]));	
        _dnsServer=new DNSServer;
        _dnsServer->start(53, "*", WiFi.softAPIP());
        h4.every(1000,[this](){ _dnsServer->processNextRequest(); },nullptr,H4P_TRID_WFAP,true);
        _cb["ip"]=WiFi.softAPIP().toString().c_str();
        _scan();
        h4pcConnected();	        
 //   }
}

void H4P_WiFi::getPersistentValue(string v,string prefix){
    string persistent=read("/"+v);
    string cat=_cb[v]+persistent;
    if(persistent.size()){
        if(H4P_PREFER_PERSISTENT) _cb[v]=persistent;  
    }
    if(!cat.size()) _cb[v]=string(prefix)+_cb[chiptag()];
//    Serial.printf("PV %s IS %s\n",CSTR(v),CSTR(_cb[v]));
}

void H4P_WiFi::setPersistentValue(string n,string v,bool reboot){
//    string oldvalue=read("/"+n); // PREFER SAVED
    string oldvalue=_cb[n];
//    Serial.printf("PV %s CURRENT VALUE %s,NEW VALUE %s\n",CSTR(n),CSTR(oldvalue),CSTR(v));
    if(oldvalue!=v){
        write("/"+n,v);
        if(reboot) h4reboot();
    } //else Serial.printf("PV %s UNCHANGED\n",CSTR(n));
}

//  spiffs
string H4P_WiFi::read(const string& fn){
	string rv="";
        File f=SPIFFS.open(CSTR(fn), "r");
        if(f && f.size()) {
            int n=f.size();
            uint8_t* buff=(uint8_t *) malloc(n+1);
            f.readBytes((char*) buff,n);
            rv=stringFromBuff(buff,n);
            free(buff);
        }
        f.close();
	return rv;	
}

string H4P_WiFi::replaceParams(const string& s){ // oh for a working regex...
	int i=0;
	int j=0;
	string rv(s);
	while((i=rv.find("%",i))!=string::npos){
        if(j){
            string var=rv.substr(j+1,i-j-1);
            if(_cb.count(var)) {
                rv.replace(j,i-j+1,_cb[var]); // FIX!!
                rv.shrink_to_fit();
            }
            j=0;
        } else j=i;    
        ++i;
	}
	return rv.c_str();	
}

void H4P_WiFi::write(const string& fn,const string& data){
    File b=SPIFFS.open(CSTR(fn), "w");
    b.print(CSTR(data));
    b.close(); 
}
//
uint32_t H4P_WiFi::_dump(vector<string> vs){
    return guard<1>(vs,[this](vector<string> vs){
        return ([this](string h){ 
            Serial.printf("DUMP FILE %s\n",CSTR(h));
            Serial.printf("%s\n",CSTR(read("/"+h)));
            return H4_CMD_OK;
        })(PAYLOAD);
    });
}

uint32_t H4P_WiFi::_host(vector<string> vs){
    return guard<1>(vs,[this](vector<string> vs){
        return ([this](string h){
            host(h); 
            return H4_CMD_OK;
        })(PAYLOAD);
    });
}

uint32_t H4P_WiFi::_change(vector<string> vs){
    return guard<1>(vs,[this](vector<string> vs){
        auto vg=split(PAYLOAD,",");
        if(vg.size()==2) return ([this](string s,string p){ change(s,p); return H4_CMD_OK; })(vg[0],vg[1]);
        else return H4_CMD_PAYLOAD_FORMAT;
    });
}

void H4P_WiFi::change(string ssid,string psk){ // add device / name?
    stop();
    _cb[ssidtag()]=ssid;
    _cb["psk"]=psk;
    _startSTA();
}

void H4P_WiFi::_lostIP(){
    if(!_discoDone){
        h4pcDisconnected();
        _discoDone=true;
        Serial.println("WiFi down");
    }
}

#ifdef ARDUINO_ARCH_ESP8266

void H4P_WiFi::showSPIFFS(){
    uint32_t sigma=0;
    uint32_t n=0;
    Dir dir = SPIFFS.openDir("/");

    while (dir.next()) {
        n++;
        reply("FILE: %s %u\n",CSTR(dir.fileName()),dir.fileSize());
        sigma+=dir.fileSize();
    }
    reply("\n %d file(s) %u bytes\n",n,sigma);
}

void H4P_WiFi::clear(){ 
	stop();
    WiFi.disconnect(true); 
	ESP.eraseConfig();
    SPIFFS.remove(CSTR(string("/"+string(devicetag()))));
}

void H4P_WiFi::_scan(){ // check 4 common hoist
//    Serial.println("SCAN");
    WiFi.enableSTA(true);
    int n=WiFi.scanNetworks();

    for (uint8_t i = 0; i < n; i++){
        char buf[128];
        snprintf(buf,127,"<option>%s</option>",CSTR(WiFi.SSID(i)));
        _cb["opts"].append(buf); // delete later!!!
    }
    WiFi.scanDelete();
    WiFi.enableSTA(false); // force AP only
}

void H4P_WiFi::_startSTA(){
    WiFi.mode(WIFI_STA);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.enableAP(false); 
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
    WiFi.begin(CSTR(_cb[ssidtag()]),CSTR(_cb["psk"]));
}

void H4P_WiFi::start(){
    if(WiFi.SSID()=="") {
        stop();
        _startAP();
    }
    else if(WiFi.getMode()==WIFI_OFF) _startSTA();
}

void H4P_WiFi::stop(){
    h4.cancelSingleton({H4P_TRID_HOTA,H4P_TRID_WFAP});
    if(WiFi.getMode() & WIFI_AP) {
        if(_dnsServer){
            _dnsServer->stop();
            delete _dnsServer;
            _dnsServer=nullptr;
            h4pcDisconnected();
        }
    }
	WiFi.mode(WIFI_OFF);
}
//
//      H4P_WiFi
//
void H4P_WiFi::_gotIP(){
    _discoDone=false;
    _cb["ip"]=WiFi.localIP().toString().c_str();
    _cb[ssidtag()]=CSTR(WiFi.SSID());
    _cb["psk"]=CSTR(WiFi.psk());

    string host=_cb[devicetag()];

    h4.every(H4WF_OTA_RATE,[](){ ArduinoOTA.handle(); },nullptr,H4P_TRID_HOTA,true);
    WiFi.hostname(CSTR(host));
  	ArduinoOTA.setHostname(CSTR(host));
	ArduinoOTA.setRebootOnSuccess(false);	
	ArduinoOTA.begin();

    Serial.printf("WiFi up ip=%s\n",CSTR(_cb["ip"]));
    h4pcConnected();
}

/*
ESP8266

    WIFI_EVENT_STAMODE_CONNECTED = 0,
    WIFI_EVENT_STAMODE_DISCONNECTED,
    WIFI_EVENT_STAMODE_AUTHMODE_CHANGE,
    WIFI_EVENT_STAMODE_GOT_IP,
    WIFI_EVENT_STAMODE_DHCP_TIMEOUT,
    WIFI_EVENT_SOFTAPMODE_STACONNECTED,
    WIFI_EVENT_SOFTAPMODE_STADISCONNECTED,
    WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED,
    WIFI_EVENT_MAX,
    WIFI_EVENT_ANY = WIFI_EVENT_MAX,
    WIFI_EVENT_MODE_CHANGE

*/
void H4P_WiFi::_wifiEvent(WiFiEvent_t event) {
//    Serial.printf("FH=%d _wifiEvent %d\n",ESP.getFreeHeap(),event);
    switch(event) {
        case WIFI_EVENT_STAMODE_DISCONNECTED:
			h4.queueFunction([](){ h4wifi._lostIP(); });
            break;    
		case WIFI_EVENT_STAMODE_GOT_IP:
			h4.queueFunction([](){ h4wifi._gotIP(); });
			break;
//		default:
//			Serial.printf("FH=%d _wifiEvent %d\n",ESP.getFreeHeap(),event);
//			break;
	}
}
#else
void H4P_WiFi::showSPIFFS(){
    uint32_t sigma=0;
    uint32_t n=0;

    File root = SPIFFS.open("/");
 
    File file = root.openNextFile();
 
    while(file){
        n++;
        reply("FILE: %s %u\n",file.name(),file.size());
        sigma+=file.size();
        file = root.openNextFile();
    }
    reply("\n %d file(s) %u bytes\n",n,sigma);
}

void H4P_WiFi::clear(){
//    Serial.printf("clear\n");
    _stop();
	WiFi.disconnect(true,true);
//    show();
}

void H4P_WiFi::_scan(){
//    WiFi.enableSTA(true);
    int n=WiFi.scanNetworks();
//    Serial.printf("SCAN finds %d networks\n",n);

    for (uint8_t i = 0; i < n; i++){
        //if(WiFi.SSID(i)==CSTR(_cb[ssidtag()])) sel=i;
        char buf[128];
        snprintf(buf,127,"<option>%s</option>",CSTR(WiFi.SSID(i)));
        _cb["opts"].append(buf); // delete later!!!
    }
    WiFi.scanDelete();
    WiFi.enableSTA(false); // force AP only
}
/*
void H4P_WiFi::_startAP(){
//    Serial.printf("_startAP\n");
//    if(!_dnsServer){
        _dnsServer= new DNSServer;
        WiFi.mode(WIFI_AP);
        WiFi.enableSTA(false); // force AP only
        WiFi.softAP(CSTR(_cb[devicetag()]));	
        _dnsServer=new DNSServer;
        _dnsServer->start(53, "*", WiFi.softAPIP());
        h4.every(1000,[this](){ _dnsServer->processNextRequest(); },nullptr,H4P_TRID_WFAP,true);
//        _cb["ip"]=WiFi.softAPIP().toString().c_str();
        _scan();
        h4pcConnected();	        
//    }
}
*/
void H4P_WiFi::_startSTA(){
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.enableAP(false); 
	WiFi.setAutoReconnect(true);
    WiFi.begin(CSTR(_cb[ssidtag()]),CSTR(_cb["psk"]));
}

void H4P_WiFi::start(){
    if(WiFi.begin()){                 
//        Serial.printf("Cannot start status=%d ssid=%s psk=%s\n",WiFi.status(),CSTR(WiFi.SSID()),CSTR(WiFi.psk()));
        if(WiFi.SSID()=="" && WiFi.psk()=="") _startAP();
    }
}

void H4P_WiFi::_stop(){
    h4.cancelSingleton({H4P_TRID_HOTA,H4P_TRID_WFAP}); 
    if(_dnsServer){
//        Serial.printf("AP %s stopping\n",CSTR(_cb[devicetag()]));
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer=nullptr;
        h4pcDisconnected();
    }
}

void H4P_WiFi::stop(){
    _stop();
    WiFi.disconnect(true,false);
}
//
//      H4P_WiFi
//
void H4P_WiFi::_gotIP(){
    _discoDone=false;
    _cb["ip"]=WiFi.localIP().toString().c_str();
    _cb[ssidtag()]=CSTR(WiFi.SSID());
    _cb["psk"]=CSTR(WiFi.psk());

    string host=_cb[devicetag()];

    Serial.printf("WiFi up ip=%s\n",CSTR(_cb["ip"]));
    h4pcConnected();
}
/*
void H4P_WiFi::_lostIP(){
//    Serial.printf("_lostIP\n");
    if(!_discoDone){
        h4pcDisconnected();
        _discoDone=true;
        Serial.println("WiFi down");
    }
}
*/
/* ESP32
* WiFi Events
0  SYSTEM_EVENT_WIFI_READY               < ESP32 WiFi ready
1  SYSTEM_EVENT_SCAN_DONE                < ESP32 finish scanning AP
2  SYSTEM_EVENT_STA_START                < ESP32 station start
3  SYSTEM_EVENT_STA_STOP                 < ESP32 station stop
4  SYSTEM_EVENT_STA_CONNECTED            < ESP32 station connected to AP
5  SYSTEM_EVENT_STA_DISCONNECTED         < ESP32 station disconnected from AP
6  SYSTEM_EVENT_STA_AUTHMODE_CHANGE      < the auth mode of AP connected by ESP32 station changed
7  SYSTEM_EVENT_STA_GOT_IP               < ESP32 station got IP from connected AP
8  SYSTEM_EVENT_STA_LOST_IP              < ESP32 station lost IP and the IP is reset to 0
9  SYSTEM_EVENT_STA_WPS_ER_SUCCESS       < ESP32 station wps succeeds in enrollee mode
10 SYSTEM_EVENT_STA_WPS_ER_FAILED        < ESP32 station wps fails in enrollee mode
11 SYSTEM_EVENT_STA_WPS_ER_TIMEOUT       < ESP32 station wps timeout in enrollee mode
12 SYSTEM_EVENT_STA_WPS_ER_PIN           < ESP32 station wps pin code in enrollee mode
13 SYSTEM_EVENT_AP_START                 < ESP32 soft-AP start
14 SYSTEM_EVENT_AP_STOP                  < ESP32 soft-AP stop
15 SYSTEM_EVENT_AP_STACONNECTED          < a station connected to ESP32 soft-AP
16 SYSTEM_EVENT_AP_STADISCONNECTED       < a station disconnected from ESP32 soft-AP
17 SYSTEM_EVENT_AP_STAIPASSIGNED         < ESP32 soft-AP assign an IP to a connected station
18 SYSTEM_EVENT_AP_PROBEREQRECVED        < Receive probe request packet in soft-AP interface
19 SYSTEM_EVENT_GOT_IP6                  < ESP32 station or ap or ethernet interface v6IP addr is preferred
20 SYSTEM_EVENT_ETH_START                < ESP32 ethernet start
21 SYSTEM_EVENT_ETH_STOP                 < ESP32 ethernet stop
22 SYSTEM_EVENT_ETH_CONNECTED            < ESP32 ethernet phy link up
23 SYSTEM_EVENT_ETH_DISCONNECTED         < ESP32 ethernet phy link down
24 SYSTEM_EVENT_ETH_GOT_IP               < ESP32 ethernet got IP from connected AP
25 SYSTEM_EVENT_MAX

    WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_DISCONNECTED     = 6

*/
void H4P_WiFi::_wifiEvent(WiFiEvent_t event) {
//    Serial.printf("FH=%d _wifiEvent %d\n",ESP.getFreeHeap(),event);
    switch(event) {
/*2  SYSTEM_EVENT_STA_START       for ap_mode
        case SYSTEM_EVENT_WIFI_READY: // diag hoist
            Serial.printf("SYSTEM_EVENT_WIFI_READY %s\n",CSTR(WiFi.localIP().toString()));
			h4.queueFunction([](){ h4wifi.show();h4wifi.start(); });
            break;
*/
        case SYSTEM_EVENT_STA_STOP:
//            Serial.printf("SYSTEM_EVENT_STA_STOP mode? %s AP mode? %s [%d]\n",WiFi.getMode() & WIFI_STA ? "true":"false",WiFi.getMode() & WIFI_AP ? "true":"false",WiFi.getMode());
        case SYSTEM_EVENT_STA_LOST_IP:
//        case SYSTEM_EVENT_STA_DISCONNECTED:
//            WiFi.printDiag(Serial);
			if(!(WiFi.getMode() & WIFI_AP)) {
 //               Serial.printf("STA mode, going down\n");
                h4.queueFunction([](){ h4wifi._lostIP(); });
            } //else Serial.printf("STA stop ignored in AP_MODE\n");
            break;    
		case SYSTEM_EVENT_STA_GOT_IP:
			h4.queueFunction([](){ h4wifi._gotIP(); });
			break;
//		default:
//			Serial.printf("FH=%d _wifiEvent %d\n",ESP.getFreeHeap(),event);
//			break;
	}
}
#endif

#endif