#include <cp437font.h>
#include <LedMatrix.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <FirebaseArduino.h>
#include <math.h>

  
// Define constants
#define WIFI_SSID "<WIFI SSID>" 
#define WIFI_PASS "WIFI PASS" 
#define FIREBASE_HOST "<applicationName>.firebaseio.com"
#define FIREBASE_AUTH "<Firebase database secret>"
#define BUTTON_PIN D1
#define BUTTONLOW_PIN D0
#define RX_PIN D5
#define TX_PIN D3
#define MATRIX_CS_PIN D8

// Define software serial for GPS module
SoftwareSerial ss(RX_PIN, TX_PIN);

// Active status of ride
bool active = false;

// Strings that will contain GPS information
String latVal;
String NSind;
String lonVal;
String EWind;
String restVal;

// Previous LatLong, and current LatLong
double dPrevLon = 0;
double dPrevLat = 0;
double dCurrLon = 0;
double dCurrLat = 0;
long totalDistance = 0;

void setup() {
  // Serial and software serials config
  Serial.begin(115200);
  ss.begin(9600);
  
  // SoftwareSerial pinmode config for GPS module
  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);

  // Builtin LED config
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Button config
  pinMode(BUTTONLOW_PIN, OUTPUT);
  digitalWrite(BUTTONLOW_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Start and config firebase 
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); 
  Serial.println("Setup done.");
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print the WiFi IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// Read serial and await the right character
void waitForChar(char c){
  while(true){
    if(ss.available()){
      char current = (char)ss.read();
      if(current == c){
        break;
      }
    }
    delay(0);
  }
}

// Read the character
char readChar(){
  while(true){
    if(ss.available()){
      return (char) ss.read();
    }
    delay(0);
  }
}

// Extract the data from the NMEA GPGLL message
void getData(String data){
  // Split the strings at each comma and store them in variables
  int f1comma = data.indexOf(',');
  int f2comma = data.indexOf(',', f1comma + 1);
  int f3comma = data.indexOf(',', f2comma + 1);
  int f4comma = data.indexOf(',', f3comma + 1);
  int f5comma = data.indexOf(',', f4comma + 1);
  latVal = data.substring(f1comma + 1, f2comma);
  NSind = data.substring(f2comma + 1, f3comma);
  lonVal = data.substring(f3comma + 1, f4comma);
  EWind = data.substring(f4comma + 1, f5comma);
}

// Toggle the Active status when the button is pressed
void toggleActive(){
  if(active){
    active = false;
    Serial.println("Ride stoppped");
    // Start connecting to WiFi for internet access
    connectToWiFi();
    delay(1000);
    // Save the ride when internet access is available
    saveRideToFirebase(totalDistance / 1000);
    // Reset the position variables for the next ride
    dPrevLon = 0;
    dPrevLat = 0;
    dCurrLon = 0;
    dCurrLat = 0;
    totalDistance = 0;
    // Turn off the builtin led
    digitalWrite(LED_BUILTIN, HIGH);
  }else{
    active = true;
    Serial.println("Ride started"); 
    // Turn on the builtin led
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void saveRideToFirebase(long distance){  
  // Create JSON object and push to Firebase
  Serial.print("Trying to save the distance: "); Serial.print(distance);Serial.println("KM to firebase...");
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& newRide = jsonBuffer.createObject();
  newRide["distance"] = distance;
  newRide["date"] = "null"; // Needs to be filled by firebase onCreate trigger
  
  String name = Firebase.push("/KMTracker/Rides", newRide);
    if(Firebase.failed()){
      Serial.println("Firebase write failed!");
      Serial.println(Firebase.error());  
    }else{
      Serial.println("Ride saved to Firebase: " + name); // Print id on success
    }
}

// Update the counting distance 
void updateDistance(){
  float currLatitude = latVal.toFloat();
  float currLongitude = lonVal.toFloat();

  if(dCurrLat == 0.00 && dCurrLon == 0.00){
    dPrevLat = convertDegMinToDecDeg(currLatitude);
    dPrevLon = convertDegMinToDecDeg(currLongitude);
  }else{
      dPrevLat = dCurrLat;
      dPrevLon = dCurrLon;
  }
  dCurrLat = convertDegMinToDecDeg(currLatitude);
  dCurrLon = convertDegMinToDecDeg(currLongitude);

  // Add the new distance to the old distance
  totalDistance += calculateDistance(dCurrLat, dCurrLon, dPrevLat, dPrevLon);
  Serial.print("Current distance (m): "); Serial.println(totalDistance);
}

// Convert DegreesMinutes to Decimal degrees
double convertDegMinToDecDeg (float degMin) {
  double min = 0.0;
  double decDeg = 0.0;
 
  //get the minutes, fmod() requires double
  min = fmod((double)degMin, 100.0);
 
  //rebuild coordinates in decimal degrees
  degMin = (int) ( degMin / 100 );
  decDeg = degMin + ( min / 60 );
 
  return decDeg;
}


void connectToWiFi(){
   // WiFi config
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  // Await the connection of WiFi
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  Serial.println();
  // Print wifi status, only for debugging
  printWifiStatus();
}

// Calculate the distance with a formula that computes distance between two coordinates
float calculateDistance(float flat1, float flon1, float flat2, float flon2)
{
  float dist_calc=0;
  float dist_calc2=0;
  float diflat=0;
  float diflon=0;
  
  diflat=radians(flat2-flat1);
  flat1=radians(flat1);
  flat2=radians(flat2);
  diflon=radians((flon2)-(flon1));
  
  dist_calc = (sin(diflat/2.0)*sin(diflat/2.0));
  dist_calc2= cos(flat1);
  dist_calc2*=cos(flat2);
  dist_calc2*=sin(diflon/2.0);
  dist_calc2*=sin(diflon/2.0);
  dist_calc +=dist_calc2;
  
  dist_calc=(2*atan2(sqrt(dist_calc),sqrt(1.0-dist_calc)));
  
  dist_calc*=6371000.0; //Converting to meters
  return dist_calc;
}

void loop() {
  // If a ride is active, use the GPS module and update the distance with each difference between coordinates
    if(active){
      // Wait until the GPS module sents a $. After this char, the message follows
      waitForChar('$');
      char buf[65];
      for(int i = 0; i < 64; i++){
        char c = readChar();
        if(c == '\n'){
          buf[i] = '\0';
          break;
        }
      buf[i] = c;
      }
  
      String data = String(buf);
      // Only use GPGLL message of the NMEA protocol
      if(data.startsWith("GPGLL")){
        Serial.println(data);
        getData(data);
        Serial.print("Longitude: "); Serial.println(lonVal);
        Serial.print("Latitude: "); Serial.println(latVal);  
        Serial.println("");
        updateDistance();
    }
  }
  // Check if the button is pressed. If so, toggle the activity status
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    toggleActive();
    delay(1000);
  }
  delay(0);
}
