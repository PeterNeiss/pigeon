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

    void show_alarm_count()
    {
      std::cout << pigeon.size() << " alarms connected \n";
    }

  private:
    pigeon::pigeon pigeon;
};

int main()
{
  pigeon::message<> kitchen_alarm;

  {
    intruder_alert alert;
    alert.connect(kitchen_alarm, "kitchen");
    alert.show_alarm_count();

    {
      pigeon::message<> garage_alarm;
      alert.connect(garage_alarm , "garage");
      alert.show_alarm_count();
      kitchen_alarm.send();
      garage_alarm.send();
    } // garage_alarm lifetime ends here, no disconnect required
  
    alert.show_alarm_count();
    kitchen_alarm.send();
  } // lifetime of alert ended here

  // The pigeon of alert is gone, this is safe and 
  // no crash happens, but no alarm get delivered 
  kitchen_alarm.send();

  return 0;
}

