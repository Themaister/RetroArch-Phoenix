#include "phoenix.hpp"
using namespace nall;
using namespace phoenix;

struct Application : Window {
  HorizontalLayout layout0;
  ListView list;

  HorizontalLayout layout1;
  Widget widget1;
  VerticalLayout layout1a;
  Button buttonA;

  HorizontalLayout layout2;
  Widget widget2;
  VerticalLayout layout2a;
  Button buttonB;

  void create() {
    list.append("Page 1");
    list.append("Page 2");

    buttonA.setText("Hello");
    buttonB.setText("Goodbye");

    layout0.setMargin(5);
    layout0.append(list, 200, 0, 5);

    layout1.setMargin(5);
    layout1.append(widget1, 200, 0, 5);
    layout1a.append(buttonA, 0, 0);
    layout1.append(layout1a, 0, 30, 5);

    layout2.setMargin(5);
    layout2.append(widget2, 200, 0, 5);
    layout2a.append(buttonB, 0, 0);
    layout2.append(layout2a, 0, 30, 5);

    append(layout0);
    append(layout1);
    append(layout2);

    onClose = []() { OS::quit(); };

    list.onChange = [this]() {
      if(this->list.selection()() == 0) {
        this->layout1.setVisible(true);
        this->layout2.setVisible(false);
      } else {
        this->layout1.setVisible(false);
        this->layout2.setVisible(true);
      }
    };
    list.setSelection(0);
    list.onChange();

    setTitle("Layout List");
    setGeometry({ 64, 64, 640, 480 });
    setVisible();
  }
} application;

int main() {
  application.create();
  OS::main();
  return 0;
}
