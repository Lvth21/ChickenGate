#include <Wire.h>
#include "RTClib.h"
#include "BluetoothSerial.h"

// Define pins
const int IN1 = 27; // Motor driver input 1
const int IN2 = 26; // Motor driver input 2

RTC_DS3231 rtc;
BluetoothSerial SerialBT;

// Sunrise and sunset times for each month (approximate, in 24-hour format)
const int sunriseHours[12] = {7, 7, 6, 5, 4, 4, 4, 5, 5, 6, 6, 7}; // Sunrise times by month
const int sunriseMinutes[12] = {30, 0, 0, 30, 30, 30, 30, 0, 30, 0, 40, 15};
const int sunsetHours[12] = {19, 19, 21, 20, 20, 21, 20, 20, 19, 19, 18, 18}; // Sunset times by month
const int sunsetMinutes[12] = {0, 30, 0, 30, 30, 0, 45, 30, 50, 15, 20, 0};

bool isDoorOpen = false; // Track the door state
bool isAutomaticMode = true; // Track if the system is in automatic mode

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ChickenDoorController"); // Bluetooth name
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Initialize motor control pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  
  
    // Get the current time
  DateTime now = rtc.now();
  int currentMonth = now.month() - 1; // Adjust for zero-based index
  int sunriseHour = sunriseHours[currentMonth];
  int sunriseMinute = sunriseMinutes[currentMonth];
  int sunsetHour = sunsetHours[currentMonth];
  int sunsetMinute = sunsetMinutes[currentMonth];

  // Determine if the door should be open or closed
  if (now.hour() > sunriseHour || 
     (now.hour() == sunriseHour && now.minute() >= sunriseMinute)) {
    if (now.hour() < sunsetHour || 
        (now.hour() == sunsetHour && now.minute() < sunsetMinute)) {
      // It is daytime; open the door
      openDoor();
    } else {
      // It is nighttime; close the door
      closeDoor();
    }
  } else {
    // It is nighttime; close the door
    closeDoor();
  }

  Serial.println("Setup complete. Door state adjusted based on time.");

  Serial.println("Bluetooth initialized. Waiting for commands...");
}

void loop() {
  // Handle Bluetooth commands
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim(); // Remove whitespace
    handleBluetoothCommand(command);
  }

  // Automatic operation based on RTC time
  DateTime now = rtc.now();
  int currentMonth = now.month() - 1; // Adjust for zero-based index

  // Get sunrise and sunset times for the current month
  int sunriseHour = sunriseHours[currentMonth];
  int sunriseMinute = sunriseMinutes[currentMonth];
  int sunsetHour = sunsetHours[currentMonth];
  int sunsetMinute = sunsetMinutes[currentMonth];
  
  // Check if it's after sunrise
  bool isAfterSunrise = (now.hour() > sunriseHour) || 
                        (now.hour() == sunriseHour && now.minute() >= sunriseMinute);

  // Check if it's after sunset
  bool isAfterSunset = (now.hour() > sunsetHour) || 
                       (now.hour() == sunsetHour && now.minute() >= sunsetMinute);


  // Check for sunrise time (only open the door if it's after sunrise and before sunset)
  if (isAutomaticMode && !isDoorOpen && isAfterSunrise && !isAfterSunset) {
    openDoor();
  }

  // Check for sunset time (only close the door if it's after sunset)
  if (isAutomaticMode && isDoorOpen && isAfterSunset) {
    closeDoor();
  }

  delay(1000); // Wait a second before next check
}

void handleBluetoothCommand(String command) {
  if (command.startsWith("settime")) {
    // Expected format: settime dd/MM/YYYY HH:mm:ss
    String datetime = command.substring(8); // Extract everything after "settime "
    datetime.trim();
    
    int day = datetime.substring(0, 2).toInt();
    int month = datetime.substring(3, 5).toInt();
    int year = datetime.substring(6, 10).toInt();
    int hour = datetime.substring(11, 13).toInt();
    int minute = datetime.substring(14, 16).toInt();
    int second = datetime.substring(17, 19).toInt();

    if (day > 0 && month > 0 && year > 0 && hour >= 0 && minute >= 0 && second >= 0) {
      rtc.adjust(DateTime(year, month, day, hour, minute, second));
      Serial.println("RTC time updated to: " + datetime);
      SerialBT.println("RTC time updated to: " + datetime);
    } else {
      Serial.println("Invalid date/time format");
      SerialBT.println("Invalid date/time format. Use: dd/MM/YYYY HH:mm:ss");
    }
  } else if (command.equalsIgnoreCase("open")) {
	isAutomaticMode = false;
    openDoor();
    Serial.println("Door opened via Bluetooth, switched to manual mode");
    SerialBT.println("Door opened");
  } else if (command.equalsIgnoreCase("close")) {
	isAutomaticMode = false;
    closeDoor();
    Serial.println("Door closed via Bluetooth, switched to manual mode");
    SerialBT.println("Door closed");
  } else if (command.equalsIgnoreCase("time")) {
    DateTime now = rtc.now();
    String currentTime = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
    SerialBT.println("Current time: " + currentTime);
    Serial.println("Time requested: " + currentTime);
  } else if (command.equalsIgnoreCase("date")) {
    DateTime now = rtc.now();
    String currentDate = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year());
    SerialBT.println("Current date: " + currentDate);
    Serial.println("Date requested: " + currentDate);
  } else if (command.equalsIgnoreCase("schedule")) {
    int currentMonth = rtc.now().month() - 1;
    String schedule = "Opens at " + String(sunriseHours[currentMonth]) + ":" + String(sunriseMinutes[currentMonth]) +
                      ", Closes at " + String(sunsetHours[currentMonth]) + ":" + String(sunsetMinutes[currentMonth]);
    SerialBT.println(schedule);
    Serial.println("Schedule requested: " + schedule);
  } else if (command.equalsIgnoreCase("status")) {
    String status = isDoorOpen ? "Door is open" : "Door is closed";
    SerialBT.println(status);
    Serial.println("Status requested: " + status);
  } else if (command.equalsIgnoreCase("stop")) {
    stopMotor();
    SerialBT.println("Motor stopped");
    Serial.println("Motor stopped via Bluetooth");
  } else if (command.equalsIgnoreCase("manual")) {
    isAutomaticMode = false;
    Serial.println("Switched to manual mode");
    SerialBT.println("Switched to manual mode");
  } else if (command.equalsIgnoreCase("auto")) {
    isAutomaticMode = true;
    Serial.println("Switched to automatic mode");
    SerialBT.println("Switched to automatic mode");
  } else {
    Serial.println("Unknown command: " + command);
    SerialBT.println("Unknown command: " + command);
  }
}


void openDoor() {
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, HIGH);
  isDoorOpen = true;
  Serial.println("Door opening...");
}

void closeDoor() {
  digitalWrite(IN1, HIGH); 
  digitalWrite(IN2, LOW);
  isDoorOpen = false;
  Serial.println("Door closing...");
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  Serial.println("Motor stopped");
}
