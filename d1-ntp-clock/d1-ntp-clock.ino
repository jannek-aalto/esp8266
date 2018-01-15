//
//   NTP-Based Clock - https://steve.fi/Hardware/
//
//   This is a simple program which uses WiFi & an 4x7-segment display
//   to show the current time, complete with blinking ":".
//
//   Steve
//   --
//
//
//   jk 2018-01-15
//
//   NTP server configuration changed (now defined here)
//
//   European Union daylight saving time (DST) support added,
//   needed to add a bit of code to NTPClient for this too, a
//   function to get the month as a number: int NTPClient::getMon()
//   *TODO: move the DST code to NTPClient.cpp*
//
//   Blink code changed, slowly blinks the ":" 3 times before the 
//   minute changes, switching it off for seconds 55, 57 and 59.
//
//   OTA requests handled at end of loop (less latency, possibly)
//


//
// WiFi & over the air updates
//
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

//
// For dealing with NTP & the clock.
//
#include "NTPClient.h"
#define NTP_UPDATE_FREQ 3600 // seconds
#define NTP_SERVER "europe.pool.ntp.org"
//#define NTP_SERVER "192.168.1.1"

//
// The display-interface
//
#include "TM1637.h"


//
// WiFi setup.
//
#include "WiFiManager.h"


//
// Debug messages over the serial console.
//
#include "debug.h"


//
// The name of this project.
//
// Used for:
//   Access-Point name, in config-mode
//   OTA name.
//
#define PROJECT_NAME "NTP-clock"


//
// The timezone
//
// To honour European Union Daylight Saving Time (DST) rules:
// define TIME_ZONE 0, define TZ_OFFSET as hours and define EU_DST
//
#define TIME_ZONE 0
#define TZ_OFFSET 2
#define EU_DST


//
// NTP client, and UDP socket it uses.
//
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, TIME_ZONE * 3600, NTP_UPDATE_FREQ * 1000);


//
// Pin definitions for TM1637 and can be changed to other ports
//
#define CLK D3
#define DIO D2
TM1637 tm1637(CLK, DIO);


//
// Called just before the date/time is updated via NTP
//
void on_before_ntp()
{
    DEBUG_LOG("Updating date & time\n");
    tm1637.point(POINT_OFF);
}

//
// Called just after the date/time is updated via NTP
//
void on_after_ntp()
{
    DEBUG_LOG("Updated NTP client\n");
    tm1637.point(POINT_ON);
}

//
// This function is called when the device is powered-on.
//
void setup()
{
    // Enable our serial port.
    Serial.begin(115200);

    // initialize the display
    tm1637.init();

    // We want to see ":" between the digits.
    tm1637.point(POINT_ON);

    //
    // Set the intensity - valid choices include:
    //
    //   BRIGHT_DARKEST   = 0
    //   BRIGHT_TYPICAL   = 2
    //   BRIGHT_BRIGHTEST = 7
    //
    tm1637.set(BRIGHT_DARKEST);

    //
    // Handle WiFi setup
    //
    WiFiManager wifiManager;
    wifiManager.autoConnect(PROJECT_NAME);


    //
    // Ensure our NTP-client is ready.
    //
    timeClient.begin();

    //
    // Configure the callbacks.
    //
    timeClient.on_before_update(on_before_ntp);
    timeClient.on_after_update(on_after_ntp);


    //
    // The final step is to allow over the air updates
    //
    // This is documented here:
    //     https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/
    //
    // Hostname defaults to esp8266-[ChipID]
    //
    ArduinoOTA.setHostname(PROJECT_NAME);

    ArduinoOTA.onStart([]()
    {
        DEBUG_LOG("OTA Start\n");
    });
    ArduinoOTA.onEnd([]()
    {
        DEBUG_LOG("OTA End\n");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        char buf[32];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "Upgrade - %02u%%\n", (progress / (total / 100)));
        DEBUG_LOG(buf);
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        DEBUG_LOG("Error - ");

        if (error == OTA_AUTH_ERROR)
            DEBUG_LOG("Auth Failed\n");
        else if (error == OTA_BEGIN_ERROR)
            DEBUG_LOG("Begin Failed\n");
        else if (error == OTA_CONNECT_ERROR)
            DEBUG_LOG("Connect Failed\n");
        else if (error == OTA_RECEIVE_ERROR)
            DEBUG_LOG("Receive Failed\n");
        else if (error == OTA_END_ERROR)
            DEBUG_LOG("End Failed\n");
    });

    //
    // Ensure the OTA process is running & listening.
    //
    ArduinoOTA.begin();
}



//
// This function is called continously and is responsible
// for flashing the ":" and otherwise updating the display.
//
// We rely on the background NTP-updates to actually make sure
// that that works.
//
// Evaluates EU DST rules.
//
void loop()
{
    static char buf[10] = { '\0' };
    static char prev_buf[10] = { '\0' };
    static int  prev_sec = 0;
    
    //
    // Resync the clock?
    //
    timeClient.update();

    //
    // Get the current hour/min
    //
    int cur_wday = timeClient.getDay(); // 0-6,  0 == Sun
    int cur_mon  = timeClient.getMon(); // 1-12
    int cur_day  = timeClient.getDayOfMonth(); // 1-31
    
    int cur_hour = timeClient.getHours();
    int cur_min  = timeClient.getMinutes();
    int cur_sec  = timeClient.getSeconds();

    int disp_hour = cur_hour + TZ_OFFSET; // 0-24h overflow handled later

#ifdef EU_DST
    //
    // EU DST
    //
    if ((cur_mon < 3) || (cur_mon > 10)) {
        // Jan-Feb, Nov-Dec: Normal time
        // exit if
    } else if ((cur_mon > 3) && (cur_mon < 10)) {
        // Apr-Sep: DST
        disp_hour++;
    } else if (cur_mon == 3) {
        // March
        if ((cur_day > 24) && ((!cur_wday && cur_hour) || (cur_wday && (cur_day - cur_wday > 24)))) {
            // Last Sunday of March, 01:00 UTC and after: DST
            disp_hour++;
        }
    } else if (cur_day < 25) {
        // October, all before 25th: DST
        disp_hour++;
    } else if (!((!cur_wday && cur_hour) || (cur_wday && (cur_day - cur_wday > 24)))) {
        // October 25th and after, but before last Sunday 01:00 UTC: DST
        disp_hour++;
    } // else normal time.
#endif

    // Fix hour overflow
    if (disp_hour > 23) disp_hour -= 24;
    if (disp_hour <  0) disp_hour += 24;

    //
    // Format in a useful way.
    //
    sprintf(buf, "%02d%02d", disp_hour, cur_min);

    //
    // If the second has changed and the 
    // minute is about to change soon...
    //
    if (prev_sec != cur_sec) {
        if (cur_sec > 54 || !cur_sec) {
            tm1637.point(!(cur_sec % 2));     // Boolean option, conveniently
            memset(prev_buf, '\0', sizeof(prev_buf)); // Force display update
        }
        prev_sec = cur_sec;
    }
    
    //
    // If the current "hourmin" has changed...
    //
    if (strcmp(buf, prev_buf)) {
        // Update the display
        tm1637.display(0, buf[0] - '0');
        tm1637.display(1, buf[1] - '0');
        tm1637.display(2, buf[2] - '0');
        tm1637.display(3, buf[3] - '0');

        // And cache it
        strcpy(prev_buf, buf);
    }

    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();
}
