#include <VAGGearSelector.h>

VAGGearSelector gearSelector(NULL,6);

void setup() {
  gearSelector.begin();
//  gearSelector.setTransmitionType(5);
//  gearSelector.setGear(1);
Serial.begin(115200); 
}

void loop() {
  if (gearSelector.gearChanged()){
     //Serial.print("trans type: ");Serial.println(gearSelector.getTransmitionType());
     //Serial.print("trans gear: ");Serial.println(gearSelector.getGear());
     Serial.println(gearSelector.decodeSelectedGear());
     gearSelector.clearGearChangedFlag();
  }
}
