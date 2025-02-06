#include <iostream>
#include <string>

#include "pigeon/pigeon.h"

class intruder_alert
{
  public:

    void sirene(std::string location)
    {
      std::cout << "Alarm! Intruder in " << location << "!\n";
    }
 
    void connect(pigeon::message<>& alarm, std::string location)
    {
      pigeon.deliver(alarm, [this, location] { sirene(location); });
    }

  private:
    // Connect the lifetime of your handler code with the pigeon,
    // by making it a data member of the same class with the
    // handler code. Otherwise the pigeon tries to deliver the 
    // message into nirvana and crash.
    // One pigeon can deliver many messages.
    pigeon::pigeon pigeon;
};

int main()
{
  pigeon::message<> kitchen_alarm;
  pigeon::message<> garage_alarm;

  intruder_alert alert;

  alert.connect(kitchen_alarm, "kitchen");
  alert.connect(garage_alarm , "garage");

  kitchen_alarm.send();
  garage_alarm.send();

  return 0;
}

