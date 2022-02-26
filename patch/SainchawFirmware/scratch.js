function getFreq(input, semitones = 0, cents = 0) {
  let pitch = input
  const CENT = 0.02
  const SEMITONE = 0.1666666666667
  if (semitones > 0) {
    pitch = pitch * semitones * SEMITONE
  } else if (semitones < 0) {
    pitch = -pitch / (semitones * SEMITONE)
  }

  if (cents > 0) {
    pitch = pitch * cents * CENT
  } else if (cents < 0) {
    pitch = -pitch / (cents * CENT)
  }
  console.log(
    `inpt: ${input}\nsemi: ${semitones}\ncent: ${cents}\noutp: ${pitch}\n\n`
  )
}

function f(p, s) {
  const S = 0.1666666666666666667
  if (s > 0) {
    p = p * s * S
  } else if (s < 0) {
    p = (-1 / (s * S)) * p
  }
  return p
}

function getPitch(p, s) {
  const divisor = Math.ceil(s / 12)
  console.log(divisor)
  const factor = 12 * divisor - s * divisor
  console.log(factor)
  return (p / divisor) * factor
}

console.log(getPitch(36.7, -1))
