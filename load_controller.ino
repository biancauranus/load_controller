#include <LiquidCrystal.h>
#define RELAYS 2
#define BATTERY_SENSE_PIN A1
#define PV_SENSE_PIN A2
#define DELAY_MILLIS 1000 // 1 second
#define LCD_COLUMNS 16
#define LCD_ROWS 2
#define HACK_PIN 10
#define INVERTER_SENSE_GROUND 6
#define INVERTER_SENSE_PIN 7
#define INVERTER_POWER_PIN 8
#define SOLAR_CONTACTOR_PIN 9
#define INVERTER_TIMER 120000 // 2 minutes
/* RELAYS
 * 1 - Inverter Power
 * 2 - Transfer Switch Inverter Supply
 */
/* LiFePO4 Voltages
 *  13.6 100%
 *  13.4 90%
 *  13.3 80%
 *  13.2 70%
 *  12.9 20%
 *  12.5 14%
 *  12.0 9%
 *  10.0 0%
 */
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2; // LCD PINS
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
const int vpins[RELAYS] = {INVERTER_POWER_PIN, SOLAR_CONTACTOR_PIN}; // 8 = Inverter Power, 9 = Solar Contactor
int states[RELAYS]; // 0 = OFF, 1 = ON
const double volts_per_unit = double(5) / double(1024);
const double units_per_volt = double(1024) / double(5);
const double r1 = 10000; // ohms
const double r2 = 2000; // ohms
const double ratio = r2/(r1+r2);
const double voltage_max = 5 / ratio;
const double high_threshold_voltages[RELAYS] = {13.0, 13.2};
const double low_threshold_voltages[RELAYS] = {12.0, 12.4};
double high_threshold_volts[RELAYS];
double low_threshold_volts[RELAYS];
double high_threshold_units[RELAYS];
double low_threshold_units[RELAYS];
const double pv_r1 = 10000; // ohms
const double pv_r2 = 1000; // ohms
const double pv_ratio = pv_r2/(pv_r1+pv_r2);
const double pv_voltage_max = 5 / pv_ratio;
const double high_threshold_pv_voltages[RELAYS] = {0, 15};
const double low_threshold_pv_voltages[RELAYS] = {pv_voltage_max, pv_voltage_max};
double high_threshold_pv_volts[RELAYS];
double low_threshold_pv_volts[RELAYS];
double high_threshold_pv_units[RELAYS];
double low_threshold_pv_units[RELAYS];
int high_threshold_asc_order[RELAYS]; // sorted by high_threshold_voltages then high_threshold_pv_voltages
int low_threshold_desc_order[RELAYS]; // sorted by low_threshold_voltages then low_threshold_pv_voltages

// https://www.geeksforgeeks.org/gnome-sort-a-stupid-one/
/*
 * after this function runs, indexarr[] will be a sorted list of indexes
 * into arr[] either ascending or descending according to dir
 * 
 * indexarr[] - array the same size as arr[], will be overwritten
 * arr[] - the array to be sorted
 * n - the size of arr[]
 * dir - direction (0 = asc, 1 = desc)
 */
void gnomeSort(int indexarr[], const double arr[], const double pvarr[], int n, int dir)
{
  double temp;
  // initialize indexarr
  for (int i = 0; i < n; i = i + 1) {
    indexarr[i] = i;
  }
  int index = 0;
  while (index < n) {
    if (index == 0) {
      index++;
    } else if ((dir == 0 && arr[indexarr[index]] > arr[indexarr[index - 1]]) || (dir == 1 && arr[indexarr[index]] < arr[indexarr[index - 1]])) {
      index++;
    } else if ((dir == 0 && pvarr[indexarr[index]] > pvarr[indexarr[index - 1]]) || (dir == 1 && pvarr[indexarr[index]] < pvarr[indexarr[index - 1]])) {
      index++;
    } else {
      //swap(indexarr[index], indexarr[index - 1]);
      temp = indexarr[index];
      indexarr[index] = indexarr[index - 1];
      indexarr[index - 1] = temp;
      
      index--;
    }
  }
  return;
}

