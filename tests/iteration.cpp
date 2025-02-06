#include <catch2/catch_test_macros.hpp>
#include "pigeon/pigeon.h"
#include <iostream>
#include <string>

TEST_CASE("Single Pigeon - Single Message")
{
  pigeon::pigeon pigeon;
  pigeon::message<> message;

  SECTION("detect forbidden method calls while iterating")
  {
    // todo
    pigeon.deliver(message, [] {; });
  }

}

TEST_CASE("single value_state")
{
  pigeon::pigeon pigeon;
  pigeon::message<void(int&, pigeon::value_state&)> message;

  int check1;
  auto modify1 = [&check1](int& value, pigeon::value_state& state)
  { 
    if (state == pigeon::value_state::original)
    {
      value = 42;
      state = pigeon::value_state::changed;
    } 
    check1 = value;
  };

  int check2;
  auto modify2 = [&check2](int& value, pigeon::value_state& state)
  { 
    if (state == pigeon::value_state::original)
    {
      value = 43;
      state = pigeon::value_state::changed;
    } 
    check2 = value;
  };

  int check3;
  auto nomodify = [&check3](int& value, pigeon::value_state&)
  { 
    check3 = value;
  };

  pigeon.deliver(message, modify1);
  pigeon.deliver(message, modify2);
  pigeon.deliver(message, nomodify);

  int value = 0;
  pigeon::value_state state = pigeon::value_state::original;
  message.response(value, state, [&state]
    {
      if (state == pigeon::value_state::changed)
      {
        state = pigeon::value_state::constant;
        return pigeon::iteration_state::repeat;
      }
      else
        return pigeon::iteration_state::progress;
    }
  );       

  CHECK(value == 43);
  CHECK(value == check1);
  CHECK(value == check2);
  CHECK(value == check3);
}

TEST_CASE("multiple value_state")
{
  pigeon::pigeon pigeon;
  pigeon::message<void(int&, pigeon::value_state&, std::string&, pigeon::value_state&)> message;

  int checkInt1;
  std::string checkString1;
  auto modify1 = [&checkInt1, &checkString1] 
  (
    int& valueInt, pigeon::value_state& stateInt,
    std::string& valueString, pigeon::value_state& stateString
  )
  { 
    if (stateInt == pigeon::value_state::original)
    {
      valueInt = 42;
      stateInt = pigeon::value_state::changed;
    } 

    if (stateString == pigeon::value_state::original)
    {
      valueString = "Modified by modify1";
      stateString = pigeon::value_state::changed;
    } 

    checkInt1 = valueInt;
    checkString1 = valueString;
  };

  int checkInt2;
  std::string checkString2;
  auto modify2 = [&checkInt2, &checkString2] 
  (
    int& valueInt, pigeon::value_state& stateInt,
    std::string& valueString, pigeon::value_state&
  )
  { 
    if (stateInt == pigeon::value_state::original)
    {
      valueInt = 43;
      stateInt = pigeon::value_state::changed;
    } 

    checkInt2 = valueInt;
    checkString2 = valueString;
  };


  int checkInt3;
  std::string checkString3;
  auto modify3 = [&checkInt3, &checkString3] 
  (
    int& valueInt, pigeon::value_state&,
    std::string& valueString, pigeon::value_state&
  )
  { 
    checkInt3 = valueInt;
    checkString3 = valueString;
  };

  pigeon.deliver(message, modify1);
  pigeon.deliver(message, modify2);
  pigeon.deliver(message, modify3);

  int valueInt = 0;
  std::string valueString;
  pigeon::value_state stateInt = pigeon::value_state::original;
  pigeon::value_state stateString = pigeon::value_state::original;

  message.response(valueInt, stateInt, valueString, stateString, [&stateInt, &stateString]
    {
      pigeon::iteration_state state{pigeon::iteration_state::progress};

      if (stateInt == pigeon::value_state::changed)
      {
        stateInt = pigeon::value_state::constant;
        state = pigeon::iteration_state::repeat;
      }

      if (stateString == pigeon::value_state::changed)
      {
        stateString = pigeon::value_state::constant;
        state = pigeon::iteration_state::repeat;
      }

      return state;
    }
  );       

  CHECK(valueInt == 43);
  CHECK(valueInt == checkInt1);
  CHECK(valueInt == checkInt2);
  CHECK(valueInt == checkInt3);

  CHECK(valueString == "Modified by modify1");
  CHECK(valueString == checkString1);
  CHECK(valueString == checkString2);
  CHECK(valueString == checkString3);
}

TEST_CASE("change during iteration")
{
  pigeon::pigeon pigeon;

  class Modifiable
  {
    public:
      pigeon::message<void(Modifiable&, pigeon::value_state&)> OnChange;

      int get() { return Value; }
      void set(int value) 
      { 
        Value = value;  
        pigeon::value_state state;
        OnChange.send(*this, state);
      }

    private:
      int Value;
  };

  Modifiable obj;

  CHECK_FALSE(obj.OnChange.isSending());

  pigeon::contact_token token = pigeon.deliver(obj.OnChange, 
    [&] (Modifiable& mod, pigeon::value_state& state) 
    {
      CHECK(mod.OnChange.isSending());
      CHECK(mod.OnChange.size() != 0);
      CHECK_THROWS_AS(mod.OnChange.clear(), std::logic_error);
      CHECK_THROWS_AS(mod.OnChange.drop(token), std::logic_error);

      mod.set(42);  // Does not recurse!
      state = pigeon::value_state::changed;
    }
  );

  CHECK_FALSE(obj.OnChange.isSending());
  obj.set(1);
  CHECK_FALSE(obj.OnChange.isSending());
}

