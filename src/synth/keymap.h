// QWERTY "tracker" keyboard layout for playing notes on the Cardputer.
//
//   Upper octave:   2 3   5 6 7   9 0   =         (black keys)
//                  q w e r t y u i o p [ ]        (white keys)
//   Lower octave:   s d   g h j   l ;             (black keys)
//                  z x c v b n m , . /            (white keys)
//
// z = C of the base octave, q = C one octave up. The rows overlap so you get
// about two and a half octaves without shifting.
#pragma once

namespace ps {

// Undo shift / caps lock so a key always maps to the same note.
inline char unshiftKey(char c) {
  if (c >= 'A' && c <= 'Z') return char(c - 'A' + 'a');
  switch (c) {
    case '~': return '`';  case '!': return '1';  case '@': return '2';
    case '#': return '3';  case '$': return '4';  case '%': return '5';
    case '^': return '6';  case '&': return '7';  case '*': return '8';
    case '(': return '9';  case ')': return '0';  case '_': return '-';
    case '+': return '=';  case '{': return '[';  case '}': return ']';
    case '|': return '\\'; case ':': return ';';  case '"': return '\'';
    case '<': return ',';  case '>': return '.';  case '?': return '/';
    default:  return c;
  }
}

// Semitone offset from the base C for a (unshifted) key, or -1 if the key
// isn't a note key.
inline int keyToSemitone(char c) {
  switch (c) {
    // lower row: z = 0
    case 'z': return 0;  case 's': return 1;  case 'x': return 2;
    case 'd': return 3;  case 'c': return 4;  case 'v': return 5;
    case 'g': return 6;  case 'b': return 7;  case 'h': return 8;
    case 'n': return 9;  case 'j': return 10; case 'm': return 11;
    case ',': return 12; case 'l': return 13; case '.': return 14;
    case ';': return 15; case '/': return 16;
    // upper row: q = 12
    case 'q': return 12; case '2': return 13; case 'w': return 14;
    case '3': return 15; case 'e': return 16; case 'r': return 17;
    case '5': return 18; case 't': return 19; case '6': return 20;
    case 'y': return 21; case '7': return 22; case 'u': return 23;
    case 'i': return 24; case '9': return 25; case 'o': return 26;
    case '0': return 27; case 'p': return 28; case '[': return 29;
    case '=': return 30; case ']': return 31;
    default: return -1;
  }
}

}  // namespace ps
