#include <iostream>
#include <string>

#include "pigeon/pigeon.h"

class alarm
{
  public:
    alarm(std::string l):location(std::move(l)) {}

    // pigeon::message is normally public, so arbitrary classes
    // can use it easily

    // Everybody can call send() on
    // pigeon::message<void(std::string)> intruder_detected;
    // but only alarm can call send() on
    pigeon::message<void(std::string), alarm> intruder_detected;

    void detect() { intruder_detected.send(location); }

  private:
    std::string location;
};

// pigeon::receiver is a convenience class using the CRTP pattern
// that provides a pigeon protected data member and a deliver 
// member function that has an overload to deliver pigeon::message
// to member function of your class
class intruder_alert: public pigeon::receiver<intruder_alert>
{
  public:

    void sirene(std::string location)
    {
      std::cout << "Alarm! Intruder in " << location << "!\n";
    }
 
    void connect(alarm& alarm)
    {
      deliver(alarm.intruder_detected, &intruder_alert::sirene);
    }

    void show_alarm_count()
    {
      std::cout << pigeon.size() << " alarms connected \n";
    }
};

int main()
{
  intruder_alert alert;

  alarm kitchen_alarm{"kitchen"};
  alarm garage_alarm {"garage"};

  alert.connect(kitchen_alarm);
  alert.connect(garage_alarm);

  // Does compile for 
  // pigeon::message<void(std::string)>
  // but not for
  // pigeon::message<void(std::string), alarm>
  // kitchen_alarm.intruder_detected.send("garbage");

  kitchen_alarm.detect();
  garage_alarm.detect();

  return 0;
}

