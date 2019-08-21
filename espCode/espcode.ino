#define ESPCODE

#define SERIAL_BAUD 115200

//#define DEBUG

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "JPEGDecoder.h"

#define WIDTH 320
#define HEIGHT 240

// put your wireless information here.

const char wireless_name[] = "";
const char wireless_pass[] = "";

const long picture_change_delay = 30000L; //30 seconds between each piture
const long picture_load_timeout = 30000L; //30 seconds before trying to load again; ( errors, etc)

//#define IMG "http://i.imgur.com/Oo3sKMHm.jpg" //appended small m

int decodeStatus = 0;

int pictureIndex = 0;
String targetURL;
long timer = 0;

ESP8266WebServer server(80);

void setup()
{

    Serial.begin(SERIAL_BAUD);
    SPIFFS.begin();

    targetURL.reserve(100); //decent size.

    WiFi.begin(wireless_name, wireless_pass);

#ifdef DEBUG
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
#endif

    if (MDNS.begin("imgurPhotoFrame"))
    {
        Serial.println("% mdns started");
    }

    //manage index.html
    server.on("/", []() {
        fs::File file = SPIFFS.open("/index.html", "r");
        if (!file)
        {
            server.send(400, "text/plain", "index.html not found, but code is working; did you forget to upload data?");
            return;
        }
        server.streamFile(file, "text/html");
        file.close();
    });

    //this is the page that changes data bettwen the flash and the web-app
    server.on("/urllist", HTTP_POST, []() {
        if (server.arg("data") == NULL)
        {
            server.send(400, "text/plain", "No Data provided");
            return;
        }
        else
        {
            fs::File urlList = SPIFFS.open("/url", "w");
            urlList.print(server.arg("data"));
            urlList.close();
            pictureIndex = 0;
        }
        server.send(200, "text/plain", "A-OK");
    });

    server.on("/urllist", HTTP_GET, [](){
            fs::File urlList = SPIFFS.open("/url", "r");
            if (!urlList)
            {
                server.send(400, "text/plain", "url not found, but code is working; did you forget to upload data?");
                return;
            }

            server.streamFile(urlList, "text/plain");
            urlList.close();
            return;

    });

    server.begin();

    timer = 0;
}

void loop()
{
    //delay(1);
    server.handleClient();
    MDNS.update();

    if (millis() > timer)
    {
        prepareNewPicture();
        timer = millis() + picture_load_timeout;

        fs::File file = SPIFFS.open("/currentDisplay", "r");
        if (!file)
        {
#ifdef DEBUG
            Serial.print("unable to open the file to read information");
#endif

            return;
        }

        decodeStatus = JpegDec.decodeFsFile(file);
    }

    if (decodeStatus > 0)
    {

#ifdef DEBUG
        Serial.println("Sending Data for another image");
        Serial.println(targetURL);
#else
        sendRender();
#endif
        //reset the timer
        timer = millis() + picture_change_delay;
        decodeStatus = 0;
    }
}

void prepareNewPicture()
{
    decodeStatus = 0; // reset decode status, haven't decoded it yet;

    fs::File urlList = SPIFFS.open("/url", "r");

    //get the next picture in the array:
    for (int i = 0; i < pictureIndex + 1; i++)
    {
        //this will only run once for the first index, twice for second; etc.
        targetURL = "";
        targetURL += urlList.readStringUntil(',');
    }

    urlList.close();

    // check if targetURL is empty; if it is, re-open and read the first one
    if (!targetURL.startsWith("http"))
    {
        urlList = SPIFFS.open("/url", "r");
        targetURL = "";
        targetURL += urlList.readStringUntil(',');
        urlList.close();
        pictureIndex = 0; // reset picture index.
    }

    targetURL.replace("https", "http"); //remove https codes
#ifdef DEBUG
    Serial.println("Getting:");
    Serial.println(targetURL);
#endif
    HTTPClient http;

    if (http.begin(targetURL))
    { // HTTP
        int httpCode = http.GET();

#ifdef DEBUG
        Serial.println();
        Serial.printf("HTTP status code: %ld \n", httpCode);
#endif

        if (httpCode > 0)
        {
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
            {
                // Write Binary JPEG data to the filesystem
                fs::File displayFile = SPIFFS.open("/currentDisplay", "w");
                if (!displayFile)
                {
                    return;
                }

                size_t bytesSent = http.writeToStream(&displayFile);
                displayFile.close();

#ifdef DEBUG
                Serial.println();
                Serial.printf("sent %ld bytes to file\n", bytesSent);
#endif
            }
        }
        http.end();
    }

    pictureIndex++; //load up the next picture next time we call this;
}

void send16(uint16_t data)
{
    Serial.write((data >> 8) & 0xFF);
    Serial.write(data & 0xFF);
    Serial.flush();
}
void send32(uint32_t data)
{
    send16((data >> 16) & 0xFFFF);
    send16(data & 0xFFFF);
}

void sendRender()
{
    uint16_t *pImg;
    uint16_t mcu_w = JpegDec.MCUWidth;
    uint16_t mcu_h = JpegDec.MCUHeight;
    uint32_t max_x = JpegDec.width;
    uint32_t max_y = JpegDec.height;
    uint32_t min_w = _min(mcu_w, max_x % mcu_w);
    uint32_t min_h = _min(mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    while (JpegDec.read())
    {
        pImg = JpegDec.pImage;

        // calculate where the image block should be drawn on the screen
        uint16_t mcu_x = JpegDec.MCUx * mcu_w;
        uint16_t mcu_y = JpegDec.MCUy * mcu_h;

        // check if the image block size needs to be changed for the right and bottom edges
        win_w = (mcu_x + mcu_w <= max_x) ? mcu_w : min_w;
        win_h = (mcu_y + mcu_h <= max_y) ? mcu_h : min_h;

        // calculate how many pixels must be drawn
        uint16_t mcu_pixels = win_w * win_h;

        // draw image MCU block only if it will fit on the screen
        if ((WIDTH < mcu_x + win_w) || (HEIGHT < mcu_y + win_h))
        {
            //abort
            JpegDec.abort();
            break;
        }
        //otherwise, set bounding window
        uint32_t param_x = mcu_x + win_w - 1;
        uint32_t param_y = mcu_y + win_h - 1;

        Serial.print(F(">>>")); //start of data tag
        send16(mcu_pixels);     // number of pixels
        send16(mcu_x);          // position x
        send16(mcu_y);          // position y
        send32(param_x);
        send32(param_y);

        while (mcu_pixels--)
            send16(*pImg++); //for each pixel, send16.

        //You could experiment with sending patterns here.

        //yield(); //just to manage wifi and other connections.
    }

    // finished sending data from decoded file;
}
