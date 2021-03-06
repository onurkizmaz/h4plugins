/*
 MIT License

Copyright (c) 2019 Phil Bowles <H48266@gmail.com>
   github     https://github.com/philbowles/H4
   blog       https://8266iot.blogspot.com
   groups     https://www.facebook.com/groups/esp8266questions/
              https://www.facebook.com/H4-Esp8266-Firmware-Support-2338535503093896/


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

beta:
Erickcampos50@gmail.com


*/
#ifndef H4P_HO
#define H4P_HO

#define H4P_VERSION "0.0.1"

#include<H4.h>
#include<H4Utils.h>
#include<unordered_set>
#include<cstdarg>

using namespace std::placeholders;

#ifndef ARDUINO_SONOFF_BASIC // or s20 / sv ect
  #define RELAY_BUILTIN   12
  #define BUTTON_BUILTIN  0
#endif
//
using 	H4_FN_MSG 		=function<uint32_t(vector<string>)>;
using   H4BS_FN_SWITCH   =function<void(bool)>;

struct command{
	uint32_t            owner;
	uint32_t 		    levID;
	H4_FN_MSG 			fn;
};

using 	H4_CMD_MAP		    =std::unordered_multimap<string,command>;
using 	H4_CMD_MAP_I        =H4_CMD_MAP::iterator;
using   H4P_CONFIG_BLOCK    =std::unordered_map<string,string>;
/// todo: add these to literals kw.txt
enum H4_CMD_ERROR:uint32_t  {
    H4_CMD_OK,
    H4_CMD_UNKNOWN,
    H4_CMD_TOO_FEW_PARAMS,
    H4_CMD_TOO_MANY_PARAMS,    
    H4_CMD_NOT_NUMERIC,
    H4_CMD_OUT_OF_BOUNDS,
    H4_CMD_NAME_UNKNOWN,
    H4_CMD_PAYLOAD_FORMAT,
    H4_CMD_PROHIBITED
};

#define PAYLOAD vs.back()
#define PAYLOAD_INT atoi(CSTR(vs.back()))
#define STOI(n) atoi(CSTR(n))
#define PARAM_INT(n) STOI(vs[n])
#define CHOP_FRONT(vs) (vector<string>(++vs.begin(),vs.end()))
// synthesize CMD OK returner with null vec for simple void functions
#define CMD(x) ([this](vector<string>)->uint32_t{ x(); return H4_CMD_OK; })
#define CMDVS(x) ([this](vector<string> vs)->uint32_t{ return x(vs); })

#define VSCMD(x) uint32_t x(vector<string>)
//
//      PLUGINS
//
enum trustedIds {
  H4P_TRID_PATN = 50,
  H4P_TRID_PP1x,
  H4P_TRID_PWM1,
  H4P_TRID_GPIO,
  H4P_TRID_DBNC,
  H4P_TRID_RPTP,
  H4P_TRID_POLL,
  H4P_TRID_MULT,
  H4P_TRID_TRIG,
  H4P_TRID_SQWV,
  H4P_TRID_WIFI,
  H4P_TRID_HOTA,
  H4P_TRID_WFAP,
  H4P_TRID_MQMS,
  H4P_TRID_MQRC,
  H4P_TRID_MQTT,
  H4P_TRID_ASWS,
  H4P_TRID_SOAP,
  H4P_TRID_UDPM,
  H4P_TRID_NTFY,
  H4P_TRID_UBSW,
  H4P_TRID_3FNB,
  H4P_TRID_CERR,
  H4P_TRID_SCMD,
  H4P_TRID_QWRN,
  H4P_TRID_SNIF
};

enum H4PC_CMD_ID{
    H4PC_ROOT=1,
    H4PC_SHOW,
    H4PC_SNIF,
    H4PC_QWRN,
    H4PC_ESW_ROOT,
    H4PC_ESW_SET,
    H4PC_ESW_SWEEP,
    H4PC_WIFI,
    H4PC_MQTT,
    H4PC_ASWS,
    H4PC_SPIF,
    H4PC_UPNP 
};

class H4Plugin {
    protected:
        static   H4P_CONFIG_BLOCK _cb;

        static  H4_CMD_MAP      commands;

                H4_FN_VOID      _hook=nullptr;
                H4_INT_MAP      _names={};
                H4_CMD_MAP      _cmds={};

        template<size_t N>
        uint32_t guard(vector<string> vs,H4_FN_MSG f){
            if(vs.size()<N) return H4_CMD_TOO_FEW_PARAMS;
            return vs.size()>N ? H4_CMD_TOO_MANY_PARAMS:f(vs);
        }

        vector<uint32_t>    expectInt(string pl,const char* delim=",");  
    
            uint32_t        guardInt1(vector<string> vs,function<void(uint32_t)> f);

            uint32_t        guardInt4(vector<string> vs,function<void(uint32_t,uint32_t,uint32_t,uint32_t)> f);

            uint32_t        guardString1(vector<string> vs,function<void(string)> f);

            uint32_t        guardString2(vector<string> vs,function<void(string,string)> f);

    public:
                string          _pid="h4p"; // diag hoist
        static   vector<H4Plugin*>  _pending;

        virtual void        _startup();

        virtual void        _hookIn(){}

        virtual void        _greenLight(){}
/*    
        static void         dumpCommands(H4_CMD_MAP cm=commands){
            for(auto const c:cm){
                Serial.print(CSTR(c.first));Serial.print(" o=");Serial.print(c.second.owner);Serial.print(" l=");Serial.println(c.second.levID);
            }
        }      
*/   
        H4Plugin();

                void         reply(const char* fmt,...); // hoist protected

        virtual void         _reply(string msg) { Serial.print(CSTR(_pid));Serial.print(": ");Serial.print(CSTR(msg)); }
};

class H4PluginService: public H4Plugin {
    protected:
            uint32_t            subid;
            H4_CMD_MAP          _local;           
            vector<H4_FN_VOID>  _connChain;
            vector<H4_FN_VOID>  _discoChain;
            bool                _discoDone=false;


                void        h4pcConnected(){ for(auto const& c:_connChain) c(); }

                void        h4pcDisconnected(){ for(auto const& d:_discoChain) d(); }

    public:
        static vector<H4_FN_VOID>  _factoryChain;

                void        _startup() override;

        virtual void        _hookIn() override {}

        virtual void        _greenLight() override {}

        H4PluginService(H4_FN_VOID onConnect=[](){},H4_FN_VOID onDisconnect=[](){}){
                hookConnect(onConnect);
                hookDisconnect(onDisconnect);
        }
                string      getConfig(string c){ return _cb[c]; }
                void        hookConnect(H4_FN_VOID f){ _connChain.push_back(f); }
                void        hookDisconnect(H4_FN_VOID f){ _discoChain.push_back(f); } 
        static  void        hookFactory(H4_FN_VOID f){ _factoryChain.push_back(f); } 
        virtual void        restart(){ stop();start(); };
        virtual void        start() =0;      
        virtual void        stop()  =0;
};
#include<H4PConfig.h>
#endif // H4P_HO
