#include "phoenix.hpp"
using namespace nall;
using namespace phoenix;

struct Application : Window {
  Font defaultFont;

  VerticalLayout layout;
  Canvas canvas;

  void create() {
    defaultFont.setSize(8);
    setWidgetFont(defaultFont);

    layout.setMargin(5);
    layout.append(canvas, ~0, ~0);
    append(layout);

    setTitle("phoenix");
    setGeometry({ 128, 128, 266, 266 });
    setVisible();

    onClose = &OS::quit;

  //Geometry geometry = canvas.geometry();
  //print(geometry.x, ",", geometry.y, ",", geometry.width, ",", geometry.height, "\n");

    onSize = [this]() {
      uint32_t *buffer = this->canvas.buffer();
      if(buffer == 0) return;

      Geometry geometry = this->canvas.geometry();
      unsigned width = geometry.width, height = geometry.height;
      print(width, ",", height, "\n");

      for(unsigned y = 0; y < height; y++) {
        for(unsigned x = 0; x < width; x++) {
          uint8_t pixel = 255.0 * ((double)y / (double)height * 0.5 + (double)x / (double)width * 0.5);
          *buffer++ = pixel ^ 255;
        }
      }

      this->canvas.update();
    };
  }
} application;

int main() {
  application.create();
  OS::main();
  return 0;
}
