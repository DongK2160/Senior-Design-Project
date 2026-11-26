#include <Arduino.h>
#include <Preferences.h>

// the config

const bool SIMULATION_MODE = true;

const unsigned long SAMPLE_INTERVAL_MS = 2000;

// the measurments

const float PH_MIN = 0.0f;
const float PH_MAX = 14.0f;

const float TEMP_MIN_C = 0.0f;
const float TEMP_MAX_C = 80.0f;

const float EC_MIN_US_CM = 0.0f;
const float EC_MAX_US_CM = 20000.0f;

const float TURBIDITY_MIN_NTU = 0.0f;
const float TURBIDITY_MAX_NTU = 1000.0f;


const float EC_REFERENCE_TEMP_C = 25.0f;


const float DEVELOPMENT_EC_TEMP_COEFFICIENT = 0.02f;

struct SensorData {
  float rawTemperature;
  float rawPH;
  float rawEC;
  float rawTurbidity;

  float temperature;
  float ph;
  float ec;
  float tds;
  float turbidity;

  bool temperatureValid;
  bool phValid;
  bool ecValid;
  bool tdsValid;
  bool turbidityValid;
};

SensorData sensorData;


struct CalibrationData {
  float phSlope;
  float phOffset;

  float ecFactor;

  float tdsFactor;

  float turbiditySlope;
  float turbidityOffset;

  bool phCalibrated;
  bool ecCalibrated;
  bool turbidityCalibrated;
};

CalibrationData calibration;

// thiss is the power status

struct PowerStatus {
  float supplyVoltage;

  bool measurementAllowed;
  bool lowEnergy;
  bool statusAvailable;
};

PowerStatus powerStatus;



struct SystemStatus {
  bool hardwareAvailable;
  bool calibrationLoaded;
  bool allMeasurementsValid;
};

SystemStatus systemStatus;



const int FILTER_SIZE = 5;

struct MovingAverageFilter {
  float values[FILTER_SIZE];

  int index;
  int count;
};

MovingAverageFilter temperatureFilter;
MovingAverageFilter phFilter;
MovingAverageFilter ecFilter;
MovingAverageFilter turbidityFilter;



Preferences preferences;

const char *PREFERENCES_NAMESPACE = "lqdguard";


unsigned long previousSampleTime = 0;


void initializeFilter(MovingAverageFilter &filter) {
  filter.index = 0;
  filter.count = 0;

  for (int i = 0; i < FILTER_SIZE; i++) {
    filter.values[i] = 0.0f;
  }
}

float updateFilter(MovingAverageFilter &filter, float value) {
  
  if (isnan(value) || isinf(value)) {
    return NAN;
  }

  filter.values[filter.index] = value;

  filter.index++;

  if (filter.index >= FILTER_SIZE) {
    filter.index = 0;
  }

  if (filter.count < FILTER_SIZE) {
    filter.count++;
  }

  float sum = 0.0f;

  for (int i = 0; i < filter.count; i++) {
    sum += filter.values[i];
  }

  if (filter.count == 0) {
    return NAN;
  }

  return sum / (float)filter.count;
}



float simulateTemperature() {
  // 23.5 C to 26.5 C
  return 25.0f + ((float)random(-15, 16) / 10.0f);
}

float simulatePH() {
  // pH 6.80 to 7.20
  return 7.0f + ((float)random(-20, 21) / 100.0f);
}

float simulateEC() {
  // 710 to 790 uS/cm
  return 750.0f + (float)random(-40, 41);
}

float simulateTurbidity() {
  // 15 to 25 nu
  return 20.0f + ((float)random(-50, 51) / 10.0f);
}



float readTemperatureHardware() {

  return NAN;
}

float readPHHardware() {

  return NAN;
}

float readECHardware() {

  return NAN;
}

float readTurbidityHardware() {


  return NAN;
}


