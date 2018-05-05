#include <Debug.h>

#ifdef DEBUG
  bool isSetup = false;

  void connect() {
    if (isSetup == false)   {
      Serial.begin(9600);
      isSetup = true;
    }
  }
  void debug(const char* message) {
    connect();
    Serial.print(message);
  }

  void debugln(const char* message) {
    connect();
    Serial.println(message);
  }
#else
  void connect() {
  }

  void debug(const char* message) {
  }

  void debugln(const char* message) {
  }
#endif
