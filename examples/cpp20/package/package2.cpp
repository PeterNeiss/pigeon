#include "pigeon/pigeon.h"

#include <cstddef>
#include <cstdint>
#include <vector>
#include <span>
#include <type_traits>
#include <cstring>
#include <iostream>
#include <variant>

class PackageOne;
class PackageTwo;
class PackageThree;

using Package = std::variant<PackageOne, PackageTwo, PackageThree>;

struct Generator 
{
  pigeon::message<pigeon::value_state(Package&&, pigeon::value_state&)> msgNewPackage;
  void generate();
};

struct Dispatcher: pigeon::receiver<Dispatcher>
{
  pigeon::message<void(PackageOne const&)> msgOne;
  pigeon::message<void(PackageTwo const&)> msgTwo;
  pigeon::message<void(PackageThree&&, pigeon::value_state&)> msgThree;

  pigeon::value_state onNewPackage(Package&&, pigeon::value_state&); 
};

struct PackagePrinter: pigeon::receiver<PackagePrinter> 
{
  void onMessageOne  (PackageOne   const&);
  void onMessageTwo  (PackageTwo   const&);
  void onMessageThree(PackageThree&&, pigeon::value_state&);
};

int main()
{
  Dispatcher dispatcher;
  Generator generator;

  dispatcher.deliver(generator.msgNewPackage, &Dispatcher::onNewPackage);

  PackagePrinter printer;
  printer.deliver(dispatcher.msgOne  , &PackagePrinter::onMessageOne);
  printer.deliver(dispatcher.msgTwo  , &PackagePrinter::onMessageTwo);
  printer.deliver(dispatcher.msgThree, &PackagePrinter::onMessageThree);

  for (auto packageCount = 0; packageCount < 100; ++packageCount)
    generator.generate();

  return 0;
}

struct PackageOne   { char Data[32];};
struct PackageTwo   { std::uint32_t Data;};
struct PackageThree { std::vector<std::byte> Data; };

void Generator::generate()
{
  // pseudorandom
  static unsigned char count{0};
  count = (count == 0) ? 1 : count * (1 + count); 
  switch(count % 3)
  {
    case 0:
    {
      Package package{PackageOne{"Data for PackageOne"}};
      pigeon::value_state state{pigeon::value_state::original};
      msgNewPackage.send(std::move(package), state);
      break;
    }
    case 1:
    {
      Package package{PackageTwo{count}};
      pigeon::value_state state{pigeon::value_state::original};
      msgNewPackage.send(std::move(package), state);
      break;
    }
    case 2:
    {
      Package package{PackageThree{std::vector<std::byte>(5000)}};
      pigeon::value_state state{pigeon::value_state::original};
      msgNewPackage.response(std::move(package), state, [](pigeon::value_state state)
        {
          if (state == pigeon::value_state::original)
            return pigeon::iteration_state::progress;
          else
            return pigeon::iteration_state::finish;
        }
      );
      break;
    }
    default:
      throw "error";
  }
}

pigeon::value_state Dispatcher::onNewPackage(Package&& package, pigeon::value_state& state)
{
  if (state == pigeon::value_state::original)
  {
    std::visit([this](auto&& arg)
    {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, PackageOne>)
        msgOne.send(arg);
      else if constexpr (std::is_same_v<T, PackageTwo>)
        msgTwo.send(arg);
      else if constexpr (std::is_same_v<T, PackageThree>)
      {
        pigeon::value_state state{pigeon::value_state::original};
        msgThree.send(std::move(arg), state);
      }
      else
          static_assert(false, "non-exhaustive visitor!");
    }, std::move(package));

    state = pigeon::value_state::moved_from;
  }
  return state;
}

void PackagePrinter::onMessageOne(PackageOne const& package)
{
  std::cout << "PackageOne received: " << package.Data << "\n";
}

void PackagePrinter::onMessageTwo(PackageTwo const& package)
{
  std::cout << "PackageTwo received: " << package.Data << "\n";
}

void PackagePrinter::onMessageThree(PackageThree&& package, pigeon::value_state& state)
{
  if (state == pigeon::value_state::original)
  {
    auto grabData{std::move(package.Data)};
    std::cout << "PackageThree received\n";
    state = pigeon::value_state::moved_from;
  }
}