void acquireSensorData() {
  if (SIMULATION_MODE) {
    sensorData.rawTemperature = simulateTemperature();
    sensorData.rawPH = simulatePH();
    sensorData.rawEC = simulateEC();
    sensorData.rawTurbidity = simulateTurbidity();

    systemStatus.hardwareAvailable = false;
  } else {
    sensorData.rawTemperature = readTemperatureHardware();
    sensorData.rawPH = readPHHardware();
    sensorData.rawEC = readECHardware();
    sensorData.rawTurbidity = readTurbidityHardware();

    systemStatus.hardwareAvailable = true;
  }
}

// digital filter

void filterSensorData() {
  sensorData.temperature =
      updateFilter(temperatureFilter,
                   sensorData.rawTemperature);

  sensorData.ph =
      updateFilter(phFilter,
                   sensorData.rawPH);

  sensorData.ec =
      updateFilter(ecFilter,
                   sensorData.rawEC);

  sensorData.turbidity =
      updateFilter(turbidityFilter,
                   sensorData.rawTurbidity);
}

// ph 

void calculatePH() {
  if (isnan(sensorData.ph)) {
    return;
  }

  
  sensorData.ph =
      calibration.phSlope * sensorData.ph
      + calibration.phOffset;
}


void calculateEC() {
  if (isnan(sensorData.ec)) {
    return;
  }

  sensorData.ec =
      sensorData.ec * calibration.ecFactor;
}

void applyECTemperatureCompensation() {
  if (isnan(sensorData.ec) ||
      isnan(sensorData.temperature)) {
    return;
  }

  float denominator =
      1.0f +
      DEVELOPMENT_EC_TEMP_COEFFICIENT *
      (sensorData.temperature - EC_REFERENCE_TEMP_C);

  if (fabs(denominator) > 0.0001f) {
    sensorData.ec =
        sensorData.ec / denominator;
  } else {
    sensorData.ec = NAN;
  }
}

void applyPHTemperatureCompensation() {

}


void calculateTDS() {
  if (isnan(sensorData.ec)) {
    sensorData.tds = NAN;
    return;
  }

  sensorData.tds =
      sensorData.ec * calibration.tdsFactor;
}

// turbiditty 

void calculateTurbidity() {
  if (isnan(sensorData.turbidity)) {
    return;
  }

  sensorData.turbidity =
      calibration.turbiditySlope *
      sensorData.turbidity +
      calibration.turbidityOffset;
}


void setDevelopmentCalibration() {
 

  calibration.phSlope = 1.0f;
  calibration.phOffset = 0.0f;

  calibration.ecFactor = 1.0f;

  calibration.tdsFactor = 0.5f;

  calibration.turbiditySlope = 1.0f;
  calibration.turbidityOffset = 0.0f;

  calibration.phCalibrated = false;
  calibration.ecCalibrated = false;
  calibration.turbidityCalibrated = false;
}



void saveCalibration() {
  preferences.begin(PREFERENCES_NAMESPACE, false);

  preferences.putFloat("phSlope",
                       calibration.phSlope);

  preferences.putFloat("phOffset",
                       calibration.phOffset);

  preferences.putFloat("ecFactor",
                       calibration.ecFactor);

  preferences.putFloat("tdsFactor",
                       calibration.tdsFactor);

  preferences.putFloat("turSlope",
                       calibration.turbiditySlope);

  preferences.putFloat("turOffset",
                       calibration.turbidityOffset);

  preferences.putBool("phCal",
                      calibration.phCalibrated);

  preferences.putBool("ecCal",
                      calibration.ecCalibrated);

  preferences.putBool("turCal",
                      calibration.turbidityCalibrated);

  preferences.putBool("saved", true);

  preferences.end();

  Serial.println("Calibration saved to nonvolatile memory.");
}

