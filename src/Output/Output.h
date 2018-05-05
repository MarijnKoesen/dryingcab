#ifndef GAME_MODE_GAME_OUTPUT_H
#define GAME_MODE_GAME_OUTPUT_H

#include <Status.h>
#include <Settings.h>

class Output
{
public:
  virtual void output(Status status, Settings settings);
};

#endif
