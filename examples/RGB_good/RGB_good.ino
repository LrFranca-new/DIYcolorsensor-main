#include <DIYcolorsensor.h>

ColorSensor sensor(2, 3, 4, A0);

void setup() {
  Serial.begin(115200);
}

void loop() {
  sensor.getColor();
  sensor.rgbToHSB(sensor.R_raw, sensor.G_raw, sensor.B_raw);
  byte color = colorDetection(sensor.hue, sensor.saturation, sensor.brightness);
  
  Serial.print("Red: ");fixPrint(sensor.R_raw);Serial.print(sensor.R_raw);
  Serial.print(" |Green: ");fixPrint(sensor.G_raw);Serial.print(sensor.G_raw);
  Serial.print(" |Blue: ");fixPrint(sensor.B_raw);Serial.print(sensor.B_raw);
  Serial.print(" |color: "); Serial.print(color);
  Serial.print(" |Hue: ");fixPrint(sensor.hue);Serial.print(sensor.hue);
  Serial.print(" |Saturation: ");fixPrint(sensor.saturation);Serial.print(sensor.saturation);
  Serial.print(" |Brightness: ");fixPrint(sensor.brightness);Serial.println(sensor.brightness);

}

byte colorDetection(float h, float s, float b) {
  byte color = 0; // Default: unknown/black

  // Check if all RGB values are very low → No color
  if (sensor.R_raw <= 3 && sensor.G_raw <= 3 && sensor.B_raw <= 3) {
    color = 0; // no color
  }
  // If saturation is low → Gray or White
  else if (sensor.R_raw <= 100 && sensor.G_raw <= 100 && sensor.B_raw <= 100) {
      color = 1; //black
       }
   else if (sensor.R_raw >= 100 && sensor.G_raw >= 100 && sensor.B_raw >= 110) { 
      color = 6; // White
    }
  // High saturation → Check hue for color
  else {
    if ((h >= 285 && h <= 360) || (h >= 0 && h <= 30)) {
      color = 5; // Red
    }
    else if (h >= 30 && h <= 105) {
      color = 4; // Yellow
    }
    else if (h >= 105 && h <= 170) {
      color = 3; // Green
    }
    else if (h >= 170 && h <= 285) {
      color = 2; // Blue
    }
    else {
      color = 8; // Unknown color
    }
  }
  return color;
}


void fixPrint(int v){ //sorry it is 4am and I should be sleaping right now.

  if (v <= 9){
    Serial.print("  ");
  } 
  else if (v <= 99){
    Serial.print(" ");
  }  
}