void loadCalibration() {
  preferences.begin(PREFERENCES_NAMESPACE, true);

  bool calibrationExists =
      preferences.getBool("saved", false);

  if (!calibrationExists) {
    preferences.end();

    setDevelopmentCalibration();

    systemStatus.calibrationLoaded = false;

    Serial.println(
      "No physical calibration stored.");
    Serial.println(
      "Using development calibration values.");

    return;
  }

  calibration.phSlope =
      preferences.getFloat("phSlope", 1.0f);

  calibration.phOffset =
      preferences.getFloat("phOffset", 0.0f);

  calibration.ecFactor =
      preferences.getFloat("ecFactor", 1.0f);

  calibration.tdsFactor =
      preferences.getFloat("tdsFactor", 0.5f);

  calibration.turbiditySlope =
      preferences.getFloat("turSlope", 1.0f);

  calibration.turbidityOffset =
      preferences.getFloat("turOffset", 0.0f);

  calibration.phCalibrated =
      preferences.getBool("phCal", false);

  calibration.ecCalibrated =
      preferences.getBool("ecCal", false);

  calibration.turbidityCalibrated =
      preferences.getBool("turCal", false);

  preferences.end();

  systemStatus.calibrationLoaded = true;

  Serial.println(
    "Stored calibration loaded.");
}

// calibration

void setPHCalibration(float slope, float offset) {
  calibration.phSlope = slope;
  calibration.phOffset = offset;
  calibration.phCalibrated = true;

  saveCalibration();
}

void setECCalibration(float factor) {
  calibration.ecFactor = factor;
  calibration.ecCalibrated = true;

  saveCalibration();
}

void setTDSFactor(float factor) {
  calibration.tdsFactor = factor;

  saveCalibration();
}

void setTurbidityCalibration(float slope,
                             float offset) {
  calibration.turbiditySlope = slope;
  calibration.turbidityOffset = offset;
  calibration.turbidityCalibrated = true;

  saveCalibration();
}

// the measurment validation 

bool validNumber(float value) {
  return !isnan(value) && !isinf(value);
}

bool valueInRange(float value,
                  float minimum,
                  float maximum) {
  return validNumber(value) &&
         value >= minimum &&
         value <= maximum;
}

void validateMeasurements() {
  sensorData.temperatureValid =
      valueInRange(sensorData.temperature,
                   TEMP_MIN_C,
                   TEMP_MAX_C);

  sensorData.phValid =
      valueInRange(sensorData.ph,
                   PH_MIN,
                   PH_MAX);

  sensorData.ecValid =
      valueInRange(sensorData.ec,
                   EC_MIN_US_CM,
                   EC_MAX_US_CM);

  sensorData.tdsValid =
      validNumber(sensorData.tds) &&
      sensorData.tds >= 0.0f;

  sensorData.turbidityValid =
      valueInRange(sensorData.turbidity,
                   TURBIDITY_MIN_NTU,
                   TURBIDITY_MAX_NTU);

  systemStatus.allMeasurementsValid =
      sensorData.temperatureValid &&
      sensorData.phValid &&
      sensorData.ecValid &&
      sensorData.tdsValid &&
      sensorData.turbidityValid;
}


void updatePowerStatus() {
  if (SIMULATION_MODE) {

    powerStatus.supplyVoltage = 3.30f;
    powerStatus.statusAvailable = true;
    powerStatus.lowEnergy = false;
    powerStatus.measurementAllowed = true;

    return;
  }

 

  powerStatus.supplyVoltage = NAN;
  powerStatus.statusAvailable = false;
  powerStatus.lowEnergy = false;

  powerStatus.measurementAllowed = true;
}

// status report
void printWarnings() {
  if (!powerStatus.statusAvailable &&
      !SIMULATION_MODE) {
    Serial.println(
      "WARNING: Power monitoring unavailable.");
  }

  if (powerStatus.lowEnergy) {
    Serial.println(
      "WARNING: Insufficient stored energy.");
  }

  if (!sensorData.temperatureValid) {
    Serial.println(
      "WARNING: Invalid temperature measurement.");
  }

  if (!sensorData.phValid) {
    Serial.println(
      "WARNING: Invalid pH measurement.");
  }

  if (!sensorData.ecValid) {
    Serial.println(
      "WARNING: Invalid EC measurement.");
  }

  if (!sensorData.tdsValid) {
    Serial.println(
      "WARNING: Invalid TDS measurement.");
  }

  if (!sensorData.turbidityValid) {
    Serial.println(
      "WARNING: Invalid turbidity measurement.");
  }
}

