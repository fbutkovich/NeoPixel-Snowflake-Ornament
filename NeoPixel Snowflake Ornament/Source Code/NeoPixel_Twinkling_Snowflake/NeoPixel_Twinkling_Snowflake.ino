#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <SPI.h>
#include <RH_RF69.h>
#include "Adafruit_MCP23008.h"

#define PIN A3  //NeoPixel data line
#define RANDOMSEED A0 //Analog input which is used for generating random seed value
#define NumPixels 40  //Number of pixels on snowflake PCB
#define BRIGHTNESS 25 //Global brightness value for NeoPixels
#define PIN_SWITCH 3  //Input pin on MCP23008 I/O expander for momentary pushbutton switch, which is pulled up HIGH
#define RFM69_INT     A2
#define RFM69_CS      A1
#define RFM69_RST     0

#define RF69_FREQ 915.0   //Setting the radio frequency to operate from, the reciever MUST also be set to this frequency

#define N_TEMP_SAMPLES 30 //Number of temperature samples to average

//Floating point arrays for storing color value of pixels
float redStates[NumPixels];
float blueStates[NumPixels];
float greenStates[NumPixels];
//Controls how slowly the pixels "die" after they are flased by the twinkling animation
float fadeRate = 0.95;

//Integer variable that gets incremented every time the flashing mode button is pressed
uint8_t FLASHING_MODE = 0;
uint8_t CURRENT_PIXELS = 0;
uint8_t i = 0;
/*Counter variable which gets incremented every time a wireless temperature data transmission is received from the
  standalone temperature controller*/
uint8_t TempIndex = 0;
const long ExplosionDelay[4] = {150, 125, 100, 75};
uint8_t ExplosionColor[4][3] = {{0, 50, 50}, {25, 75, 75}, {50, 100, 100}, {125, 125, 125}};
//This array stores all of the acquired temperature data samples and is used for averaging
uint8_t RandomColors[3] = {};
int TempArray[N_TEMP_SAMPLES] = {};

/*Generally, you should use "unsigned long" for variables that hold time
  because the value will quickly become too large for an int to store*/
unsigned long previousMillis = 0; //Stores the last time the delayed action occured
unsigned long previousMillis2 = 0;

Adafruit_MCP23008 mcp;  //Initialization of I/O expander object
RH_RF69 rf69(RFM69_CS, RFM69_INT);    //Initialization of radio module object over SPI communication.

/*NEO_KHZ800 800 KHz bitstream (most NeoPixel products w/WS2812 LEDs), NEO_GRB NumPixels
  are wired for GRB bitstream (most NeoPixel products)*/
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NumPixels, PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
  Serial.begin(9600);
  //Begin I2C communication with I/O expander, use default I2C bus address 0
  mcp.begin();
  //Being NeoPixel communication at specified data rate of 800kHz
  strip.begin();

  //Initialize input pin on MCP23008 I/O expander for momentary pushbutton switch, which is pulled up HIGH
  mcp.pinMode(PIN_SWITCH, INPUT);
  mcp.pullUp(PIN_SWITCH, HIGH);  //Turn on a 100K pullup internally

  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);

  //Reset RFM69 radio module by toggling the reset pin HIGH then LOW
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);

  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
                  };

  //Begin SPI communication with radio module
  rf69.init();
  rf69.setFrequency(RF69_FREQ);
  rf69.setTxPower(14, true);
  //All radio modules that communicate with each other should have the same encryption key and frequency
  rf69.setEncryptionKey(key);
  //Set NeoPixel brightness using the global brightness variable
  strip.setBrightness(BRIGHTNESS);
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
  /*Initialize all immediate values to be averaged within the temperature array to 10 to prevent triggering of any
    temperature based animations as soon as the board is powered on*/
  for (uint8_t i = 0; i < N_TEMP_SAMPLES - 1; i++)
  {
    TempArray[i] = 10;
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
      TemperatureBasedTwinkling(currentMillis);
      break;
    case 1:
      SinglePixelTwinkling(currentMillis);
      break;
    case 2:
      FireworkExplosion(currentMillis);
      break;
    case 3:
      theaterChase(strip.Color(RandomColors[0], RandomColors[1], RandomColors[2]), currentMillis, 50);
      break;
    case 4:
      rainbow(currentMillis, 50);
      break;
    case 5:
      Comet(RandomColors[0], RandomColors[1], RandomColors[2], 10, currentMillis, 50);
      break;
    case 6:
      CometRainbow(10, currentMillis, 50);
      break;
    default:
      for (uint8_t n = 0; n < NumPixels; n++)
      {
        strip.setPixelColor(n, 0, 0, 0);
      }
      break;
  }
}

//This is a polling type function which is called every time main loop runs
void ChangeFlashingMode()
{
  if (ReadButton())
  {
    //Increment the int variable "FLASHING_MODE" by one every time the button is pressed
    FLASHING_MODE++;
    for (uint8_t i = 0; i < 3; i++)
    {
      RandomColors[i] = random(200);
    }
    //Reset the mode counter variable
    if (FLASHING_MODE > 6)
    {
      FLASHING_MODE = 0;
    }
  }
}

