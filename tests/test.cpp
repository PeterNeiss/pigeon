#include <catch2/catch_test_macros.hpp>
#include "pigeon/pigeon.h"
#include <iostream>

static_assert(sizeof (pigeon::pigeon) == sizeof(void*), "pigeon::pigeon too big");
static_assert(sizeof (pigeon::message<>) == sizeof(void*), "pigeon::message<> too big");

static_assert(sizeof (pigeon::detail::contact) == 2 * sizeof(void*), "pigeon::detail::contact too big");
static_assert(sizeof (pigeon::detail::sender<void>) == 3 * sizeof(void*), "pigeon::detail::sender too big");

auto handler_dummy = [] { };
auto drop_dummy = [] { };
static_assert(sizeof (pigeon::detail::inbox<decltype(handler_dummy), decltype(drop_dummy), void>) == 3 * sizeof(void*), 
  "pigeon::detail::inbox too big");
static_assert(sizeof (pigeon::detail::inbox_with_allocator<decltype(handler_dummy), decltype(drop_dummy), void>) == 4 * sizeof(void*),
  "pigeon::detail::inbox_with_allocator too big");

TEST_CASE("Single Pigeon - Single Message")
{
  pigeon::pigeon pigeon;
  CHECK(pigeon.size() == 0);

  pigeon::message<> message;
  CHECK(message.size() == 0);

  std::size_t CallCounter{0};

  auto token = pigeon.deliver(message).to([&CallCounter] { ++CallCounter; });
  CHECK(pigeon.size()  == 1);
  CHECK(message.size() == 1);

  SECTION("send")
  {
    CHECK(CallCounter == 0);

    message.send();
    CHECK(CallCounter == 1);
    message.send();
    CHECK(CallCounter == 2);
  }

  SECTION("pigeon::clear-send")
  {
    CHECK(CallCounter == 0);
      message.send();
    CHECK(CallCounter == 1);
      pigeon.clear();
      message.send();
    CHECK(CallCounter == 1);
  }

  SECTION("message::clear-send")
  {
    CHECK(CallCounter == 0);
      message.send();
    CHECK(CallCounter == 1);
      message.clear();
      message.send();
    CHECK(CallCounter == 1);
  }

  SECTION("pigeon::clear-message::clear-send")
  {
      pigeon.clear();
      message.clear();
      message.send();
    CHECK(CallCounter == 0);
  }

  SECTION("message::clear-pigeon::clear-send")
  {
      message.clear();
      pigeon.clear();
      message.send();
    CHECK(CallCounter == 0);
  }

  SECTION("drop-send")
  {
    CHECK(CallCounter == 0);
      message.send();
    CHECK(CallCounter == 1);
      pigeon.drop(token);
      message.send();
    CHECK(CallCounter == 1);
  }

  SECTION("two_deliver-send")
  {
      auto token2 = pigeon.deliver(message, [&CallCounter] { ++CallCounter; });
    CHECK(pigeon.size()  == 2);
    CHECK(message.size() == 2);
    CHECK(CallCounter == 0);
      message.send();
    CHECK(CallCounter == 2);

    SECTION("drop")
    {
        pigeon.drop(token2);
      CHECK(pigeon.size()  == 1);
      CHECK(message.size() == 1);
        message.send();
      CHECK(message.size() == 1);
      CHECK(CallCounter == 3);
    }

    SECTION("two_drop")
    {
        pigeon.drop(token2);
        message.send();
      CHECK(CallCounter == 3);
        pigeon.drop(token);
        message.send();
      CHECK(CallCounter == 3);
    }

    SECTION("two_drop_same")
    {
        pigeon.drop(token2);
        message.send();
      CHECK(CallCounter == 3);
        pigeon.drop(token2);
        message.send();
      CHECK(CallCounter == 4);
    }
  }
}

