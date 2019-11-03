/**********************************************************
 * LoRa sample code snipped: Reading sensors and sending
 * LoRa messages.
 * ---
 * After setup, the sensors are read in intervals and the 
 * sensor data message is sent using LoRa.
 * - USB voltage measurement
 * - DS18B20 temperature sensors (multiple) are read from pin D3
 * - DHT22 temperature/humidity sensor is read from pin D4
 * - Weight measured with load cell and HX711 ADC from pins A0/A1
 * ---
 * message version (aka command):
 *  0: sensor data v0
 **********************************************************/

#include "SensorReader.h"
#include "DraginoLoRa.h"
//#include <Adafruit_SleepyDog.h>

typedef struct beesensor_t {
  byte version;
  struct { 
    short outer;
    short lower;
    short middle;
    short upper;
  } temperature; 
  struct {
    short outer;
  } humidity;
  short weight;
  short battery;
};

#define MS 1L
#define SEC (1000*MS)
#define MIN (60*SEC)

#define MESSAGE_VERSION 0
#define INTERVAL          (5*MIN)
#define TRANSMISSION_WAIT (30*SEC)
#define SETUP_DURATION    (2*MIN)

union {
  beesensor_t sensor;
  byte bytes[1+8+2+2+2];
} message;

unsigned long lastTransmissionMs = 0L;

SensorReader sensor = SensorReader();
DraginoLoRa radio = DraginoLoRa();
  

/* Setup ******************************************/

void setup() {
  Serial.begin(115200);
  Serial.print("Start LoRa test script with sensor message v");
  Serial.println(MESSAGE_VERSION);

  // initialize sensor message
  message.sensor.version = MESSAGE_VERSION;

  sensor.begin();
  radio.begin();

  sensor.listTemperatureSensors();
}

/* Loop ******************************************/

void loop() {
  if (millis() < SETUP_DURATION) {
    Serial.println();
    sensor.listRawWeight();
    readSensors();
    printSensorData();
  } else if (lastTransmissionMs == 0L || millis() >= lastTransmissionMs + INTERVAL) {
    lastTransmissionMs = millis();
    Serial.println();
    readSensors();
    printSensorData();
    Serial.print("Message: ");
    printBufferAsString(message.bytes, sizeof(message)); 
    radio.send(message.bytes, sizeof(message));
  } else if (radio.isTransmitting() && millis() < lastTransmissionMs + TRANSMISSION_WAIT) {
    // Wait for transmission complete
  } else {
    sleep();
  }

  os_runloop_once();
}

/* Helper methods ******************************************/

short asShort(float value) {
  if (isnan(value) || value == -127.0) return WINT_MIN;
  return value * 100;
}

void readSensors() {
  sensor.powerUp();
  sensor.startReading();
  
  message.sensor.temperature.upper = asShort(sensor.getUpperTemperature());
  message.sensor.temperature.middle = asShort(sensor.getMiddleTemperature());
  message.sensor.temperature.lower = asShort(sensor.getLowerTemperature());
  message.sensor.temperature.outer = asShort(sensor.getOuterTemperature());
  message.sensor.humidity.outer = asShort(sensor.getOuterHumidity());
  message.sensor.weight = asShort(sensor.getWeight());
  message.sensor.battery = asShort(sensor.getVoltage());

  sensor.stopReading();
}

void printSensorData() {
  if (message.sensor.weight > WINT_MIN) {
    Serial.print(message.sensor.weight / 100.0);
    Serial.println(" kg");
  }
  if (message.sensor.temperature.lower > WINT_MIN) {
    Serial.print(message.sensor.temperature.lower / 100.0);
    Serial.println(" C lower level");
  }
  if (message.sensor.temperature.middle > WINT_MIN) {
    Serial.print(message.sensor.temperature.middle / 100.0);
    Serial.println(" C middle level");
  }
  if (message.sensor.temperature.upper > WINT_MIN) {
    Serial.print(message.sensor.temperature.upper / 100.0);
    Serial.println(" C upper level");
  }
  if (message.sensor.temperature.outer > WINT_MIN) {
    Serial.print(message.sensor.temperature.outer / 100.0);
    Serial.println(" C outside");
  }
  if (message.sensor.humidity.outer > WINT_MIN) {
    Serial.print(message.sensor.humidity.outer / 100.0);
    Serial.println(" % rel");
  }
  if (message.sensor.battery > WINT_MIN) {
    Serial.print(message.sensor.battery / 100.0);
    Serial.println(" Vcc");
  }
}

void printBufferAsString(byte* buffer, int length) {
  Serial.print("\"");
  for (uint8_t i = 0; i < length; i++) {
    if (buffer[i] < 16) Serial.print("0");
    Serial.print(buffer[i], HEX);
  }
  Serial.println("\"");
}

void sleep() {
  // shut down
  sensor.powerDown();
  Serial.flush();
  #ifdef USBCON
    USBDevice.detach();
  #endif

  // for lower power consumption use deep sleep mode 
  // (as long a possible, will be approx. 8 sec).
  delay(8000); 
  //Watchdog.sleep();  

  // wake up
  #ifdef USBCON
    USBDevice.init();
    USBDevice.attach();
  #endif
  Serial.print('.');
}