//Function which uses debounce technique by Henry Cheung @ https://www.e-tinkers.com/ to detect a button press
bool ReadButton()
{
  static uint16_t state = 0;
  state = (state << 1) | mcp.digitalRead(3) | 0xfe00;
  return (state == 0xff00);
}

/*This animation uses recorded temperature data from the AverageTemp() function to change how frequently and
  which color the NeoPixel LEDS are "twinkled" at*/
void TemperatureBasedTwinkling(unsigned long currentMillis)
{
  int frequency = map(AverageOutdoorTemp(), -20, 20, 1, 10);
  if (currentMillis - previousMillis >= random(abs(AverageOutdoorTemp()), 50))
  {
    //Save the last time this loop iteration happened
    previousMillis = currentMillis;
    //Begin twinkling if average reported outdoor temperature is less than 4*C (~40*F)
    if (AverageOutdoorTemp() < 4)
    {
      /*Sets twinkling frequency by using the random() function to generate an random integer up to maximum specified which changes with temperature,
        the lower the number (colder outside), the more frequent the pixels will twinkle*/
      if (random(frequency) == 1)
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
          strip.setPixelColor(n, 25, 35, 35);
        }
      }
    }
    else
    {
      for (uint8_t n = 0; n < NumPixels; n++)
      {
        strip.setPixelColor(n, 0, 0, 0);
      }
    }
  }
  //Acquire temperature data @ 1Hz sampling rate using a non-blocking timer counter
  else if (currentMillis - previousMillis2 >= 1000)
  {
    previousMillis2 = currentMillis;
    ReadTemperature();
  }
  strip.show();
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
  strip.show();
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
        strip.setPixelColor(n, ExplosionColor[0][0], ExplosionColor[0][1], ExplosionColor[0][2]);
      }
    }
    else if (CURRENT_PIXELS == 2)
    {
      for (uint8_t n = 0; n < 8; n++)
      {
        strip.setPixelColor(n, ExplosionColor[0][0] / CURRENT_PIXELS, ExplosionColor[0][1] / CURRENT_PIXELS, ExplosionColor[0][2] / CURRENT_PIXELS);
      }
      for (uint8_t n = 8; n < 24; n++)
      {
        strip.setPixelColor(n, ExplosionColor[1][0], ExplosionColor[1][1], ExplosionColor[1][2]);
      }
    }
    else if (CURRENT_PIXELS == 3)
    {
      for (uint8_t n = 0; n < 8; n++)
      {
        strip.setPixelColor(n, ExplosionColor[0][0] / CURRENT_PIXELS, ExplosionColor[0][1] / CURRENT_PIXELS, ExplosionColor[0][2] / CURRENT_PIXELS);
      }
      for (uint8_t n = 8; n < 24; n++)
      {
        strip.setPixelColor(n, ExplosionColor[1][0] / CURRENT_PIXELS, ExplosionColor[1][1] / CURRENT_PIXELS, ExplosionColor[1][2] / CURRENT_PIXELS);
      }
      for (uint8_t n = 24; n < 32; n++)
      {
        strip.setPixelColor(n, ExplosionColor[2][0], ExplosionColor[2][1], ExplosionColor[2][2]);
      }
    }
    else if (CURRENT_PIXELS == 4)
    {
      for (uint8_t n = 0; n < 8; n++)
      {
        strip.setPixelColor(n, ExplosionColor[0][0] / CURRENT_PIXELS, ExplosionColor[0][1] / CURRENT_PIXELS, ExplosionColor[0][2] / CURRENT_PIXELS);
      }
      for (uint8_t n = 8; n < 24; n++)
      {
        strip.setPixelColor(n, ExplosionColor[1][0] / CURRENT_PIXELS, ExplosionColor[1][1] / CURRENT_PIXELS, ExplosionColor[1][2] / CURRENT_PIXELS);
      }
      for (uint8_t n = 24; n < 32; n++)
      {
        strip.setPixelColor(n, ExplosionColor[2][0] / CURRENT_PIXELS, ExplosionColor[2][1] / CURRENT_PIXELS, ExplosionColor[2][2] / CURRENT_PIXELS);
      }
      for (uint8_t n = 32; n < 40; n++)
      {
        strip.setPixelColor(n, ExplosionColor[3][0], ExplosionColor[3][1], ExplosionColor[3][2]);
      }
    }
    CURRENT_PIXELS++;
  }
  if (currentMillis - previousMillis >= 750)
  {
    for (uint8_t n = 0; n < NumPixels; n++)
    {
      strip.setPixelColor(n, 0, 0, 0);
    }
    CURRENT_PIXELS = 0;
  }
  strip.show();
}

//Theatre-style crawling lights animation provided by Adafruit open-source example
void theaterChase(uint32_t c, unsigned long currentMillis, uint8_t wait)
{
  if (currentMillis - previousMillis >= wait)
  {
    previousMillis = currentMillis;
    for (int q = 0; q < 3; q++)
    {
      for (uint16_t i = 0; i < strip.numPixels(); i = i + 3)
      {
        strip.setPixelColor(i + q, c);  //Turn every third pixel on
      }
      strip.show();
      delay(wait);
      for (uint16_t i = 0; i < strip.numPixels(); i = i + 3)
      {
        strip.setPixelColor(i + q, 0, 0, 0);      //Turn every third pixel off
      }
    }
  }
}

