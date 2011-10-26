Geometry pButton::minimumGeometry() {
  Font &font = this->font();
  Geometry geometry = font.geometry(button.state.text);
#ifdef __APPLE__
  return { 0, 0, geometry.width + 30, geometry.height + 12 };
#else
  return { 0, 0, geometry.width + 20, geometry.height + 12 };
#endif
}

void pButton::setText(const string &text) {
  qtButton->setText(QString::fromUtf8(text));
}

void pButton::constructor() {
  qtWidget = qtButton = new QPushButton;
  connect(qtButton, SIGNAL(released()), SLOT(onTick()));
}

void pButton::onTick() {
  if(button.onTick) button.onTick();
}
