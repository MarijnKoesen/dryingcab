#include <Output/Lcd.h>

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
bool lcd_is_initialized = false;

extern void lcd_init() {
  if (lcd_is_initialized == false) {
    lcd.begin(20, 4);
    lcd.backlight();
    lcd.clear();

    lcd_is_initialized = true;
  }
}
