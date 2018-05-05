#ifndef LCD_OUTPUT_H
#define LCD_OUTPUT_H

#include <Output/Output.h>
#include <Output/Lcd.h>
#include <Status.h>
#include <Settings.h>

class LcdOutput: public Output
{
  public:
    virtual void output(Status status, Settings settings);
};

#endif
