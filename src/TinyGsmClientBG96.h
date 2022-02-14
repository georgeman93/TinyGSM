/**
 * @file       TinyGsmClientBG96.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Apr 2018
 */

#include "../../testing.h"

#ifndef SRC_TINYGSMCLIENTBG96_H_
#define SRC_TINYGSMCLIENTBG96_H_

#include <functional>
#define MQTT_CALLBACK_SIGNATURE std::function<void(char*, char*, unsigned int)> callback

// #pragma message("TinyGSM:  TinyGsmClientBG96")

// #define TINY_GSM_DEBUG Serial

#define TINY_GSM_MUX_COUNT 12
#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE

#include "TinyGsmBattery.hpp"
#include "TinyGsmCalling.hpp"
#include "TinyGsmGPRS.hpp"
#include "TinyGsmGPS.hpp"
#include "TinyGsmModem.hpp"
#include "TinyGsmNTP.hpp"
#include "TinyGsmSMS.hpp"
#include "TinyGsmTCP.hpp"
#include "TinyGsmTemperature.hpp"
#include "TinyGsmTime.hpp"

#define GSM_NL "\r\n"
static const char GSM_OK[] TINY_GSM_PROGMEM = "OK" GSM_NL;
static const char GSM_ERROR[] TINY_GSM_PROGMEM = "ERROR" GSM_NL;
#if defined TINY_GSM_DEBUG
static const char GSM_CME_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CME ERROR:";
static const char GSM_CMS_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CMS ERROR:";
#endif

enum RegStatus {
    REG_NO_RESULT = -1,
    REG_UNREGISTERED = 0,
    REG_SEARCHING = 2,
    REG_DENIED = 3,
    REG_OK_HOME = 1,
    REG_OK_ROAMING = 5,
    REG_UNKNOWN = 4,
};

