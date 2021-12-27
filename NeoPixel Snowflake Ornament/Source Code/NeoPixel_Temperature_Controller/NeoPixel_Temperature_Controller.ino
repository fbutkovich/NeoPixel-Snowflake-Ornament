#include "Wire.h"
#include <SPI.h>
#include <RH_RF69.h>

#define RF69_FREQ 915.0   //Setting the radio frequency to operate from, the reciever MUST be set to this frequency also.

//ADC pin that the 3950 NTC thermistor is connected to
#define THERMISTORPIN A3
//Resistance at 25 degrees C
#define THERMISTORNOMINAL 10000
//Temp for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
//How many samples to take and average
#define NUMSAMPLES 5
//The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
//The value of the 'other' resistor
#define SERIESRESISTOR 10000

//Supporting signals for RFM69HCW radio module
#define RFM69_INT     A2
#define RFM69_CS      A1
#define RFM69_RST     A0

RH_RF69 rf69(RFM69_CS, RFM69_INT);    //Initialization of radio module object over SPI communication.

int samples[NUMSAMPLES];

void setup()
{
  Serial.begin(9600);

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
}

void loop()
{
  char TemperatureData[4];
  sprintf (TemperatureData, "%i", ReadTemperature());
  rf69.send((uint8_t *)TemperatureData, strlen(TemperatureData));
  rf69.waitPacketSent();
  Serial.println(ReadTemperature());
  delay(1000);
}

int ReadTemperature()
{
  uint8_t i;
  float average;

  //Take N samples in a row, with a slight delay
  for (i = 0; i < NUMSAMPLES; i++) {
    samples[i] = analogRead(THERMISTORPIN);
    delay(10);
  }

  //Average all the samples out
  average = 0;
  for (i = 0; i < NUMSAMPLES; i++) {
    average += samples[i];
  }
  average /= NUMSAMPLES;

  //Convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;

  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert absolute temp to C

  return int(steinhart);
}
