#define BLYNK_TEMPLATE_ID "INSET TEMPLATE ID HERE"
#define BLYNK_TEMPLATE_NAME "Smart Plant Monitor"
#define BLYNK_AUTH_TOKEN "INSERT AUTHENTICATION TOKEN HERE"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>

#define SOIL_MOISTURE_PIN A3   
#define PUMP_PIN A5            
#define DHTPIN A1              
#define DHTTYPE DHT11          

#define APP_KEY           "INSERT APP KEY HERE"        // Sinric Pro App Key
#define APP_SECRET        "INSERT APP SECRET HERE" // Sinric Pro App Secret
#define DEVICE_ID         "INSERT DEVICE ID HERE"                    // Sinric Pro Device ID

DHT dht(DHTPIN, DHTTYPE);      

// Wi-Fi credentials
char ssid[] = "WIFI NETWORK NAME";
char pass[] = "WIFI PASSWORD";

// Pump control flags
bool wateredRecently = false; 
unsigned long lastWateredTime = 0;
unsigned long lastPrintTime = 0;  // Timer for serial printing
const unsigned long cooldownPeriod = 60000;  // 1-minute cooldown after watering
const unsigned long printInterval = 10000;   // 10 seconds for printing readings
bool shouldPrintStatus = false; // Flag to print status immediately after watering

// Non-blocking pump activation
bool pumpActive = false;              // Track pump active state
unsigned long pumpStartTime = 0;      // Time when the pump was turned on
const unsigned long pumpDuration = 2000; // Pump duration in milliseconds (2 seconds)
const unsigned long wateredRecentlyDuration = 60000; // Duration to keep wateredRecently as "Yes"

// Plant-specific thresholds for Cactus and Pothos
struct PlantSettings {
  int soilDryThreshold;
  float tempHighThreshold;
};

PlantSettings cactus = {3000, 20.0};   // Cactus needs dry soil and higher temperature threshold
PlantSettings pothos = {3000, 20.0};   // Pothos needs less dry soil and a lower temperature threshold
PlantSettings currentPlantSettings = cactus;  // Default to cactus settings

// Blynk virtual pins
#define VPIN_SOIL V1
#define VPIN_TEMP V2
#define VPIN_HUMIDITY V3
#define VPIN_PUMP_CONTROL V4
#define VPIN_PLANT_SELECTOR V5
#define VPIN_MANUAL_WATER V6  // New virtual pin for manual 2-second watering

// Callback function for Sinric Pro control
bool onPowerState(const String &deviceId, bool &state) {
    Serial.printf("Device %s turned %s via Alexa\n", deviceId.c_str(), state ? "on" : "off");
    
    if (state) {
        // Activate pump for 2 seconds if Alexa turns it on
        activatePump();
    } else {
        digitalWrite(PUMP_PIN, HIGH); // Turn off pump
        pumpActive = false; // Reset pump active state
    }

    return true; // Return true to acknowledge the command
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Smart Plant Monitor...");
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize pump pin
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH);  // Ensure pump is off initially

  // Connect to Wi-Fi and Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Set up Sinric Pro
  SinricProSwitch &mySwitch = SinricPro[DEVICE_ID];
  mySwitch.onPowerState(onPowerState); // Register the power state callback for Alexa
  SinricPro.begin(APP_KEY, APP_SECRET); // Initialize Sinric Pro with App Key and App Secret

  delay(2000);
}

