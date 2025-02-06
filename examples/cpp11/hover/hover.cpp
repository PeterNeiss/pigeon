#include <iostream>
#include <memory>
#include <cstddef>
#include <string>
#include <vector>

#include "pigeon/pigeon.h"

struct Position
{
  int x;
  int y;
};

struct Hoverable
{
  virtual ~Hoverable() = default;
  virtual std::string getName() const = 0;
};

struct Mouse 
{
  template <typename S = void()> using msg = pigeon::message<S, Mouse>;

  msg<Hoverable*(Position)> msgMove;

  void moveTo(Position position) 
  { 
    std::cout << "Mouse position = {" << position.x << "," << position.y << "}\n";
    msgMove.response(position,
      [](Hoverable* hover)
      { 
        if (hover) 
          std::cout << "Mouse in " << hover->getName() << "\n"; 
      }
    );
  }
};

class Rectangle: public Hoverable, public pigeon::receiver<Rectangle>
{
  public:
    Rectangle(std::string name, Position lowerLeft, Position upperRight)
     :Name(name), LowerLeft(lowerLeft), UpperRight(upperRight)
    { }

    std::string getName() const override { return Name; }

    Hoverable* onMove(Position position)
    { 
      if ((LowerLeft.x <= position.x) && (position.x <= UpperRight.x) && 
          (LowerLeft.y <= position.y) && (position.y <= UpperRight.y))
        return this;
      else
        return nullptr;
    }

  private:
    std::string Name;
    Position LowerLeft; 
    Position UpperRight; 
};

int main()
{
  Mouse mouse;
  std::vector<Rectangle> rectangles;
  rectangles.emplace_back(Rectangle{"A", {10,10},{20,20}});
  rectangles.emplace_back(Rectangle{"B", {10,10},{30,30}});
  rectangles.emplace_back(Rectangle{"C", { 0, 0},{30,30}});

  for(auto& rectangle: rectangles)
    rectangle.deliver(mouse.msgMove, &Rectangle::onMove);

  mouse.moveTo({15,15});
  mouse.moveTo({5,15});
  mouse.moveTo({25,15});
  return 0;
}