TEST_CASE("deliver helper")
{
  pigeon::pigeon pigeon;
  pigeon::message<> message;

  std::size_t CallCounter{0};

  //auto token = pigeon.deliver(message, []{}, nullptr, [&CallCounter] { ++CallCounter; });
  auto token = pigeon.deliver(message).withAllocator(nullptr)
                                      .onDrop([&CallCounter] (pigeon::contact_token, pigeon::who) 
                                        { ++CallCounter; })
                                      .to([]{});

  CHECK(CallCounter == 0);
  pigeon.drop(token);
  CHECK(CallCounter == 1);
}

TEST_CASE("pigeon drop first")
{
  struct: pigeon::allocator
  {
    bool CalledAllocate{false};
    bool CalledDeAllocate{false};

    void* allocate(std::size_t size_bytes) override
    { 
      CalledAllocate = true;
      return ::operator new(size_bytes); 
    }

    void deallocate(void* pointer, std::size_t) override 
    { 
      CalledDeAllocate = true;
      ::operator delete(pointer);
    }
  } allocator;

  pigeon::pigeon pigeon;
  pigeon::message<> message;

  auto token = pigeon.deliver(message).withAllocator(&allocator)
                                      .onDrop([&message] (pigeon::contact_token token, pigeon::who) 
                                        { message.drop(token); }
                                       )
                                      .to([]{});

  CHECK(allocator.CalledAllocate   == true);
  CHECK(allocator.CalledDeAllocate == false);
  pigeon.drop(token);
  CHECK(allocator.CalledDeAllocate == true);
}

TEST_CASE("message drop first")
{
  struct: pigeon::allocator
  {
    bool CalledAllocate{false};
    bool CalledDeAllocate{false};

    void* allocate(std::size_t size_bytes) override
    { 
      CalledAllocate = true;
      return ::operator new(size_bytes); 
    }

    void deallocate(void* pointer, std::size_t) override 
    { 
      CalledDeAllocate = true;
      ::operator delete(pointer);
    }
  } allocator;

  pigeon::pigeon pigeon;
  pigeon::message<> message;

  auto token = pigeon.deliver(message).withAllocator(&allocator)
                                      .onDrop([&pigeon] (pigeon::contact_token token, pigeon::who) 
                                        { pigeon.drop(token); }
                                       )
                                      .to([]{});

  CHECK(allocator.CalledAllocate   == true);
  CHECK(allocator.CalledDeAllocate == false);
  message.drop(token);
  CHECK(allocator.CalledDeAllocate == true);
}

TEST_CASE("either drop first")
{
  struct: pigeon::allocator
  {
    bool CalledAllocate{false};
    bool CalledDeAllocate{false};

    void* allocate(std::size_t size_bytes) override
    { 
      CalledAllocate = true;
      return ::operator new(size_bytes); 
    }

    void deallocate(void* pointer, std::size_t) override 
    { 
      CalledDeAllocate = true;
      ::operator delete(pointer);
    }
  } allocator;

  pigeon::pigeon pigeon;
  pigeon::message<> message;

  auto token = pigeon.deliver(message)
    .withAllocator(&allocator)
    .onDrop(
      [&pigeon, &message] (pigeon::contact_token token, pigeon::who who) 
      { 
        switch(who)
        {
          case pigeon::who::pigeon:
            message.drop(token); 
            break;
          case pigeon::who::message:
            pigeon.drop(token); 
            break;
        }
      }
    )
    .to([]{});

  SECTION("pigeon first")
  {
    CHECK(allocator.CalledAllocate   == true);
    CHECK(allocator.CalledDeAllocate == false);
    pigeon.drop(token);
    CHECK(allocator.CalledDeAllocate == true);
  }
  SECTION("Message first")
  {
    CHECK(allocator.CalledAllocate   == true);
    CHECK(allocator.CalledDeAllocate == false);
    message.drop(token);
    CHECK(allocator.CalledDeAllocate == true);
  }
}