// debug display
void printMeasurements() {
  Serial.println();
  Serial.println(
    "========================================");

  Serial.println(
    "               LQDguard");

  Serial.println(
    "========================================");

  if (sensorData.phValid) {
    Serial.print("pH:          ");
    Serial.println(sensorData.ph, 2);
  } else {
    Serial.println("pH:          ERROR");
  }

  if (sensorData.temperatureValid) {
    Serial.print("Temperature: ");
    Serial.print(sensorData.temperature, 1);
    Serial.println(" C");
  } else {
    Serial.println("Temperature: ERROR");
  }

  if (sensorData.ecValid) {
    Serial.print("EC:          ");
    Serial.print(sensorData.ec, 0);
    Serial.println(" uS/cm");
  } else {
    Serial.println("EC:          ERROR");
  }

  if (sensorData.tdsValid) {
    Serial.print("TDS:         ");
    Serial.print(sensorData.tds, 0);
    Serial.println(" ppm");
  } else {
    Serial.println("TDS:         ERROR");
  }

  if (sensorData.turbidityValid) {
    Serial.print("Turbidity:   ");
    Serial.print(sensorData.turbidity, 1);
    Serial.println(" NTU");
  } else {
    Serial.println("Turbidity:   ERROR");
  }

  Serial.println("----------------------------------------");

  if (powerStatus.statusAvailable) {
    Serial.print("Supply:      ");
    Serial.print(powerStatus.supplyVoltage, 2);
    Serial.println(" V");
  } else {
    Serial.println("Supply:      UNKNOWN");
  }

  Serial.print("Calibration: ");

  if (calibration.phCalibrated ||
      calibration.ecCalibrated ||
      calibration.turbidityCalibrated) {
    Serial.println("stored/partial");
  } else {
    Serial.println("development values");
  }

  Serial.print("Mode:        ");

  if (SIMULATION_MODE) {
    Serial.println("SIMULATION");
  } else {
    Serial.println("PHYSICAL HARDWARE");
  }

  Serial.println(
    "========================================");
}


void initializeDisplay() {
  
}

void updateDisplay() {

}

void performMeasurementCycle() {
  updatePowerStatus();

  if (!powerStatus.measurementAllowed) {
    Serial.println();
    Serial.println(
      "Measurement skipped: insufficient energy.");

    updateDisplay();

    return;
  }

  // aquire sensor
  acquireSensorData();

  // filter sensor
  filterSensorData();

  // calculate ph
  calculatePH();

  // calculate ec
  calculateEC();

  // apple temp
  applyPHTemperatureCompensation();
  applyECTemperatureCompensation();

  // calculate tdd
  calculateTDS();

  // calculate turbitty
  calculateTurbidity();

  
  validateMeasurements();

 
  updateDisplay();

  printMeasurements();


  printWarnings();
}

// this is the setup

void setup() {
  Serial.begin(115200);

  delay(1000);

  randomSeed(micros());

  initializeFilter(temperatureFilter);
  initializeFilter(phFilter);
  initializeFilter(ecFilter);
  initializeFilter(turbidityFilter);

  setDevelopmentCalibration();

  loadCalibration();

  initializeDisplay();

  Serial.println();
  Serial.println(
    "LQDguard firmware initialized.");

  if (SIMULATION_MODE) {
    Serial.println(
      "Pre-hardware simulation mode is active.");
  } else {
    Serial.println(
      "Physical hardware mode is active.");
  }
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - previousSampleTime >=
      SAMPLE_INTERVAL_MS) {

    previousSampleTime = currentTime;

    performMeasurementCycle();
  }
}