#ifdef __unix__
#define LUA_USE_POSIX 1
#endif
#include <sol/sol.hpp>

#include <iostream>

struct Test {
  int value{};
  sol::protected_function func;
  Test(int value) : value(value) {}
  // ~Test() { std::cout << "deleted" << std::endl; } // uncomment this to confirm it's deleted
};

int main() {
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  lua.new_usertype<Test>("Test",
    sol::factories([]() {
      return Test(0); // value test
      // return std::make_unique<Test>(0); // unique test
      // return std::make_shared<Test>(0); // new shared_ptr test
    }),
    "set", [](Test& t, int value) { t.value = value; },
    "get", [](Test& t) { return t.value; },
    "func", &Test::func
  );

  lua.script_file("test.lua");

  lua.collect_garbage();
  lua.collect_garbage();
  std::cout << "total usage after script executed and lua.collect_garbage(): " << lua.memory_used() << std::endl;
  return 0;
}