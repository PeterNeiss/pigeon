#include <iostream>
#include <memory>

#include "pigeon/pigeon.h"

struct Ticker 
{
  pigeon::message<> msgTick;
  void tick() { msgTick.send(); }
};

struct Listener
{
  pigeon::pigeon pigeon;
  void onTick() { std::cout << "onTick called\n"; }
};

int main()
{
  Ticker ticker;
  std::unique_ptr<Listener> listener{new Listener};

  listener->pigeon.deliver(ticker.msgTick, [&listener]{ listener->onTick(); });

  ticker.tick();
  ticker.tick();

  listener.reset(); // delete listener

  ticker.tick();    // No use after free, fine with pigeon
  return 0;
}