void setup() {

  pinMode(HACK_PIN, OUTPUT);    // Hack to set PV voltage to max until voltage divider is built
  digitalWrite(HACK_PIN, HIGH); // Hack to set PV voltage to max until voltage divider is built

  pinMode(INVERTER_SENSE_PIN, INPUT_PULLUP); // Active LOW
  pinMode(INVERTER_SENSE_GROUND, OUTPUT);
  digitalWrite(INVERTER_SENSE_GROUND, LOW);

  // Initialize relay pins
  for (int relay = 0; relay < RELAYS; relay = relay + 1) {
    pinMode(vpins[relay], OUTPUT);
    states[relay] = LOW;
    digitalWrite(vpins[relay], states[relay]);
  }
  pinMode(BATTERY_SENSE_PIN, INPUT);
  pinMode(PV_SENSE_PIN, INPUT);

  // Initialize LCD screen
  lcd.begin(LCD_COLUMNS, LCD_ROWS);

  // Initialize serial port
  Serial.begin(9600);
  Serial.print("volts_per_unit: ");
  Serial.println(volts_per_unit);
  Serial.print("units_per_volt: ");
  Serial.println(units_per_volt);
  Serial.print("r1: ");
  Serial.println(r1);
  Serial.print("r2: ");
  Serial.println(r2);
  Serial.print("ratio: ");
  Serial.println(ratio);
  Serial.print("pv_r1: ");
  Serial.println(pv_r1);
  Serial.print("pv_r2: ");
  Serial.println(pv_r2);
  Serial.print("pv_ratio: ");
  Serial.println(pv_ratio);

  for (int x = 0; x < RELAYS; ++x) {
    high_threshold_volts[x] = high_threshold_voltages[x] * ratio;
    low_threshold_volts[x] = low_threshold_voltages[x] * ratio;
    high_threshold_units[x] = high_threshold_volts[x] * units_per_volt;
    low_threshold_units[x] = low_threshold_volts[x] * units_per_volt;
    Serial.print("x - ");
    Serial.println(x);
    Serial.print("high_threshold_voltages[x] - ");
    Serial.println(high_threshold_voltages[x]);
    Serial.print("low_threshold_voltages[x] - ");
    Serial.println(low_threshold_voltages[x]);
    Serial.print("low_threshold_volts[x] - ");
    Serial.println(low_threshold_volts[x]);
    Serial.print("high_threshold_volts[x] - ");
    Serial.println(high_threshold_volts[x]);
    Serial.print("low_threshold_units[x] - ");
    Serial.println(low_threshold_units[x]);
    Serial.print("high_threshold_units[x]: - ");
    Serial.println(high_threshold_units[x]);
  }

  for (int x = 0; x < RELAYS; ++x) {
    high_threshold_pv_volts[x] = high_threshold_pv_voltages[x] * pv_ratio;
    low_threshold_pv_volts[x] = low_threshold_pv_voltages[x] * pv_ratio;
    high_threshold_pv_units[x] = high_threshold_pv_volts[x] * units_per_volt;
    low_threshold_pv_units[x] = low_threshold_pv_volts[x] * units_per_volt;
    Serial.print("x - ");
    Serial.println(x);
    Serial.print("PV high_threshold_voltages[x] - ");
    Serial.println(high_threshold_voltages[x]);
    Serial.print("PV low_threshold_voltages[x] - ");
    Serial.println(low_threshold_voltages[x]);
    Serial.print("PV low_threshold_volts[x] - ");
    Serial.println(low_threshold_volts[x]);
    Serial.print("PV high_threshold_volts[x] - ");
    Serial.println(high_threshold_volts[x]);
    Serial.print("PV low_threshold_units[x] - ");
    Serial.println(low_threshold_units[x]);
    Serial.print("PV high_threshold_units[x]: - ");
    Serial.println(high_threshold_units[x]);
  }
  
  gnomeSort(high_threshold_asc_order, high_threshold_voltages, high_threshold_pv_voltages, RELAYS, 0);
  gnomeSort(low_threshold_desc_order, low_threshold_voltages, low_threshold_pv_voltages, RELAYS, 1);
  for (int x = 0; x < RELAYS; ++x) {
    Serial.print("x - ");
    Serial.println(x);
    Serial.print("high_threshold_voltages[high_threshold_asc_order[x]] - ");
    Serial.println(high_threshold_voltages[high_threshold_asc_order[x]]);
  }
  for (int x = 0; x < RELAYS; ++x) {
    Serial.print("x - ");
    Serial.println(x);
    Serial.print("low_threshold_voltages[low_threshold_desc_order[x]] - ");
    Serial.println(low_threshold_voltages[low_threshold_desc_order[x]]);
  }
}

