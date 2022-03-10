#include "Pitch.h"
#include "daisysp.h"

using namespace daisysp;

const float Pitch::note[] = {16.35f,
                             17.32f,
                             18.35f,
                             19.45f,
                             20.60f,
                             21.83f,
                             23.12f,
                             24.50f,
                             25.96f,
                             27.50f,
                             29.14f,
                             30.8f};

float Pitch::SetByVoltage(float voltage) {
  this->hz = powf(2.f, voltage) * 32.7f;
  return this->hz;
}

float Pitch::SetByNote(u_int8_t note) {
  size_t baseNote = note % 12;
  int    octave   = note < 12 ? 1 : (note / (int)note) + 1;
  this->hz        = (float)Pitch::note[baseNote] * (float)octave;
  return this->hz;
}

float Pitch::calcTransposition(float semitones) {
  return this->hz * powf(2.f, semitones * kOneTwelfth);
}

float Pitch::TransposeBySemitones(float semitones) {
  this->hz = this->calcTransposition(semitones);
  return this->hz;
}

float Pitch::TransposeByCents(float cents) {
  this->hz *= powf(2.f, cents * 0.01f * kOneTwelfth);
  return this->hz;
}

float Pitch::Transpose(float semitones, float cents) {
  this->TransposeBySemitones(semitones);
  this->TransposeByCents(cents);
  return this->hz;
}

float Pitch::MinorSecond() { return this->calcTransposition(1); }
float Pitch::MajorSecond() { return this->calcTransposition(2); }
float Pitch::MinorThird() { return this->calcTransposition(3); }
float Pitch::MajorThird() { return this->calcTransposition(4); }
float Pitch::PerfectFourth() { return this->calcTransposition(5); }
float Pitch::DiminishedFifth() { return this->calcTransposition(6); }
float Pitch::AugmentedFourth() { return this->calcTransposition(6); }
float Pitch::PerfectFifth() { return this->calcTransposition(7); }
float Pitch::MinorSixth() { return this->calcTransposition(8); }
float Pitch::MajorSixth() { return this->calcTransposition(9); }
float Pitch::MinorSeventh() { return this->calcTransposition(10); }
float Pitch::MajorSeventh() { return this->calcTransposition(11); }
float Pitch::octave() { return this->hz * 2.f; }