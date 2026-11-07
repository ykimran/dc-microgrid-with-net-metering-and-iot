#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// ===== LCDs (all 16x2) =====
#define LCD1_ADDR 0x27   // Display-1: Grid status + Battery Level (%) + Battery Voltage
#define LCD2_ADDR 0x26   // Display-2: Load power (ACS-2 * 12V) + ACS-2 current + Day/Night
#define LCD3_ADDR 0x25   // Display-3: Net metering (ACS-1 * 12V)

#define LCD_COLS 16
#define LCD_ROWS 2

LiquidCrystal_I2C lcd1(LCD1_ADDR, LCD_COLS, LCD_ROWS);
LiquidCrystal_I2C lcd2(LCD2_ADDR, LCD_COLS, LCD_ROWS);
LiquidCrystal_I2C lcd3(LCD3_ADDR, LCD_COLS, LCD_ROWS);

// ===== Grid sense state =====
bool gridActive = false;   // false = grid OFF, true = grid ON

// ===== Pin assignments =====
// Relays (active LOW)
const uint8_t RELAY1_PIN = 2;   // Relay-1 (charge vs export)
const uint8_t RELAY2_PIN = 3;   // Relay-2 (grid vs battery) -> NO=battery, NC=grid
const uint8_t RELAY3_PIN = 4;   // Relay-3 (Day/Night profile)
const uint8_t RELAY4_PIN = 5;   // Relay-4 (non-essential loads)

// Grid sense (0V = grid OFF, 5V = grid ON)
const uint8_t GRID_SENSE_PIN = 6;

// Buzzer pin (active buzzer module or simple piezo)
const uint8_t BUZZER_PIN = 7;

// Sensors
const uint8_t PIN_ACS1 = A0;
const uint8_t PIN_VS   = A1;
const uint8_t PIN_ACS2 = A2;
const uint8_t PIN_LDR  = A3;

// ===== Calibration =====
const float VREF = 5.00;
const float SENSITIVITY_V_PER_A = 0.066;
float VOLTAGE_SCALE = 3.65;

const float BATTERY_V_MIN = 9.6;   // 0% SOC voltage
const float BATTERY_V_MAX = 12.0;  // 100% SOC voltage (adjust if needed)

const float NOMINAL_V = 12.0;

// ===== Sampling =====
const uint16_t CAL_SAMPLES = 1000;
const uint16_t N_SAMPLES   = 100;

const float ALPHA_I1 = 0.30;
const float ALPHA_I2 = 0.30;
const float ALPHA_V  = 0.30;

const int LDR_THRESHOLD = 500;

inline float adcToVoltage(int adc) {
  return adc * (VREF / 1023.0f);
}

float zeroOffsetV_I1 = VREF / 2.0f;
float zeroOffsetV_I2 = VREF / 2.0f;

float filtI1 = 0;
float filtI2 = 0;
float filtV  = 0;

float avgAdcVoltage(uint8_t pin, uint16_t samples) {
  unsigned long sum = 0;
  for (uint16_t i = 0; i < samples; i++) {
    sum += analogRead(pin);
  }
  return adcToVoltage(sum / (float)samples);
}

float measureCurrent1DC() {
  float v = avgAdcVoltage(PIN_ACS1, N_SAMPLES);
  return (v - zeroOffsetV_I1) / SENSITIVITY_V_PER_A;
}

float measureCurrent2DC() {
  float v = avgAdcVoltage(PIN_ACS2, N_SAMPLES);
  return (v - zeroOffsetV_I2) / SENSITIVITY_V_PER_A;
}

float measureVoltageDC() {
  float vpin = avgAdcVoltage(PIN_VS, N_SAMPLES);
  return vpin * VOLTAGE_SCALE;
}

float batteryPercent(float vBatt) {
  float pct = (vBatt - BATTERY_V_MIN) * 100.0f / (BATTERY_V_MAX - BATTERY_V_MIN);
  return constrain(pct, 0, 100);
}

void beepSingle() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(120);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepDouble() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(120);
    digitalWrite(BUZZER_PIN, LOW);
    delay(120);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(PIN_ACS1, INPUT);
  pinMode(PIN_VS,   INPUT);
  pinMode(PIN_ACS2, INPUT);
  pinMode(PIN_LDR,  INPUT);

  // Grid sense: external 0V/5V logic signal
  pinMode(GRID_SENSE_PIN, INPUT);
  // If signal can float when grid is OFF, add an external pulldown resistor to GND (e.g. 100k).

  // Relays
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Default all relays OFF (HIGH = inactive for active-LOW relays)
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);
  digitalWrite(RELAY4_PIN, HIGH);

  // LCD init
  lcd1.init(); lcd1.backlight(); lcd1.clear();
  lcd2.init(); lcd2.backlight(); lcd2.clear();
  lcd3.init(); lcd3.backlight(); lcd3.clear();

  lcd1.setCursor(0, 0); lcd1.print("Battery/Grid");
  lcd1.setCursor(0, 1); lcd1.print("Calibrating...");

  lcd2.setCursor(0, 0); lcd2.print("Load Display");
  lcd2.setCursor(0, 1); lcd2.print("Calibrating...");

  lcd3.setCursor(0, 0); lcd3.print("Net Meter");
  lcd3.setCursor(0, 1); lcd3.print("Calibrating...");

  // Zero-current calibration for ACS712 sensors
  zeroOffsetV_I1 = avgAdcVoltage(PIN_ACS1, CAL_SAMPLES);
  zeroOffsetV_I2 = avgAdcVoltage(PIN_ACS2, CAL_SAMPLES);

  delay(800);
}

