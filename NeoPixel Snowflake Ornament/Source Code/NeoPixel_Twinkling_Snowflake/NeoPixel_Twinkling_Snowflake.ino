#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "Adafruit_MCP23008.h"

#define PIN A3  //NeoPixel data line
#define RANDOMSEED A1 //Analog input which is used for generating random seed value
#define NumPixels 40  //Number of pixels on snowflake PCB
#define BRIGHTNESS 50 //Global brightness value
#define PIN_SWITCH 3  //Input pin on MCP23008 I/O expander for momentary pushbutton switch, which is pulled up HIGH

//Floating point arrays for storing color value of pixels
float redStates[NumPixels];
float blueStates[NumPixels];
float greenStates[NumPixels];
//Controls how slowly the pixels "die" after they are flased by the twinkling animation
float fadeRate = 0.95;

uint8_t FLASHING_MODE = 0;
uint8_t CURRENT_PIXELS = 0;
const long ExplosionDelay[4] = {200, 150, 100, 50};
uint8_t ExplosionColor[4][3] = {{0, 50, 50}, {25, 75, 75}, {65, 100, 100}, {125, 125, 125}};

/*Generally, you should use "unsigned long" for variables that hold time
  because the value will quickly become too large for an int to store*/
unsigned long previousMillis = 0;        //Stores the last time the delayed action occured

Adafruit_MCP23008 mcp;

/*NEO_KHZ800 800 KHz bitstream (most NeoPixel products w/WS2812 LEDs), NEO_GRB NumPixels
  are wired for GRB bitstream (most NeoPixel products)*/
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NumPixels, PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
  mcp.begin();      //Use default I2C bus address 0
  //Being NeoPixel communication at specified data rate of 800kHz
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  //Initialize input pin on MCP23008 I/O expander for momentary pushbutton switch, which is pulled up HIGH
  mcp.pinMode(PIN_SWITCH, INPUT);
  mcp.pullUp(PIN_SWITCH, HIGH);  // turn on a 100K pullup internally
  /*Reads transient noise from analog input A1 and uses it to generate the random seed value so that every time the device is power cylced,
    new random values are used for the animation*/
  randomSeed(analogRead(RANDOMSEED));
  //Initialize all pixels states to no color
  for (uint8_t i = 0; i < NumPixels; i++)
  {
    redStates[i] = 0;
    greenStates[i] = 0;
    blueStates[i] = 0;
  }
  strip.show(); //Initialize all NumPixels to 'off'
}

void loop ()
{
  //Retrieve the current time in milliseconds since the code began running using the C function millis()
  unsigned long currentMillis = millis();
  //Check if the flashing mode change pushbutton has been pressed
  ChangeFlashingMode();
  //Switch case structure for changing the current pixel animation
  switch (FLASHING_MODE)
  {
    case 0:
      FireworkExplosion(currentMillis);
      break;
    case 1:
      TemperatureBasedTwinkling(currentMillis);
      break;
    case 2:
      SinglePixelTwinkling(currentMillis);
      break;
    default:
      for (uint8_t n = 0; n < NumPixels; n++)
      {
        strip.setPixelColor(n, 0, 0, 0);
      }
      break;
  }
  strip.show();
}

void ChangeFlashingMode()
{
  if (ReadButton())
  {
    //Increment the int variable "FLASHING_MODE" by one every time the button is pressed
    FLASHING_MODE++;
    if (FLASHING_MODE > 2)
    {
      FLASHING_MODE = 0;
    }
    Serial.println(FLASHING_MODE);
  }
}

bool ReadButton()
{
  //Uses debounce technique by Henry Cheung @ https://www.e-tinkers.com/
  static uint16_t state = 0;
  state = (state << 1) | mcp.digitalRead(3) | 0xfe00;
  return (state == 0xff00);
}

void TemperatureBasedTwinkling(unsigned long currentMillis)
{
  if (currentMillis - previousMillis >= random(25, 50))
  {
    //Save the last time this loop iteration happened
    previousMillis = currentMillis;
    /*Sets twinkling frequency by using the random() function to generate an random integer up to maximum specified,
      the lower the number, the more frequent the pixels will twinkle*/
    if (random(5) == 1)
    {
      //Use random function again to choose a single pixel out of the total to set to a random white color
      uint8_t i = random(NumPixels);
      if (redStates[i] < 1 && greenStates[i] < 1 && blueStates[i] < 1)
      {
        redStates[i] = random(100, 255);
        greenStates[i] = 255;
        blueStates[i] = 255;
      }
    }
    for (uint8_t n = 0; n < NumPixels; n++)
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
  }
}

void SinglePixelTwinkling(unsigned long currentMillis)
{
  if (currentMillis - previousMillis >= random(25))
  {
    //Save the last time this loop iteration happened
    previousMillis = currentMillis;
    /*Sets twinkling frequency by using the random() function to generate an random integer up to maximum specified,
      the lower the number, the more frequent the pixels will twinkle*/
    if (random(5) == 1)
    {
      //Use random function again to choose a single pixel out of the total to set to a random white color
      uint8_t i = random(NumPixels);
      if (redStates[i] < 1 && greenStates[i] < 1 && blueStates[i] < 1)
      {
        redStates[i] = random(100, 255);
        greenStates[i] = 255;
        blueStates[i] = 255;
      }
    }
    for (uint8_t n = 0; n < NumPixels; n++)
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
        strip.setPixelColor(n, 0, 0, 0);  //Set pixels that are not chosen to be lit to off
      }
    }
  }
}

void FireworkExplosion(unsigned long currentMillis)
{
  if (currentMillis - previousMillis >= ExplosionDelay[CURRENT_PIXELS])
  {
    //Save the last time this loop iteration happened
    previousMillis = currentMillis;
    if (CURRENT_PIXELS == 1)
    {
      for (uint8_t n = 0; n < 8; n++)
      {
        strip.setPixelColor(n, ExplosionColor[CURRENT_PIXELS - 1][0], ExplosionColor[CURRENT_PIXELS - 1][1], ExplosionColor[CURRENT_PIXELS - 1][2]);
      }
    }
    else if (CURRENT_PIXELS == 2)
    {
      for (uint8_t n = 8; n < 24; n++)
      {
        strip.setPixelColor(n, ExplosionColor[CURRENT_PIXELS - 1][0], ExplosionColor[CURRENT_PIXELS - 1][1], ExplosionColor[CURRENT_PIXELS - 1][2]);
      }
    }
    else if (CURRENT_PIXELS == 3)
    {
      for (uint8_t n = 24; n < 32; n++)
      {
        strip.setPixelColor(n, ExplosionColor[CURRENT_PIXELS - 1][0], ExplosionColor[CURRENT_PIXELS - 1][1], ExplosionColor[CURRENT_PIXELS - 1][2]);
      }
    }
    else if (CURRENT_PIXELS == 4)
    {
      for (uint8_t n = 32; n < 40; n++)
      {
        strip.setPixelColor(n, ExplosionColor[CURRENT_PIXELS - 1][0], ExplosionColor[CURRENT_PIXELS - 1][1], ExplosionColor[CURRENT_PIXELS - 1][2]);
      }
    }
    CURRENT_PIXELS++;
  }
  else if (currentMillis - previousMillis >= 500)
  {
    for (uint8_t n = 0; n < NumPixels; n++)
    {
      strip.setPixelColor(n, 0, 0, 0);
    }
    CURRENT_PIXELS = 0;
  }
}
