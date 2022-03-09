#pragma once
#include <stdint.h>

class Pitch {
public:
  Pitch() {}
  ~Pitch() {}

  const static float note[];

  float hz = 16.35f;

  float SetByVoltage(float voltage);
  float SetByNote(uint8_t note);
  float calcTransposition(float semitones);
  float TransposeBySemitones(float semitones);
  float TransposeByCents(float cents);
  float Transpose(float semitones, float cents);

  float MinorSecond();
  float MajorSecond();
  float MinorThird();
  float MajorThird();
  float PerfectFourth();
  float DiminishedFifth();
  float AugmentedFourth();
  float PerfectFifth();
  float MinorSixth();
  float MajorSixth();
  float MinorSeventh();
  float MajorSeventh();
  float octave();
};