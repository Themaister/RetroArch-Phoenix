#include "phoenix.hpp"
using namespace nall;
using namespace phoenix;

struct Application : Window {
  Font hexEditFont;
  VerticalLayout layout;
  HexEdit hexEdit;

  uint8_t buffer[544];

  void create() {
    for(unsigned n = 0; n < 544; n++) buffer[n] = n;

    hexEditFont.setFamily("Liberation Mono");
    hexEditFont.setSize(8);

    hexEdit.setFont(hexEditFont);
    hexEdit.onRead = [this](unsigned addr) { return this->buffer[addr]; };
    hexEdit.onWrite = [this](unsigned addr, uint8_t data) { this->buffer[addr] = data; };
    hexEdit.setColumns(16);
    hexEdit.setRows(16);
    hexEdit.setOffset(0);
    hexEdit.setLength(544);

    layout.setMargin(5);
    layout.append(hexEdit, ~0, ~0);
    append(layout);

    onClose = []() { OS::quit(); };

    setStatusText("Ready");
    setStatusVisible();

    setTitle("Hex Edit");
    setGeometry({ 64, 64, 485, 220 });
    setVisible();
  }
} application;

int main() {
  application.create();
  OS::main();
  return 0;
}
