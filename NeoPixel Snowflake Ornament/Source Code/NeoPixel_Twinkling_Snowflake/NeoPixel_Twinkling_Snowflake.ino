#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "Adafruit_MCP23008.h"

#define PIN A3  //NeoPixel data line
#define RANDOMSEED A1 //Analog input which is used for generating random seed value
#define NumPixels 40  //Number of pixels on snowflake PCB
#define BRIGHTNESS 25 //Global brightness value
#define PIN_SWITCH 3

//NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//NEO_GRB     NumNumPixels are wired for GRB bitstream (most NeoPixel products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NumPixels, PIN, NEO_GRB + NEO_KHZ800);

float redStates[NumPixels];
float blueStates[NumPixels];
float greenStates[NumPixels];
//Controls how slowly the pixels "die" after they are flased by the twinkling animation
float fadeRate = 0.95;

bool last_switch = true;

Adafruit_MCP23008 mcp;

void setup()
{
  mcp.begin();      // use default address 0
  //Being NeoPixel communication at specified data rate of 800kHz
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  mcp.pinMode(PIN_SWITCH, INPUT);
  mcp.pullUp(PIN_SWITCH, HIGH);  // turn on a 100K pullup internally
  /*Reads transient noise from analog input A1 and uses it to generate the random seed value so that every time the device is power cylced,
    new random values are used for the animation*/
  randomSeed(analogRead(RANDOMSEED));
  //Initialize all pixels states to no color
  for (uint16_t i = 0; i < NumPixels; i++)
  {
    redStates[i] = 0;
    greenStates[i] = 0;
    blueStates[i] = 0;
  }
  strip.show(); //Initialize all NumPixels to 'off'
}

void loop ()
{
  /*Check mechanical keyswitch
    bool curr_switch = mcp.digitalRead(PIN_SWITCH);
    //If switch has changed states since the last loop iteration, figure out if switch is pressed or released (digital HIGH or LOW)
    if (curr_switch != last_switch)
    {
    if (curr_switch)
    {
      TemperatureBasedTwinkling();
    }
    else
    {

    }
    last_switch = curr_switch;
    }
    delay(10);*/
  TemperatureBasedTwinkling();
}

void TemperatureBasedTwinkling()
{
  /*Sets twinkling frequency by using the random() function to generate an random integer up to maximum specified,
    the lower the number, the more frequent the pixels will twinkle*/
  if (random(5) == 1)
  {
    //Use random function again to choose a single pixel out of the total to set to a random white color
    uint16_t i = random(NumPixels);
    if (redStates[i] < 1 && greenStates[i] < 1 && blueStates[i] < 1)
    {
      redStates[i] = random(100, 255);
      greenStates[i] = 255;
      blueStates[i] = 255;
    }
  }
  for (uint16_t n = 0; n < NumPixels; n++)
  {
    if (redStates[n] > 1 || greenStates[n] > 1 || blueStates[n] > 1)
    {
      strip.setPixelColor(n, redStates[n], greenStates[n], blueStates[n]);
      if (redStates[n] > 1)
      {
        redStates[n] = redStates[n] * fadeRate;
      }
      else
      {
        redStates[n] = 0;
      }
      if (greenStates[n] > 1)
      {
        greenStates[n] = greenStates[n] * fadeRate;
      }
      else
      {
        greenStates[n] = 0;
      }
      if (blueStates[n] > 1)
      {
        blueStates[n] = blueStates[n] * fadeRate;
      }
      else
      {
        blueStates[n] = 0;
      }
    }
    else
    {
      strip.setPixelColor(n, 25, 35, 35);  //Set pixels that are not chosen to be lit to off
    }
  }
  strip.show();
  delay(random(25));
}
