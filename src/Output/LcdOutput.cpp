#include "LcdOutput.h"

void LcdOutput::output(Status status, Settings settings)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Temp: ");// + status.temperature);
  lcd.setCursor(6, 0);
  lcd.print(status.temperature);

  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.setCursor(10, 1);
  lcd.print(status.humidity);

  lcd.setCursor(0, 2);
  lcd.print("Set Temp: ");// + settings.minTemp + " - " + settings.maxTemp);
  lcd.setCursor(10, 2);
  char buf[9];
  sprintf(buf, "%d - %d", settings.minTemp, settings.maxTemp);
  lcd.print(buf);

  lcd.setCursor(0,3);
  lcd.print("Set Humd: ");// + settings.minHumidity + " - " + settings.maxHumidity);
  lcd.setCursor(10, 3);
  sprintf(buf, "%d - %d", settings.minHumidity, settings.maxHumidity);
  lcd.print(buf);
}
