#include <iostream>

// pigeon is a single Header C++11 library
// Make sure the header file is found by the compiler
#include "pigeon/pigeon.h"

int main()
{
  // The pigeon libarary provides the ability to define message objects 
  pigeon::message<> alarm;

  // Use the message object to send alarms.
  // The message receivers are decoupled from the message senders

  // Nothing happens here, because nobody receives the message
  alarm.send(); 

  // Use a pigeon to deliver the message to your code 
  pigeon::pigeon pigeon;
  pigeon.deliver(alarm, [] { std::cout << "Alarm!\n"; });

  // Now the alarm calls the handler
  std::cout << "Sending an alarm: ";
  alarm.send();
  return 0;
}

