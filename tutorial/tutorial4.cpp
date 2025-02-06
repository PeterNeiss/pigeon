#include <iostream>
#include <string>

#include "pigeon/pigeon.h"

struct alarm
{
  alarm(std::string l):location(std::move(l)) {}
  std::string location;

  // Messages can deliver information through parameters
  pigeon::message<void(std::string)> intruder_detected;

  void detect() { intruder_detected.send(location); }
};

class intruder_alert
{
  public:

    void sirene(std::string location)
    {
      std::cout << "Alarm! Intruder in " << location << "!\n";
    }
 
    void connect(alarm& alarm)
    {
      pigeon.deliver(alarm.intruder_detected,
        [this](std::string location) { sirene(location); });
    }

    void show_alarm_count()
    {
      std::cout << pigeon.size() << " alarms connected \n";
    }

  private:
    pigeon::pigeon pigeon;
};


int main()
{
  intruder_alert alert;

  alarm kitchen_alarm{"kitchen"};
  alarm garage_alarm {"garage"};

  alert.connect(kitchen_alarm);
  alert.connect(garage_alarm);

  kitchen_alarm.detect();
  garage_alarm.detect();

  return 0;
}