void loop() {

  // ---------- GRID STATUS SENSE FROM 5V SIGNAL ----------
  // 5V on GRID_SENSE_PIN  -> gridActive = true (ONLINE)
  // 0V on GRID_SENSE_PIN  -> gridActive = false (OFFLINE)

  bool newGridActive = (digitalRead(GRID_SENSE_PIN) == HIGH);

  if (newGridActive != gridActive) {
    // State changed, update and beep
    gridActive = newGridActive;

    if (gridActive) {
      beepSingle();   // Grid ONLINE
    } else {
      beepDouble();   // Grid OFFLINE
    }
  }

  // ---------- SENSOR READS ----------
  float I1 = measureCurrent1DC();
  float I2 = measureCurrent2DC();
  float V  = measureVoltageDC();

  // Low-pass filtering
  filtI1 = ALPHA_I1 * I1 + (1 - ALPHA_I1) * filtI1;
  filtI2 = ALPHA_I2 * I2 + (1 - ALPHA_I2) * filtI2;
  filtV  = ALPHA_V  * V  + (1 - ALPHA_V)  * filtV;

  float P_main = filtV * filtI1;       // Not shown, but kept for logic if needed
  float P_load = filtI2 * NOMINAL_V;   // Power on load side
  float P_net  = filtI1 * NOMINAL_V;   // Net power based on main current

  float battPct = batteryPercent(filtV);

  int  ldrRaw = analogRead(PIN_LDR);
  bool isDay  = (ldrRaw > LDR_THRESHOLD);

  // ---------- RELAY LOGIC ----------

  // Relay-1: Charge vs export (hysteresis)
  if (battPct > 95) {
    digitalWrite(RELAY1_PIN, LOW);   // export
  } else if (battPct < 90) {
    digitalWrite(RELAY1_PIN, HIGH);  // charge
  }

  // Relay-2: Grid vs Battery (NO=battery, NC=grid)
  if (gridActive) {
    digitalWrite(RELAY2_PIN, HIGH);  // coil off -> NC -> grid
  } else {
    digitalWrite(RELAY2_PIN, LOW);   // coil on  -> NO -> battery
  }

  // Relay-3: Day/Night
  if (isDay) {
    digitalWrite(RELAY3_PIN, HIGH);  // day -> NC
  } else {
    digitalWrite(RELAY3_PIN, LOW);   // night -> NO
  }

  // Relay-4: Non-essential loads
  if (gridActive) {
    digitalWrite(RELAY4_PIN, LOW);   // enabled when grid is available
  } else {
    digitalWrite(RELAY4_PIN, HIGH);  // disabled on battery-only
  }

  // ---------- DISPLAY-1 ----------
  lcd1.setCursor(0, 0);
  lcd1.print("GRID:  ");
  lcd1.print(gridActive ? "ONLINE " : "OFFLINE");

  lcd1.setCursor(0, 1);
  char buf[9];  // 8 chars + null terminator
  sprintf(buf, "BAT:%3d%%", (int)battPct);
  lcd1.print(buf);
  lcd1.setCursor(8, 1);
  lcd1.print("|V:");
  lcd1.print(filtV, 1);
  lcd1.print("V");

  // ---------- DISPLAY-2 ----------
  lcd2.setCursor(0, 0);
  lcd2.print("LOAD:");
  lcd2.print(P_load, 1);
  lcd2.print("W");
  lcd2.setCursor(10, 0);
  lcd2.print("|MODE");

  lcd2.setCursor(0, 1);
  lcd2.print("I:");
  lcd2.print((filtI2 >= 0) ? "+" : "-");
  lcd2.print(fabs(filtI2), 2);
  lcd2.print("A  |");
  lcd2.print(isDay ? "DAY  " : "NIGHT");

  // ---------- DISPLAY-3 ----------
  lcd3.setCursor(0, 0);
  lcd3.print("Pnet:");
  lcd3.print(fabs(P_net), 1);
  lcd3.print("W   ");

  lcd3.setCursor(0, 1);
  lcd3.print("I:");
  lcd3.print((filtI1 >= 0) ? "+" : "-");
  lcd3.print(fabs(filtI1), 2);
  lcd3.print("A   ");

  // ---------- SERIAL JSON TO ESP8266 ----------
  // One JSON line per loop. ESP8266 reads & parses this.
  Serial.print(F("{\"grid\":"));      Serial.print(gridActive ? 1 : 0);
  Serial.print(F(",\"battPct\":"));   Serial.print(battPct, 1);
  Serial.print(F(",\"battV\":"));     Serial.print(filtV, 2);

  Serial.print(F(",\"Pload\":"));     Serial.print(P_load, 1);
  Serial.print(F(",\"Iload\":"));     Serial.print(filtI2, 2);
  Serial.print(F(",\"isDay\":"));     Serial.print(isDay ? 1 : 0);

  Serial.print(F(",\"Pnet\":"));      Serial.print(P_net, 1);
  Serial.print(F(",\"Inet\":"));      Serial.print(filtI1, 2);
  Serial.println("}");

  delay(250);
}