class TinyGsmBG96 : public TinyGsmModem<TinyGsmBG96>,
                    public TinyGsmGPRS<TinyGsmBG96>,
                    public TinyGsmTCP<TinyGsmBG96, TINY_GSM_MUX_COUNT>,
                    public TinyGsmCalling<TinyGsmBG96>,
                    public TinyGsmSMS<TinyGsmBG96>,
                    public TinyGsmTime<TinyGsmBG96>,
                    public TinyGsmNTP<TinyGsmBG96>,
                    public TinyGsmGPS<TinyGsmBG96>,
                    public TinyGsmBattery<TinyGsmBG96>,
                    public TinyGsmTemperature<TinyGsmBG96> {
    friend class TinyGsmModem<TinyGsmBG96>;
    friend class TinyGsmGPRS<TinyGsmBG96>;
    friend class TinyGsmTCP<TinyGsmBG96, TINY_GSM_MUX_COUNT>;
    friend class TinyGsmCalling<TinyGsmBG96>;
    friend class TinyGsmSMS<TinyGsmBG96>;
    friend class TinyGsmTime<TinyGsmBG96>;
    friend class TinyGsmNTP<TinyGsmBG96>;
    friend class TinyGsmGPS<TinyGsmBG96>;
    friend class TinyGsmBattery<TinyGsmBG96>;
    friend class TinyGsmTemperature<TinyGsmBG96>;

    /*
     * Inner Client
     */
   public:
    class GsmClientBG96 : public GsmClient {
        friend class TinyGsmBG96;

       public:
        bool uploadFile(const char* fileName, const char* content) {
            at->sendAT("+QFDEL=", "\"", fileName, "\"");
            at->waitResponse();
            at->sendAT("+QFUPL=", "\"", fileName, "\",", strlen(content), ",", 100);
            if (at->waitResponse("CONNECT") != 1)
                return false;
            at->stream.write(content, strlen(content));
            at->stream.flush();
            if (!at->waitResponse(2000))
                return false;

            return true;
        }

        bool enableMqtts() {
            const int ssl_context_id = 1;
            const int tcp_conn_id = 0;

            while (at->stream.available())
                at->stream.read();

            at->sendAT("+QMTCFG=", "\"", "SSL", "\",", tcp_conn_id, ",1,", ssl_context_id);
            if (!at->waitResponse())
                return false;
            at->sendAT("+QSSLCFG=", "\"sslversion", "\",", ssl_context_id, ",4");
            if (!at->waitResponse())
                return false;
            at->sendAT("+QSSLCFG=", "\"ciphersuite", "\",", ssl_context_id, ",0xffff");
            if (!at->waitResponse())
                return false;
            at->sendAT("+QSSLCFG=", "\"seclevel", "\",", ssl_context_id, ",2");
            if (!at->waitResponse())
                return false;
            at->sendAT("+QSSLCFG=", "\"", "ignorelocaltime", "\",", ssl_context_id);
            if (!at->waitResponse())
                return false;

            return true;
        }

        bool setCertsMqtts(const char* rootCA, const char* cert, const char* key) {
            const int ssl_context_id = 1;

            at->sendAT("+QSSLCFG=", "\"", "cacert", "\",", ssl_context_id, ",\"", rootCA, "\"");
            if (!at->waitResponse())
                return false;
            at->sendAT("+QSSLCFG=", "\"", "clientcert", "\",", ssl_context_id, ",\"", cert, "\"");
            if (!at->waitResponse())
                return false;
            at->sendAT("+QSSLCFG=", "\"", "clientkey", "\",", ssl_context_id, ",\"", key, "\"");
            if (!at->waitResponse())
                return false;

            return true;
        }

        bool connectMqtts(const char* client_id, const char* host, uint16_t port) {
            const int tcp_conn_id = 0;
            char reply[100];

            at->sendAT("+QMTOPEN=", tcp_conn_id, ",\"", host, "\",", port);
            at->waitResponse();
            sprintf(reply, "+QMTOPEN: %d,0" GSM_NL, tcp_conn_id);
            if (at->waitResponse(30000, reply) != 1)
                return false;
            at->sendAT("+QMTCONN=", tcp_conn_id, ",\"", client_id, "\"");

            sprintf(reply, "+QMTCONN: %d,0,0" GSM_NL, tcp_conn_id);
            if (at->waitResponse(50000, reply) != 1)
                return false;

            return true;
        }

        bool disconnectMqtts() {
            int tcp_conn_id = 0;
            char reply[100];
            at->streamClear();
            at->sendAT("+QMTDISC=", tcp_conn_id);
            sprintf(reply, "+QMTDISC: %d,0" GSM_NL, tcp_conn_id);
            if (at->waitResponse(50000, reply) != 1)
                return false;
            return true;
        }

        bool subscribeMqtts(const char* topic) {
            const int tcp_conn_id = 0;
            const int msg_id = 1;
            const int qos = 1;
            char reply[100];

            at->sendAT("+QMTSUB=", tcp_conn_id, ",", msg_id, ",\"", topic, "\",", qos);

            sprintf(reply, "+QMTSUB: %d,%d,0,%d" GSM_NL, tcp_conn_id, msg_id, qos);
            if (at->waitResponse(10000, reply) != 1)
                return false;

            return true;
        }

        bool publishMqtts(const char* topic, const char* payload, bool expectreply = false) {
            const int tcp_conn_id = 0;
            const int msg_id = 1;
            const int qos = 1;
            char reply[100];
            String data;

            at->sendAT("+QMTPUB=", tcp_conn_id, ",", msg_id, ",", qos, ",0,\"", topic, "\",", strlen(payload));
            if (at->waitResponse(">") != 1)
                return false;
            at->stream.write(payload, strlen(payload));
            at->stream.flush();
            sprintf(reply, "+QMTPUB: %d,%d,", tcp_conn_id, msg_id);
            if (at->waitResponse(5000, reply) != 1)
                return false;
            at->streamSkipUntil('\n');  // in case there is an extra value for the number of retransmissions

            at->waitResponse(1000, data, "}\"");  // in case the reply comes quickly
            if (expectreply) {
                if (!data.length()) {
                    at->sendAT("+QMTPUB=", tcp_conn_id, ",", msg_id + 1, ",", qos, ",0,\"", "dummy", "\",", 1);
                    if (at->waitResponse(">") != 1)
                        return 0;
                    char dummy[1] = {'0'};
                    at->stream.write(dummy, 1);
                    at->stream.flush();
                    at->waitResponse(20000, data, "}\"");
                }

                if (data.length()) {
                    if (data.indexOf("+QMTRECV") != -1) {
                        int8_t topic_start = 0;
                        int8_t topic_end = 0;

                        // extract subscribed topic
                        topic_start = data.indexOf("\"");
                        topic_end = data.indexOf(",", topic_start);

                        String topic = data.substring(topic_start + 1, topic_end - 1);
                        int8_t payload_start = data.indexOf("\"", topic_end + 1);
                        String payload = data.substring(payload_start + 1, data.length() - 1);

                        this->callback((char*)topic.c_str(), (char*)payload.c_str(), payload.length());
                    }
                }
            }
            return 1;
        }

        void setCallbackMqtts(MQTT_CALLBACK_SIGNATURE) {
            this->callback = callback;
        }

        bool enableHttps() {
            const int ssl_context_id = 2;

            at->sendAT("+QSSLCFG=", "\"sslversion", "\",", ssl_context_id, ",4");
            if (!at->waitResponse())
                return false;

            at->sendAT("+QSSLCFG=", "\"ciphersuite", "\",", ssl_context_id, ",0xffff");
            if (!at->waitResponse())
                return false;

            at->sendAT("+QSSLCFG=", "\"seclevel", "\",", ssl_context_id, ",1");
            if (!at->waitResponse())
                return false;

            return true;
        }

        bool setCertsHttps(const char* fileName) {
            const int ssl_context_id = 2;

            at->sendAT("+QSSLCFG=", "\"", "cacert", "\",", ssl_context_id, ",\"", fileName, "\"");
            at->waitResponse();
            at->sendAT("+QSSLCFG=", "\"", "negotiatetime", "\",", ssl_context_id, ",300");
            at->waitResponse();
            at->sendAT("+QSSLCFG=", "\"", "ignorelocaltime", "\",", ssl_context_id, ",0");
            return at->waitResponse();
        }

        bool connectHttps(const char* host) {
            const int pdp_context_id = 1;
            const int ssl_context_id = 2;
            const int client_id = 0;
            char reply[100];

            at->sendAT("+QSSLOPEN=", pdp_context_id, ",", ssl_context_id, ",", client_id, ",\"", host, "\",", "443,1");

            sprintf(reply, "+QSSLOPEN: %d,0" GSM_NL, client_id);
            if (at->waitResponse(40000, reply) != 1) {
                return false;
            }
            return true;
        }

        bool sendHttps(const char* payload) {
            const int client_id = 0;
            const int max_send_size = 1024;
            const int initial_size = strlen(payload);
            int bytes_sent = 0;
            int bytes_sending;

            while (bytes_sent < initial_size) {
                delay(1000);
                if (strlen(payload) > max_send_size)
                    bytes_sending = max_send_size;
                else
                    bytes_sending = strlen(payload);

                at->sendAT("+QSSLSEND=", client_id, ",", bytes_sending);
                if (at->waitResponse(5000, ">") != 1)
                    return false;
                at->stream.write(payload, bytes_sending);
                at->stream.flush();

                if (at->waitResponse(5000, "SEND OK" GSM_NL) != 1)
                    return false;

                payload += bytes_sending;
                bytes_sent += bytes_sending;
            }

            if (!at->waitResponse(25000, "\"}" GSM_NL))
                return false;

            return true;
        }

       private:
        MQTT_CALLBACK_SIGNATURE;

       public:
        GsmClientBG96() {}

        explicit GsmClientBG96(TinyGsmBG96& modem, uint8_t mux = 0) {
            init(&modem, mux);
        }

        bool init(TinyGsmBG96* modem, uint8_t mux = 0) {
            this->at = modem;
            sock_available = 0;
            prev_check = 0;
            sock_connected = false;
            got_data = false;

            if (mux < TINY_GSM_MUX_COUNT) {
                this->mux = mux;
            } else {
                this->mux = (mux % TINY_GSM_MUX_COUNT);
            }
            at->sockets[this->mux] = this;

            return true;
        }

       public:
        virtual int connect(const char* host, uint16_t port, int timeout_s) {
            stop();
            TINY_GSM_YIELD();
            rx.clear();
            sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
            return sock_connected;
        }
        TINY_GSM_CLIENT_CONNECT_OVERRIDES

        void stop(uint32_t maxWaitMs) {
            uint32_t startMillis = millis();
            dumpModemBuffer(maxWaitMs);
            at->sendAT(GF("+QICLOSE="), mux);
            sock_connected = false;
            at->waitResponse((maxWaitMs - (millis() - startMillis)));
        }
        void stop() override {
            stop(15000L);
        }

        /*
         * Extended API
         */

        String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
    };

    /*
     * Inner Secure Client
     */

    /*
    class GsmClientSecureBG96 : public GsmClientBG96
    {
    public:
      GsmClientSecure() {}

      GsmClientSecure(TinyGsmBG96& modem, uint8_t mux = 0)
       : public GsmClient(modem, mux)
      {}


    public:
      int connect(const char* host, uint16_t port, int timeout_s) override {
        stop();
        TINY_GSM_YIELD();
        rx.clear();
        sock_connected = at->modemConnect(host, port, mux, true, timeout_s);
        return sock_connected;
      }
      TINY_GSM_CLIENT_CONNECT_OVERRIDES
    };
    */

    /*
     * Constructor
     */
   public:
    explicit TinyGsmBG96(Stream& stream) : stream(stream) {
        memset(sockets, 0, sizeof(sockets));
    }

    /*
     * Basic functions
     */
   protected:
    bool initImpl(const char* pin = NULL) {
        DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);
        DBG(GF("### TinyGSM Compiled Module:  TinyGsmClientBG96"));

        if (!testAT()) {
            return false;
        }

        sendAT(GF("E0"));  // Echo Off
        if (waitResponse() != 1) {
            return false;
        }

#ifdef TINY_GSM_DEBUG
        sendAT(GF("+CMEE=2"));  // turn on verbose error codes
#else
        sendAT(GF("+CMEE=0"));  // turn off error codes
#endif
        waitResponse();

        DBG(GF("### Modem:"), getModemName());

        // Disable time and time zone URC's
        sendAT(GF("+CTZR=0"));
        if (waitResponse(10000L) != 1) {
            return false;
        }

        // Enable automatic time zone update
        sendAT(GF("+CTZU=1"));
        if (waitResponse(10000L) != 1) {
            return false;
        }

        SimStatus ret = getSimStatus();
        // if the sim isn't ready and a pin has been provided, try to unlock the sim
        if (ret != SIM_READY && pin != NULL && strlen(pin) > 0) {
            simUnlock(pin);
            return (getSimStatus() == SIM_READY);
        } else {
            // if the sim is ready, or it's locked but no pin has been provided,
            // return true
            return (ret == SIM_READY || ret == SIM_LOCKED);
        }
    }

    /*
     * Power functions
     */
   protected:
    bool restartImpl(const char* pin = NULL) {
        if (!testAT()) {
            return false;
        }
        if (!setPhoneFunctionality(1, true)) {
            return false;
        }
        waitResponse(10000L, GF("APP RDY"));
        return init(pin);
    }

    bool powerOffImpl() {
        sendAT(GF("+QPOWD=1"));
        waitResponse(300);  // returns OK first
        return waitResponse(300, GF("POWERED DOWN")) == 1;
    }

    // When entering into sleep mode is enabled, DTR is pulled up, and WAKEUP_IN
    // is pulled up, the module can directly enter into sleep mode.If entering
    // into sleep mode is enabled, DTR is pulled down, and WAKEUP_IN is pulled
    // down, there is a need to pull the DTR pin and the WAKEUP_IN pin up first,
    // and then the module can enter into sleep mode.
    bool sleepEnableImpl(bool enable = true) {
        sendAT(GF("+QSCLK="), enable);
        return waitResponse() == 1;
    }

    bool setPhoneFunctionalityImpl(uint8_t fun, bool reset = false) {
        sendAT(GF("+CFUN="), fun, reset ? ",1" : "");
        return waitResponse(10000L, GF("OK")) == 1;
    }

    /*
     * Generic network functions
     */
   public:
    RegStatus getRegistrationStatus() {
        // Check first for EPS registration
        RegStatus epsStatus = (RegStatus)getRegistrationStatusXREG("CEREG");

        // If we're connected on EPS, great!
        if (epsStatus == REG_OK_HOME || epsStatus == REG_OK_ROAMING) {
            return epsStatus;
        } else {
            // Otherwise, check generic network status
            return (RegStatus)getRegistrationStatusXREG("CREG");
        }
    }

   protected:
    bool isNetworkConnectedImpl() {
        RegStatus s = getRegistrationStatus();
        return (s == REG_OK_HOME || s == REG_OK_ROAMING);
    }

    /*
     * GPRS functions
     */
   protected:
    bool gprsConnectImpl(const char* apn, const char* user = NULL,
                         const char* pwd = NULL) {
        gprsDisconnect();

        // Configure the TCPIP Context
        sendAT(GF("+QICSGP=1,1,\""), apn, GF("\",\""), user, GF("\",\""), pwd,
               GF("\""), GF(",1"));
        if (waitResponse() != 1) {
            return false;
        }

        // Activate GPRS/CSD Context
        sendAT(GF("+QIACT=1"));
        if (waitResponse(150000L) != 1) {
            return false;
        }

        // Attach to Packet Domain service - is this necessary?
        sendAT(GF("+CGATT=1"));
        if (waitResponse(60000L) != 1) {
            return false;
        }

        return true;
    }

    bool gprsDisconnectImpl() {
        sendAT(GF("+QIDEACT=1"));  // Deactivate the bearer context
        if (waitResponse(40000L) != 1) {
            return false;
        }

        return true;
    }

    /*
     * SIM card functions
     */
   protected:
    String getSimCCIDImpl() {
        sendAT(GF("+QCCID"));
        if (waitResponse(GF(GSM_NL "+QCCID:")) != 1) {
            return "";
        }
        String res = stream.readStringUntil('\n');
        waitResponse();
        res.trim();
        return res;
    }

    /*
     * Phone Call functions
     */
   protected:
    // Can follow all of the phone call functions from the template

    /*
     * Messaging functions
     */
   protected:
    // Follows all messaging functions per template

    /*
     * GSM Location functions
     */
   protected:
    // NOTE:  As of application firmware version 01.016.01.016 triangulated
    // locations can be obtained via the QuecLocator service and accompanying AT
    // commands.  As this is a separate paid service which I do not have access
    // to, I am not implementing it here.

    /*
     * GPS/GNSS/GLONASS location functions
     */
   protected:
    // enable GPS
    bool enableGPSImpl() {
#ifdef TESTING
        if (executeTest(8)) {
            if (t[8]) return false;
        }
#endif
        sendAT(GF("+QGPS=1"));
        if (waitResponse() != 1) {
            return false;
        }
        return true;
    }

    bool disableGPSImpl() {
        sendAT(GF("+QGPSEND"));
        if (waitResponse() != 1) {
            return false;
        }
        return true;
    }

    // get the RAW GPS output
    String getGPSrawImpl() {
        sendAT(GF("+QGPSLOC=2"));
        if (waitResponse(10000L, GF("+QGPSLOC:")) != 1) {
            return "";
        }
        String res = stream.readStringUntil('\n');
        waitResponse();
        res.trim();
        return res;
    }

    // get GPS informations
    bool getGPSImpl(float* lat, float* lon, float* speed = 0, float* alt = 0,
                    int* vsat = 0, int* usat = 0, float* accuracy = 0,
                    int* year = 0, int* month = 0, int* day = 0, int* hour = 0,
                    int* minute = 0, int* second = 0, bool* fix = 0, float* heading = 0) {
#ifdef TESTING
        if (executeTest(10)) return t[10];
#endif
        sendAT(GF("+QGPSGNMEA=\"GGA\""));
        if (waitResponse(10000L, GF("+QGPSGNMEA: $GPGGA,")) != 1) {
            waitResponse();
            return false;
        }

        // init variables
        float ilat = 0;
        float ilon = 0;
        float ispeed = 0;
        float ialt = 0;
        int iusat = 0;
        float iaccuracy = 0;
        int iyear = -1;
        int imonth = -1;
        int iday = -1;
        int ihour = -1;
        int imin = -1;
        float secondWithSS = -1;
        int ifixquality = 0;
        float iheading = 0;

        //$GPGGA,115739.00,4158.8441367,N,09147.4416929,W,4,13,0.9,255.747,M,-32.00,M,01,0000*6E

        // we can only break up the time if it appears in the first place
        String t = stream.readStringUntil(',');  // eg 103647.0
        if (t != "") {
            ihour = atoi(t.substring(0, 2).c_str());
            imin = atoi(t.substring(2, 4).c_str());
            secondWithSS = atof(t.substring(4, 9).c_str());
        }

        ilat = streamGetFloatBefore(',');  // Latitude
        int temp = (int)(ilat / 100.0);    // convert from degrees and decimal minutes to decimal degrees
        ilat = temp + (ilat - (float)temp * 100) / 60;
        String dir = stream.readStringUntil(',');  // N or S
        if (dir == "S")
            ilat = ilat * (-1);
        ilon = streamGetFloatBefore(',');  // Longitude
        temp = (int)(ilon / 100.0);
        ilon = temp + (ilon - (float)temp * 100) / 60;
        dir = stream.readStringUntil(',');  // E or W
        if (dir == "W")
            ilon = ilon * (-1);
        ifixquality = streamGetIntBefore(',');  // Fix Quality
        iusat = streamGetIntBefore(',');        // Number of satellites,
        iaccuracy = streamGetFloatBefore(',');  // Horizontal precision
        ialt = streamGetFloatBefore(',');       // Altitude from sea level
        streamSkipUntil('\n');                  // skip height of geoid above ellipse, time since last dgps update, dgps reference station id and checksum

        sendAT("+QGPSGNMEA=\"RMC\"");
        if (waitResponse(10000L, "+QGPSGNMEA: $GPRMC,") != 1) {
            waitResponse();
            return false;
        }
        //$GPRMC,220516,A,5133.82,N,00042.24,W,173.8,231.8,130694,004.2,W*70
        streamSkipUntil(',');                  // skip time
        streamSkipUntil(',');                  // skip receiver warning
        streamSkipUntil(',');                  // skip lat
        streamSkipUntil(',');                  // skip lat dir
        streamSkipUntil(',');                  // skip lon
        streamSkipUntil(',');                  // skip lon dir
        ispeed = streamGetFloatBefore(',');    // Speed Over Ground in knots
        iheading = streamGetFloatBefore(',');  // Course made good, true

        // we can only break up the date if it appears in the first place
        String d = stream.readStringUntil(',');  // eg 130922
        if (d != "") {
            iday = atoi(d.substring(0, 2).c_str());
            imonth = atoi(d.substring(2, 4).c_str());
            iyear = atof(d.substring(4, 6).c_str()) + 2000;  // 22 + 2000 = 2022
        }
        streamSkipUntil('\n');  // skip magnetic variation, associated dir and checksum

        // Set pointers
        if (lat != NULL) *lat = ilat;
        if (lon != NULL) *lon = ilon;
        if (speed != NULL) *speed = ispeed;
        if (alt != NULL) *alt = ialt;
        if (vsat != NULL) *vsat = 0;
        if (usat != NULL) *usat = iusat;
        if (accuracy != NULL) *accuracy = iaccuracy;
        if (year != NULL) *year = iyear;
        if (month != NULL) *month = imonth;
        if (day != NULL) *day = iday;
        if (hour != NULL) *hour = ihour;
        if (minute != NULL) *minute = imin;
        if (second != NULL) *second = static_cast<int>(secondWithSS);
        if (heading != NULL) *heading = iheading;
        if (fix != NULL && (ifixquality == 1 || ifixquality == 2))
            *fix = true;
        else
            *fix = false;

        waitResponse();  // Final OK
        return true;
    }

    /*
     * Time functions
     */
   protected:
    String getGSMDateTimeImpl(TinyGSMDateTimeFormat format) {
        sendAT(GF("+QLTS=2"));
        if (waitResponse(2000L, GF("+QLTS: \"")) != 1) {
            return "";
        }

        String res;

        switch (format) {
            case DATE_FULL:
                res = stream.readStringUntil('"');
                break;
            case DATE_TIME:
                streamSkipUntil(',');
                res = stream.readStringUntil('"');
                break;
            case DATE_DATE:
                res = stream.readStringUntil(',');
                break;
        }
        waitResponse();  // Ends with OK
        return res;
    }

    // The BG96 returns UTC time instead of local time as other modules do in
    // response to CCLK, so we're using QLTS where we can specifically request
    // local time.
    bool getNetworkTimeImpl(int* year, int* month, int* day, int* hour,
                            int* minute, int* second, float* timezone) {
        sendAT(GF("+QLTS=2"));
        if (waitResponse(2000L, GF("+QLTS: \"")) != 1) {
            return false;
        }

        int iyear = 0;
        int imonth = 0;
        int iday = 0;
        int ihour = 0;
        int imin = 0;
        int isec = 0;
        int itimezone = 0;

        // Date & Time
        iyear = streamGetIntBefore('/');
        imonth = streamGetIntBefore('/');
        iday = streamGetIntBefore(',');
        ihour = streamGetIntBefore(':');
        imin = streamGetIntBefore(':');
        isec = streamGetIntLength(2);
        char tzSign = stream.read();
        itimezone = streamGetIntBefore(',');
        if (tzSign == '-') {
            itimezone = itimezone * -1;
        }
        streamSkipUntil('\n');  // DST flag

        // Set pointers
        if (iyear < 2000) iyear += 2000;
        if (year != NULL) *year = iyear;
        if (month != NULL) *month = imonth;
        if (day != NULL) *day = iday;
        if (hour != NULL) *hour = ihour;
        if (minute != NULL) *minute = imin;
        if (second != NULL) *second = isec;
        if (timezone != NULL) *timezone = static_cast<float>(itimezone) / 4.0;

        // Final OK
        waitResponse();  // Ends with OK
        return true;
    }

    /*
     * NTP server functions
     */

    byte NTPServerSyncImpl(String server = "pool.ntp.org", byte = -5) {
        // Request network synchronization
        // AT+QNTP=<contextID>,<server>[,<port>][,<autosettime>]
        sendAT(GF("+QNTP=1,\""), server, '"');
        if (waitResponse(10000L, GF("+QNTP:"))) {
            String result = stream.readStringUntil(',');
            streamSkipUntil('\n');
            result.trim();
            if (TinyGsmIsValidNumber(result)) {
                return result.toInt();
            }
        } else {
            return -1;
        }
        return -1;
    }

    String ShowNTPErrorImpl(byte error) TINY_GSM_ATTR_NOT_IMPLEMENTED;

    /*
     * Battery functions
     */
   protected:
    // Can follow CBC as in the template

    /*
     * Temperature functions
     */
   protected:
    // get temperature in degree celsius
    uint16_t getTemperatureImpl() {
        sendAT(GF("+QTEMP"));
        if (waitResponse(GF(GSM_NL "+QTEMP:")) != 1) {
            return 0;
        }
        // return temperature in C
        uint16_t res =
            streamGetIntBefore(',');  // read PMIC (primary ic) temperature
        streamSkipUntil(',');         // skip XO temperature ??
        streamSkipUntil('\n');        // skip PA temperature ??
        // Wait for final OK
        waitResponse();
        return res;
    }

    /*
     * Client related functions
     */
   protected:
    bool modemConnect(const char* host, uint16_t port, uint8_t mux,
                      bool ssl = false, int timeout_s = 150) {
        if (ssl) {
            DBG("SSL not yet supported on this module!");
        }

        uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;

        // <PDPcontextID>(1-16), <connectID>(0-11),
        // "TCP/UDP/TCP LISTENER/UDPSERVICE", "<IP_address>/<domain_name>",
        // <remote_port>,<local_port>,<access_mode>(0-2; 0=buffer)
        sendAT(GF("+QIOPEN=1,"), mux, GF(",\""), GF("TCP"), GF("\",\""), host,
               GF("\","), port, GF(",0,0"));
        waitResponse();

        if (waitResponse(timeout_ms, GF(GSM_NL "+QIOPEN:")) != 1) {
            return false;
        }

        if (streamGetIntBefore(',') != mux) {
            return false;
        }
        // Read status
        return (0 == streamGetIntBefore('\n'));
    }

    int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
        sendAT(GF("+QISEND="), mux, ',', (uint16_t)len);
        if (waitResponse(GF(">")) != 1) {
            return 0;
        }
        stream.write(reinterpret_cast<const uint8_t*>(buff), len);
        stream.flush();
        if (waitResponse(GF(GSM_NL "SEND OK")) != 1) {
            return 0;
        }
        // TODO(?): Wait for ACK? AT+QISEND=id,0
        return len;
    }

    size_t modemRead(size_t size, uint8_t mux) {
        if (!sockets[mux]) return 0;
        sendAT(GF("+QIRD="), mux, ',', (uint16_t)size);
        if (waitResponse(GF("+QIRD:")) != 1) {
            return 0;
        }
        int16_t len = streamGetIntBefore('\n');

        for (int i = 0; i < len; i++) {
            moveCharFromStreamToFifo(mux);
        }
        waitResponse();
        // DBG("### READ:", len, "from", mux);
        sockets[mux]->sock_available = modemGetAvailable(mux);
        return len;
    }

    size_t modemGetAvailable(uint8_t mux) {
        if (!sockets[mux]) return 0;
        sendAT(GF("+QIRD="), mux, GF(",0"));
        size_t result = 0;
        if (waitResponse(GF("+QIRD:")) == 1) {
            streamSkipUntil(',');  // Skip total received
            streamSkipUntil(',');  // Skip have read
            result = streamGetIntBefore('\n');
            if (result) {
                DBG("### DATA AVAILABLE:", result, "on", mux);
            }
            waitResponse();
        }
        if (!result) {
            sockets[mux]->sock_connected = modemGetConnected(mux);
        }
        return result;
    }

    bool modemGetConnected(uint8_t mux) {
        sendAT(GF("+QISTATE=1,"), mux);
        // +QISTATE: 0,"TCP","151.139.237.11",80,5087,4,1,0,0,"uart1"

        if (waitResponse(GF("+QISTATE:")) != 1) {
            return false;
        }

        streamSkipUntil(',');                  // Skip mux
        streamSkipUntil(',');                  // Skip socket type
        streamSkipUntil(',');                  // Skip remote ip
        streamSkipUntil(',');                  // Skip remote port
        streamSkipUntil(',');                  // Skip local port
        int8_t res = streamGetIntBefore(',');  // socket state

        waitResponse();

        // 0 Initial, 1 Opening, 2 Connected, 3 Listening, 4 Closing
        return 2 == res;
    }

    /*
     * Utilities
     */
   public:
    // TODO(vshymanskyy): Optimize this!
    int8_t waitResponse(uint32_t timeout_ms, String& data,
                        GsmConstStr r1 = GFP(GSM_OK),
                        GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                        GsmConstStr r3 = GFP(GSM_CME_ERROR),
                        GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                        GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                        GsmConstStr r5 = NULL) {
        /*String r1s(r1); r1s.trim();
        String r2s(r2); r2s.trim();
        String r3s(r3); r3s.trim();
        String r4s(r4); r4s.trim();
        String r5s(r5); r5s.trim();
        DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s);*/
        data.reserve(64);
        uint8_t index = 0;
        uint32_t startMillis = millis();
        do {
            TINY_GSM_YIELD();
            while (stream.available() > 0) {
                TINY_GSM_YIELD();
                int8_t a = stream.read();
                if (a <= 0) continue;  // Skip 0x00 bytes, just in case
                data += static_cast<char>(a);
                if (r1 && data.endsWith(r1)) {
                    index = 1;
                    goto finish;
                } else if (r2 && data.endsWith(r2)) {
                    index = 2;
                    goto finish;
                } else if (r3 && data.endsWith(r3)) {
#if defined TINY_GSM_DEBUG
                    if (r3 == GFP(GSM_CME_ERROR)) {
                        streamSkipUntil('\n');  // Read out the error
                    }
#endif
                    index = 3;
                    goto finish;
                } else if (r4 && data.endsWith(r4)) {
                    index = 4;
                    goto finish;
                } else if (r5 && data.endsWith(r5)) {
                    index = 5;
                    goto finish;
                } else if (data.endsWith(GF(GSM_NL "+QIURC:"))) {
                    streamSkipUntil('\"');
                    String urc = stream.readStringUntil('\"');
                    streamSkipUntil(',');
                    if (urc == "recv") {
                        int8_t mux = streamGetIntBefore('\n');
                        DBG("### URC RECV:", mux);
                        if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
                            sockets[mux]->got_data = true;
                        }
                    } else if (urc == "closed") {
                        int8_t mux = streamGetIntBefore('\n');
                        DBG("### URC CLOSE:", mux);
                        if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
                            sockets[mux]->sock_connected = false;
                        }
                    } else {
                        streamSkipUntil('\n');
                    }
                    data = "";
                }
            }
        } while (millis() - startMillis < timeout_ms);
    finish:
        if (!index) {
            data.trim();
            if (data.length()) {
                DBG("### Unhandled:", data);
            }
            data = "";
        }
        // data.replace(GSM_NL, "/");
        // DBG('<', index, '>', data);
        return index;
    }

    int8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GSM_OK),
                        GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                        GsmConstStr r3 = GFP(GSM_CME_ERROR),
                        GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                        GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                        GsmConstStr r5 = NULL) {
        String data;
        return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
    }

    int8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK),
                        GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                        GsmConstStr r3 = GFP(GSM_CME_ERROR),
                        GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                        GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                        GsmConstStr r5 = NULL) {
        return waitResponse(1000, r1, r2, r3, r4, r5);
    }

   public:
    Stream& stream;

   protected:
    GsmClientBG96* sockets[TINY_GSM_MUX_COUNT];
    const char* gsmNL = GSM_NL;
};

#endif  // SRC_TINYGSMCLIENTBG96_H_
