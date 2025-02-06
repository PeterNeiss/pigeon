#include "pigeon/pigeon.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <type_traits>
#include <cstring>
#include <iostream>

struct Generator 
{
  pigeon::message<void(std::span<const std::byte>)> msgNewPackage;
  void generate();
};

class PackageOne;
class PackageTwo;
class PackageThree;

struct Dispatcher: pigeon::receiver<Dispatcher>
{
  pigeon::message<void(PackageOne   const&)> msgOne;
  pigeon::message<void(PackageTwo   const&)> msgTwo;
  pigeon::message<void(PackageThree const&)> msgThree;

  void onNewPackage(std::span<std::byte const>); 
};

struct PackagePrinter: pigeon::receiver<PackagePrinter> 
{
  void onMessageOne  (PackageOne   const&);
  void onMessageTwo  (PackageTwo   const&);
  void onMessageThree(PackageThree const&);
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

enum class ePackageType:std::underlying_type_t<std::byte>{One, Two, Three, Four};

struct Package               { ePackageType Type; };
struct PackageOne:   Package { char Data[32];};
struct PackageTwo:   Package { std::uint32_t Data;};
struct PackageThree: Package { std::array<std::byte,5> Data; };

void Generator::generate()
{
  static PackageOne one     { ePackageType::One  , "Data for PackageOne" };
  static PackageTwo two     { ePackageType::Two  , 42 };
  static PackageThree three { ePackageType::Three, {} };

  // pseudorandom
  static unsigned char count{0};
  count = (count == 0) ? 1 : count * (1 + count); 
  switch(count % 3)
  {
    case 0:
    {
      msgNewPackage.send(std::as_bytes(std::span{std::addressof(one), 1}));
      break;
    }
    case 1:
    {
      msgNewPackage.send(std::as_bytes(std::span{std::addressof(two), 1}));
      ++two.Data;
      break;
    }
    case 2:
    {
      msgNewPackage.send(std::as_bytes(std::span{std::addressof(three), 1}));
      break;
    }
    default:
      throw "error";
  }
}

void Dispatcher::onNewPackage(std::span<const std::byte> s)
{
  Package package;
  std::memcpy(&package, s.data(), sizeof (Package));

  switch (package.Type)
  {
    case ePackageType::One:
    {
      PackageOne one;
      std::memcpy(&one, s.data(), sizeof (PackageOne));
      msgOne.send(one);
      break;
    }

    case ePackageType::Two:
    {
      PackageTwo two;
      std::memcpy(&two, s.data(), sizeof (PackageTwo));
      msgTwo.send(two);
      break;
    }

    case ePackageType::Three:
    {
      PackageThree three;
      std::memcpy(&three, s.data(), sizeof (PackageThree));
      msgThree.send(three);
      break;
    }

    default:
      throw "error";
  } 
}

void PackagePrinter::onMessageOne(PackageOne const& package)
{
  std::cout << "PackageOne received: " << package.Data << "\n";
}

void PackagePrinter::onMessageTwo(PackageTwo const& package)
{
  std::cout << "PackageTwo received: " << package.Data << "\n";
}

void PackagePrinter::onMessageThree(PackageThree const&)
{
  std::cout << "PackageThree received\n";
}