//Rainbow animation provided by Adafruit open-source example
void rainbow(unsigned long currentMillis, uint8_t wait)
{
  static int j = 0, q = 0;
  static boolean on = true;
  if (on)
  {
    for (int i = 0; i < strip.numPixels(); i = i + 3)
    {
      strip.setPixelColor(i + q, Wheel( (i + j) % 255)); //turn every third pixel on
    }
  }
  else
  {
    for (int i = 0; i < strip.numPixels(); i = i + 3)
    {
      strip.setPixelColor(i + q, 0);      //turn every third pixel off
    }
  }
  if (currentMillis - previousMillis >= wait)
  {
    on = !on; // toggel pixelse on or off for next time
  }
  strip.show(); // display
  q++; // update the q variable
  if (q >= 3 )
  { // if it overflows reset it and update the J variable
    q = 0;
    j++;
    if (j >= 256)
    {
      j = 0;
    }
  }
}

/*Input a value 0 to 255 to get a color value.
  the colours are a transition r - g - b - back to r*/
uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85)
  {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170)
  {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

//A "comet" effect where every pixel before the head one slowly fades to off
void Comet(uint8_t G, uint8_t R, uint8_t B, uint8_t tailLength, unsigned long currentMillis, uint16_t wait) {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t numPixel = strip.numPixels();   // Obtain the # of NeoPixels in the pixel strip
  if (currentMillis - previousMillis >= wait && i < numPixel + tailLength)
  {
    previousMillis = currentMillis;
    /* Step through each pixel in the clockwise direction
      until the end pixel has been reached */
    r = R / numPixel * i; /* Divide the initial 8-bit brightness value for this specific color by the number
    of total pixels times the current pixel, this makes each pixel brighter as it steps through the loop */
    g = G / numPixel * i;
    b = B / numPixel * i;
    strip.setPixelColor(i, g, r, b);  /* Set the pixel color using the color values which change dynamically
    as each loop interation increments */
    strip.show();
    strip.setPixelColor(abs(i - tailLength), 0, 0, 0);  /* Turn off every pixel after the total length of the
    "tail" behind the lead pixel */
    i++;
  }
  if (i == numPixel + tailLength)
  {
    i = 0;
  }
}

//A rainbow variation of the "comet" effect where every pixel before the head one slowly fades to off
void CometRainbow(uint8_t tailLength, unsigned long currentMillis, uint16_t wait) {
  uint8_t numPixel = strip.numPixels();   // Obtain the # of NeoPixels in the pixel strip
  if (currentMillis - previousMillis >= wait && i < numPixel + tailLength)
  {
    previousMillis = currentMillis;
    strip.setPixelColor(i, Wheel(map(i, 0 , 39, 0, 255)));  /* Set the pixel color using the color values which change dynamically
    as each loop interation increments */
    strip.show();
    strip.setPixelColor(abs(i - tailLength), 0, 0, 0);  /* Turn off every pixel after the total length of the
    "tail" behind the lead pixel */
    i++;
  }
  if (i == numPixel + tailLength)
  {
    i = 0;
  }
}

/*This function is called by animations that are based on temperature and uses the RFM69 radio to retrieve
  outdoor temperature data wirelessly from the temperature controller*/
void ReadTemperature()
{
  //Only record temperature if the properly encrypted radio message is received
  if (rf69.available())
  {
    //Incoming packet buffer of uint8_t datatype
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf69.recv(buf, &len))
    {
      //If incoming message buffer is empty return nothing
      if (!len) return;
      buf[len] = 0;
      //Convert incoming char datatype buffer to integer using the atoi() function
      TempArray[TempIndex] = atoi((char*)buf);
      //Increment TempIndex counter
      TempIndex++;
      //Comment out of printing out debugging temperature data is not desired
      TemperatureDebuggingData();
    }
    //Reset the TempIndex counter every time it equals the max number of defined samples to average
    if (TempIndex > N_TEMP_SAMPLES - 1)
    {
      TempIndex = 0;
    }
  }
}

/*This function averages N samples of the incoming wireless temperature sensor data to create a smoothed buffer to prevent
  sudden ON/OFF behavior of the twinkling pattern*/
int AverageOutdoorTemp()
{
  int sum = 0;
  for (int i = 0; i < N_TEMP_SAMPLES; i++)
  {
    //Use the += operator to sum each element @ index i with the previous element
    sum += TempArray[i];
  }
  //Calculate average by dividing total temperature array sum by max number of specified samples
  return sum / N_TEMP_SAMPLES;
}

//Print serial debugging data for average outdoor temperature
void TemperatureDebuggingData()
{
  Serial.print("Average temperature: ");
  Serial.print(AverageOutdoorTemp());
  Serial.println("*C");
  Serial.println("Signal strength: " + String(rf69.lastRssi()) + "dBm\n");
}