TEST_CASE("Single Pigeon - Multiple Messages")
{
  pigeon::pigeon pigeon;
  CHECK(pigeon.size() == 0);

  pigeon::message<> msgNotify;
  CHECK(msgNotify.size() == 0);

  pigeon::message<void(int)> msgAdd;
  CHECK(msgAdd.size() == 0);

  pigeon::message<int()> msgResult;
  CHECK(msgResult.size() == 0);

  std::size_t notifyCounter{0};
  int sum{0};

  pigeon.deliver(msgNotify, [&notifyCounter]  { ++notifyCounter; });
  pigeon.deliver(msgResult, [&sum]()          { return sum; });

  auto tkAdd = pigeon.deliver(msgAdd, [&sum](int value) { sum += value; });

  CHECK(pigeon.size()    == 3);
  CHECK(msgNotify.size() == 1);
  CHECK(msgAdd.size()    == 1);
  CHECK(msgResult.size() == 1);

  SECTION("send")
  {
    CHECK(notifyCounter == 0);
      msgNotify.send();
    CHECK(notifyCounter == 1);
      
    CHECK(sum == 0);
      auto resultChecker = [&sum](int result) { CHECK(sum == result); };
      msgResult.response(resultChecker);
      msgAdd.send(42);
      msgResult.response(resultChecker);
      msgAdd.send(18);
    CHECK(sum == 60);
  }

  SECTION("pigeon::clear")
  {
    pigeon.clear();
    CHECK(pigeon.size()    == 0);
    CHECK(msgNotify.size() == 0);
    CHECK(msgAdd.size()    == 0);
    CHECK(msgResult.size() == 0);
  }

  SECTION("drop")
  {
    CHECK(notifyCounter == 0);
      msgNotify.send();
    CHECK(notifyCounter == 1);
      
    CHECK(sum == 0);
      auto resultChecker = [&sum](int result) { CHECK(sum == result); };
      msgResult.response(resultChecker);
      msgAdd.send(42);
      msgResult.response(resultChecker);
      msgAdd.drop(tkAdd);
      msgAdd.send(18);
    CHECK(sum == 42);
    CHECK(pigeon.size()    == 2);
    CHECK(msgNotify.size() == 1);
    CHECK(msgAdd.size()    == 0);
    CHECK(msgResult.size() == 1);
  }
}

TEST_CASE("allocator")
{
  struct test_allocator: pigeon::allocator
  {
    std::size_t ReferenceCount {1};
    std::size_t AllocateCount  {0};
    std::size_t DeAllocateCount{0};
/*
    ~test_allocator() 
    {
      std::cout << "~test_allocator called\n";
    }
*/
    void* allocate(std::size_t size_bytes) override
    { 
//      std::cout << "allocate called\n";
      ++AllocateCount;
      request();
      return ::operator new(size_bytes); 
    }

    void deallocate(void* pointer, std::size_t) override 
    { 
//      std::cout << "deallocate called\n";
      ++DeAllocateCount;
      ::operator delete(pointer);
      release();
    }
 
    void request() { ++ReferenceCount; }

    void release() 
    {
      if (not --ReferenceCount)
        delete this;
    }
  };
 
  pigeon::pigeon pigeon;
  CHECK(pigeon.size() == 0);

  pigeon::message<> message;
  CHECK(message.size() == 0);

  auto allocator = new test_allocator;
  CHECK(allocator->ReferenceCount == 1);
 
  pigeon.deliver(message, [] { }, allocator);
  message.send();

  CHECK(allocator->AllocateCount == 1);
  CHECK(allocator->DeAllocateCount == 0);
  CHECK(allocator->ReferenceCount == 2);

  allocator->release(); 
}

TEST_CASE("value_state")
{
  pigeon::pigeon pigeon;

  pigeon::message<void(int const&)> message;  
  // Does not compile
  // pigeon::message<void(int&)> message;  
  pigeon::message<void(int&, pigeon::value_state&)> mgsChangable;  
}

TEST_CASE("stackmemory")
{
  pigeon::allocator_pigeon<pigeon::arena_stack_allocator<100>> pigeon;
  CHECK(pigeon.available_memory() == 100);

  pigeon::message<> message1;  
  pigeon.deliver(message1).to([]{});
  CHECK(pigeon.available_memory() == 60);

  pigeon::message<> message2;  
  pigeon.deliver(message2).to([]{});
  CHECK(pigeon.available_memory() == 20);
}