void monitorPlant() {
  int moistureValue = getAverageMoisture(10);
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if temperature and humidity are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Error reading from DHT sensor!");
    return; // Skip this cycle if sensor readings are invalid
  }

  // Send data to Blynk
  Blynk.virtualWrite(VPIN_SOIL, moistureValue);
  Blynk.virtualWrite(VPIN_TEMP, temperature);
  Blynk.virtualWrite(VPIN_HUMIDITY, humidity);

  // Print sensor readings every 10 seconds or immediately if flag is set
  if (shouldPrintStatus || millis() - lastPrintTime >= printInterval) {
    Serial.print("Soil Moisture Value: ");
    Serial.println(moistureValue);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    Serial.print("Temperature: ");
    Serial.println(temperature);
    Serial.print("Watered Recently: ");
    Serial.println(wateredRecently ? "Yes" : "No");
    lastPrintTime = millis();  // Update the last print time
    shouldPrintStatus = false; // Reset the flag after printing
  }

  // Reset wateredRecently to "No" after 1 minute
  if (wateredRecently && (millis() - lastWateredTime >= wateredRecentlyDuration)) {
    wateredRecently = false;
    shouldPrintStatus = true;  // Set flag to print the updated status immediately
  }

  // Check if plant needs watering based on moisture and temperature
  if (!wateredRecently && (millis() - lastWateredTime) > cooldownPeriod) {
    if (moistureValue > currentPlantSettings.soilDryThreshold && temperature >= currentPlantSettings.tempHighThreshold) {
      Serial.println("Conditions met! Activating pump...");
      activatePump();
    }
  }
}

void activatePump() {
  if (!pumpActive) { // Start the pump only if it's not already active
    Serial.println("Activating pump for 2 seconds.");
    digitalWrite(PUMP_PIN, LOW); // Turn on the pump
    pumpStartTime = millis(); // Record the start time
    pumpActive = true; // Set pump as active
    wateredRecently = true; // Set wateredRecently as true when pump is activated
    lastWateredTime = millis(); // Update last watered time
    shouldPrintStatus = true; // Set flag to immediately print status in the next loop
  }
}

void checkPumpStatus() {
  // Check if pump is active and if the duration has passed
  if (pumpActive && (millis() - pumpStartTime >= pumpDuration)) {
    digitalWrite(PUMP_PIN, HIGH); // Turn off the pump
    Serial.println("Pump deactivated after watering.");
    pumpActive = false; // Reset pump active state
    
    // Update Alexa to reflect that the pump is off
    SinricProSwitch &mySwitch = SinricPro[DEVICE_ID];
    mySwitch.sendPowerStateEvent(false); // Notify Alexa that the device is now off
  }
}

// Average soil moisture readings
int getAverageMoisture(int numReadings) {
  int sum = 0;
  for (int i = 0; i < numReadings; i++) {
    sum += analogRead(SOIL_MOISTURE_PIN);
    delay(10);
  }
  return sum / numReadings;
}

// Blynk function to select plant type
BLYNK_WRITE(VPIN_PLANT_SELECTOR) {
  int plantType = param.asInt();

  // Update current plant settings based on selected type
  if (plantType == 1) {
    currentPlantSettings = cactus;
  } else if (plantType == 2) {
    currentPlantSettings = pothos;
  }

  Serial.print("Selected Plant Type: ");
  Serial.println(plantType == 1 ? "Cactus" : "Pothos");
}

// Blynk function for manual pump control via V4
BLYNK_WRITE(VPIN_PUMP_CONTROL) {
  int pumpControl = param.asInt();
  
  if (pumpControl == 1) { // If V4 is set to 1, activate pump for 2 seconds
    digitalWrite(PUMP_PIN, LOW); // Turn off pump immediately if V4 is set to 0
  } else {
    digitalWrite(PUMP_PIN, HIGH); // Turn off pump immediately if V4 is set to 0
    pumpActive = false;
    Serial.println("Pump turned off via Blynk V4 control.");
  }
}

// Blynk function for manual 2-second watering
BLYNK_WRITE(VPIN_MANUAL_WATER) {
  int buttonState = param.asInt();
  
  if (buttonState == 1) { // When the button is pressed
    activatePump();  // Activate pump for 2 seconds
  }
}

void loop() {
  Blynk.run();
  SinricPro.handle();  // Process Alexa commands
  monitorPlant();      // Continuously monitor plant conditions in the loop
  checkPumpStatus();   // Check if pump duration has passed and turn it off if needed
}
