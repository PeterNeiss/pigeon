#include <iostream>
#include <memory>
#include <cstddef>

#include "pigeon/pigeon.h"

class Ticker 
{
  public:
    template <typename S = void()> using msg = pigeon::message<S, Ticker>;

    msg<>                  msgTick;
    msg<void(std::size_t)> msgCount;

    void tick() 
    { 
      msgCount.send(++Counter);
      msgTick.send(); 
    }

  private:
    std::size_t Counter{0};
};

struct Listener: pigeon::receiver<Listener>
{
  void onTick()                   { std::cout << "onTick called\n"; }
  void onCount(std::size_t count) { std::cout << "onCount called (Count = " << count << ")\n"; }
};

int main()
{
  Ticker ticker;
  std::unique_ptr<Listener> listener{new Listener};

  listener->deliver(ticker.msgTick , &Listener::onTick);
  listener->deliver(ticker.msgCount, &Listener::onCount);
  ticker.tick();
  ticker.tick();

  listener.reset(); // destroy listener
  ticker.tick();    // pigeon does not deliver to listener after destruction, no Segmentation fault or worse
  return 0;
}
