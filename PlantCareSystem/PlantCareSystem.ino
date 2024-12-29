#include <FirebaseESP8266.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266WiFi.h>
#include <DHT.h>

// Wi-Fi credentials
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Firebase credentials
#define FIREBASE_HOST "plantcaresystem-182d7-default-rtdb.europe-west1.firebasedatabase.app"
#define FIREBASE_AUTH "YourFirebaseDatabaseSecret"

FirebaseData firebaseData;

BlynkTimer timer;

// Actuator pins
const int pumpPin = D1;
const int curtainPin = D2;
const int windowPin = D3;

// Sensor pins
const int soilMoisturePin = A0; // Analog pin for soil moisture sensor
#define DHTPIN D4               // Digital pin for DHT sensor
#define DHTTYPE DHT22           // DHT22 type
const int ldrPin = A1;          // Analog pin for LDR

DHT dht(DHTPIN, DHTTYPE);

// Variables to store sensor data
float temperature = 0;
float humidity = 0;
int soilMoisture = 0;
int lightLevel = 0;
int lightHours = 0;

// Selected plant (default: Cactus)
String selectedPlant = "Cactus";

// Function prototypes
void readSensors();
void controlActuators();
void updateBlynk();
void fetchPlantData();
void trackLightHours();

void setup() {
  // Serial for debugging
  Serial.begin(115200);

  // Set actuator pins as output
  pinMode(pumpPin, OUTPUT);
  pinMode(curtainPin, OUTPUT);
  pinMode(windowPin, OUTPUT);

  // Initialize actuators as OFF
  digitalWrite(pumpPin, LOW);
  digitalWrite(curtainPin, LOW);
  digitalWrite(windowPin, LOW);

  // Initialize DHT sensor
  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to Wi-Fi...");
  }
  Serial.println("Connected to Wi-Fi");

  // Connect to Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);

  // Set up timers for periodic tasks
  timer.setInterval(2000L, readSensors); // Read sensors every 2 seconds
  timer.setInterval(5000L, controlActuators); // Control actuators every 5 seconds
  timer.setInterval(5000L, updateBlynk); // Update Blynk every 5 seconds
  timer.setInterval(60000L, trackLightHours); // Track light hours every minute
  timer.setInterval(10000L, fetchPlantData); // Fetch plant data every 10 seconds
}

void loop() {
  Blynk.run();
  timer.run();
}

// Read sensor data
void readSensors() {
  // Read soil moisture
  soilMoisture = analogRead(soilMoisturePin);

  // Read temperature and humidity from DHT22
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Read light level from LDR
  lightLevel = analogRead(ldrPin);

  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Send sensor data to Firebase
  Firebase.setFloat(firebaseData, "/sensorData/temperature", temperature);
  Firebase.setFloat(firebaseData, "/sensorData/humidity", humidity);
  Firebase.setInt(firebaseData, "/sensorData/soilMoisture", soilMoisture);
  Firebase.setInt(firebaseData, "/sensorData/lightLevel", lightLevel);

  Serial.println("Sensor data updated");
  Serial.print("Temperature: ");
  Serial.println(temperature);
  Serial.print("Humidity: ");
  Serial.println(humidity);
  Serial.print("Soil Moisture: ");
  Serial.println(soilMoisture);
  Serial.print("Light Level: ");
  Serial.println(lightLevel);
}

// Track light hours
void trackLightHours() {
  // Define a threshold for sufficient light (adjust based on calibration)
  int lightThreshold = 500;

  // Check if light level is above the threshold
  if (lightLevel > lightThreshold) {
    lightHours++;
  }

  // Send light hours to Firebase
  Firebase.setInt(firebaseData, "/sensorData/lightHours", lightHours);

  Serial.print("Light Hours: ");
  Serial.println(lightHours);
}

// Control actuators based on sensor data and plant profile
void controlActuators() {
  if (Firebase.get(firebaseData, "/plants/" + selectedPlant)) {
    int idealMoisture = Firebase.getInt(firebaseData, "/plants/" + selectedPlant + "/soilMoisture");
    int idealTemp = Firebase.getInt(firebaseData, "/plants/" + selectedPlant + "/temperature");
    int idealHumidity = Firebase.getInt(firebaseData, "/plants/" + selectedPlant + "/humidity");
    int idealLightHours = Firebase.getInt(firebaseData, "/plants/" + selectedPlant + "/lightHours");

    // Control pump
    if (soilMoisture < idealMoisture) {
      digitalWrite(pumpPin, HIGH);
      Firebase.setString(firebaseData, "/actuators/pump", "ON");
    } else {
      digitalWrite(pumpPin, LOW);
      Firebase.setString(firebaseData, "/actuators/pump", "OFF");
    }

    // Control curtain
    if (lightHours < idealLightHours) {
      digitalWrite(curtainPin, HIGH);
      Firebase.setString(firebaseData, "/actuators/curtain", "CLOSED");
    } else {
      digitalWrite(curtainPin, LOW);
      Firebase.setString(firebaseData, "/actuators/curtain", "OPEN");
    }

    // Control window
    if (humidity < idealHumidity) {
      digitalWrite(windowPin, HIGH);
      Firebase.setString(firebaseData, "/actuators/window", "OPEN");
    } else {
      digitalWrite(windowPin, LOW);
      Firebase.setString(firebaseData, "/actuators/window", "CLOSED");
    }
  } else {
    Serial.println("Error fetching plant data: " + firebaseData.errorReason());
  }
}

// Update Blynk with sensor and actuator data
void updateBlynk() {
  Blynk.virtualWrite(V1, temperature); // Measured temperature
  Blynk.virtualWrite(V2, humidity);    // Measured humidity
  Blynk.virtualWrite(V3, soilMoisture); // Measured soil moisture
  Blynk.virtualWrite(V4, lightLevel); // Measured light level

  Blynk.virtualWrite(V5, Firebase.getInt(firebaseData, "/plants/" + selectedPlant + "/temperature")); // Ideal temperature
  Blynk.virtualWrite(V6, Firebase.getInt(firebaseData, "/plants/" + selectedPlant + "/humidity")); // Ideal humidity
  Blynk.virtualWrite(V7, Firebase.getInt(firebaseData, "/plants/" + selectedPlant + "/soilMoisture")); // Ideal soil moisture
  Blynk.virtualWrite(V8, Firebase.getInt(firebaseData, "/plants/" + selectedPlant + "/lightHours")); // Ideal light hours
}

// Fetch plant data from Firebase based on selected plant
void fetchPlantData() {
  if (Firebase.get(firebaseData, "/plants/" + selectedPlant)) {
    Serial.println("Plant data fetched successfully");
  } else {
    Serial.println("Error fetching plant data: " + firebaseData.errorReason());
  }
}

// Handle plant selection in Blynk
BLYNK_WRITE(V0) {
  selectedPlant = param.asString();
  Firebase.setString(firebaseData, "/selectedPlant", selectedPlant);
  Serial.println("Selected plant: " + selectedPlant);
}