void loop() {

  // Reset LCD
  lcd.begin(LCD_COLUMNS, LCD_ROWS);

  // v Get PV Voltage
  int pv_units = analogRead(PV_SENSE_PIN);
  double volts = pv_units * volts_per_unit;
  double scaledVolts = volts / pv_ratio;
  Serial.print("pv_units: ");
  Serial.println(pv_units);
  Serial.print("volts: ");
  Serial.println(volts);
  Serial.print("scaledVolts: ");
  Serial.println(scaledVolts);
  lcd.setCursor(0,0); // (x,y)
  lcd.print("PV");
  lcd.print(scaledVolts);
  lcd.print("V ");
  // ^ Get PV Voltage

  // v Get Battery Voltage
  int units = analogRead(BATTERY_SENSE_PIN);
  volts = units * volts_per_unit;
  scaledVolts = volts / ratio;
  Serial.print("units: ");
  Serial.println(units);
  Serial.print("volts: ");
  Serial.println(volts);
  Serial.print("scaledVolts: ");
  Serial.println(scaledVolts);
  lcd.setCursor(7,0); // (x,y)
  lcd.print("BAT");
  lcd.print(scaledVolts);
  lcd.print("V ");  
  // ^ Get Battery Voltage

  // v Cycle through relays displaying their state ( 0 = LOW | 1 = HIGH )
  for (int relay = 0; relay < RELAYS; relay = relay + 1) {
    /*
    lcd.setCursor(0 + (8 * relay),1);
    lcd.print(relay);
    if (states[relay] == HIGH) {
      lcd.print(" - HIGH");
    } else if (states[relay] == LOW) {
      lcd.print(" - LOW ");
    }
    */
    lcd.setCursor(relay,1);
    if (states[relay] == HIGH) {
      lcd.print("1");
    } else if (states[relay] == LOW) {
      lcd.print("0");
    }
  }
  // ^ Cycle through relays displaying their state ( LOW | HIGH )

  // v Turn relays off
  for (int relay = 0; relay < RELAYS; relay = relay + 1) {
    if (states[low_threshold_desc_order[relay]] == HIGH) {
      if (units < low_threshold_units[low_threshold_desc_order[relay]] && pv_units < low_threshold_pv_units[low_threshold_desc_order[relay]]) {
        Serial.print("Turning off relay: ");
        Serial.println(low_threshold_desc_order[relay]);
        states[low_threshold_desc_order[relay]] = LOW;
        digitalWrite(vpins[low_threshold_desc_order[relay]], states[low_threshold_desc_order[relay]]);
        goto finish;
      }
    }
  }
  // ^ Turn relays off
  // v Restart Inverter if it has failed
  int inverter_sense;
  if (states[0] == HIGH) { // If the inverter is expected to be on
    Serial.println("states[0] == HIGH");
    inverter_sense = digitalRead(INVERTER_SENSE_PIN);
    Serial.print("inverter_sense: ");
    Serial.println(inverter_sense);
    while (inverter_sense == HIGH) { // INVERTER_SENSE_PIN is active LOW
      Serial.println("inverter_sense == HIGH");
      Serial.println("Restarting Inverter");
      digitalWrite(INVERTER_POWER_PIN, LOW);
      delay(10000); // Wait 10 seconds
      digitalWrite(INVERTER_POWER_PIN, HIGH);
      delay(INVERTER_TIMER); // Wait INVERTER_TIMER
      inverter_sense = digitalRead(INVERTER_SENSE_PIN);
    }
  }
  // ^ Restart Inverter if it has failed
  // v Turn relays on
  for (int relay = 0; relay < RELAYS; relay = relay + 1) {
    if (states[high_threshold_asc_order[relay]] == LOW) {
      if (units >= high_threshold_units[high_threshold_asc_order[relay]] && pv_units >= high_threshold_pv_units[high_threshold_asc_order[relay]]) {
        Serial.print("Turning on relay: ");
        Serial.println(high_threshold_asc_order[relay]);
        states[high_threshold_asc_order[relay]] = HIGH;
        digitalWrite(vpins[high_threshold_asc_order[relay]], states[high_threshold_asc_order[relay]]);
        if (vpins[high_threshold_asc_order[relay]] == 8) { // If we just turned Inverter Power on
          delay(INVERTER_TIMER); // Wait INVERTER_TIMER
        }
        goto finish;
      }
    }
  }
  // ^ Turn relays on
  finish:
  Serial.println();
  delay(DELAY_MILLIS);
}
