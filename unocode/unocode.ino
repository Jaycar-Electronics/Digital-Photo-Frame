

//This must be the same on both the ESP and uno.
#define SERIAL_BAUD 115200

#include "Arduino.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
#define GREY 0x8410
#define ORANGE 0xE880

MCUFRIEND_kbv tft;

uint16_t param_x;
uint16_t param_y;
uint16_t mcu_x;
uint16_t mcu_y;
uint16_t datalen;

inline uint16_t read16()
{
    byte upper = read();
    byte lower = read();
    return (upper << 8) | lower;
}

inline uint32_t read32()
{
    uint16_t upper = read16();
    uint16_t lower = read16();
    return (upper << 16) | lower;
}

inline byte read()
{
    // block and wait until serial data arrives
    while (!Serial.available())
        ;
    return Serial.read();
}

void receiveRender()
{
    // Check for 3 '>' chars in a row;
    // using the read() function in the if case.
    int count = 0;
    while (count < 3)
    {
        if (char(read()) == '>')
            count++;
        else
            count = 0;
    }

    // Start of image data; this must be the same size and format as what the esp is sending.
    datalen = read16();
    mcu_x = read16();
    mcu_y = read16();
    param_x = read32();
    param_y = read32();

    // Set to this portion of the screen; 
    tft.setAddrWindow(mcu_x, mcu_y, param_x, param_y);

    // prepare a block of data to receive
    uint16_t block = 0;
    bool first = true;

    while (datalen--)
    {
        block = read16();
        tft.pushColors(&block, 1, first);
        first = false;
        // It's a strange thing with this library
        // but the first block of information must be flagged first for this window. 
    }

    //finished receiving data; should we send an ack back?
}

void setup()
{
    uint16_t ID;
    Serial.begin(SERIAL_BAUD);
    tft.reset();
    ID = tft.readID();
    
    if (ID == 0xD3D3)
        ID = 0x9486; // write-only shield

    tft.begin(ID);
    tft.setRotation(3);
    tft.fillScreen(BLACK);


    //send a dollar sign as a way of saying ok 
    Serial.write('$'); 
}

void loop()
{
    receiveRender();
}
