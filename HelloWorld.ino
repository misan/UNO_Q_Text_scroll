// ---- UNO Q LED Matrix text scroller (8x13), using only matrixBegin + matrixWrite ----
// Resolution: 8 rows x 13 columns (row 0 = top, col 0 = left)

extern "C" void matrixBegin();
extern "C" void matrixWrite(const uint32_t* buf);

constexpr int ROWS = 8;
constexpr int COLS = 13;
constexpr int NUM_PIX = ROWS * COLS;     // 104 LEDs
constexpr uint16_t ROW_MASK = (1u << COLS) - 1;  // 13-bit mask

// Display state stored per-row as a 13-bit shift register (bit 12 = leftmost, bit 0 = rightmost)
static uint16_t rowsBits[ROWS] = {0};

// Buffer sent to the matrix (4 * 32 = 128 bits)
static uint32_t buf[4] = {0, 0, 0, 0};

// ------------------- 5x7 font (column-wise, LSB = top row) -------------------
// Each glyph is up to 5 columns wide, 7 rows tall (fits the 8-row display).
// For each column byte: bit0->row0 (top), ..., bit6->row6. Row7 is always off here.
// Space between characters = 1 blank column.
struct Glyph { uint8_t w; uint8_t col[5]; };

static const Glyph FONT_5x7[] = {
  // ASCII 32..90 (space to 'Z'); undefined chars become space
  /* 32 ' ' */ {3, {0x00,0x00,0x00,0x00,0x00}},
  /* 33 '!' */ {1, {0x5F,0x00,0x00,0x00,0x00}},
  /* 34 '\"'*/ {3, {0x03,0x00,0x03,0x00,0x00}},
  /* 35 '#' */ {5, {0x14,0x7F,0x14,0x7F,0x14}},
  /* 36 '$' */ {5, {0x24,0x2A,0x7F,0x2A,0x12}},
  /* 37 '%' */ {5, {0x23,0x13,0x08,0x64,0x62}},
  /* 38 '&' */ {5, {0x36,0x49,0x55,0x22,0x50}},
  /* 39 '\''*/ {1, {0x03,0x00,0x00,0x00,0x00}},
  /* 40 '(' */ {3, {0x1C,0x22,0x41,0x00,0x00}},
  /* 41 ')' */ {3, {0x41,0x22,0x1C,0x00,0x00}},
  /* 42 '*' */ {5, {0x14,0x08,0x3E,0x08,0x14}},
  /* 43 '+' */ {5, {0x08,0x08,0x3E,0x08,0x08}},
  /* 44 ',' */ {2, {0x40,0x20,0x00,0x00,0x00}},
  /* 45 '-' */ {5, {0x08,0x08,0x08,0x08,0x08}},
  /* 46 '.' */ {1, {0x40,0x00,0x00,0x00,0x00}},
  /* 47 '/' */ {5, {0x20,0x10,0x08,0x04,0x02}},
  /* 48 '0' */ {5, {0x3E,0x51,0x49,0x45,0x3E}},
  /* 49 '1' */ {3, {0x42,0x7F,0x40,0x00,0x00}},
  /* 50 '2' */ {5, {0x62,0x51,0x49,0x49,0x46}},
  /* 51 '3' */ {5, {0x22,0x49,0x49,0x49,0x36}},
  /* 52 '4' */ {5, {0x18,0x14,0x12,0x7F,0x10}},
  /* 53 '5' */ {5, {0x2F,0x49,0x49,0x49,0x31}},
  /* 54 '6' */ {5, {0x3E,0x49,0x49,0x49,0x32}},
  /* 55 '7' */ {5, {0x01,0x71,0x09,0x05,0x03}},
  /* 56 '8' */ {5, {0x36,0x49,0x49,0x49,0x36}},
  /* 57 '9' */ {5, {0x26,0x49,0x49,0x49,0x3E}},
  /* 58 ':' */ {1, {0x14,0x00,0x00,0x00,0x00}},
  /* 59 ';' */ {2, {0x40,0x34,0x00,0x00,0x00}},
  /* 60 '<' */ {4, {0x08,0x14,0x22,0x41,0x00}},
  /* 61 '=' */ {5, {0x14,0x14,0x14,0x14,0x14}},
  /* 62 '>' */ {4, {0x41,0x22,0x14,0x08,0x00}},
  /* 63 '?' */ {5, {0x02,0x01,0x59,0x09,0x06}},
  /* 64 '@' */ {5, {0x3E,0x41,0x5D,0x55,0x1E}},
  /* 65 'A' */ {5, {0x7E,0x11,0x11,0x11,0x7E}},
  /* 66 'B' */ {5, {0x7F,0x49,0x49,0x49,0x36}},
  /* 67 'C' */ {5, {0x3E,0x41,0x41,0x41,0x22}},
  /* 68 'D' */ {5, {0x7F,0x41,0x41,0x22,0x1C}},
  /* 69 'E' */ {5, {0x7F,0x49,0x49,0x49,0x41}},
  /* 70 'F' */ {5, {0x7F,0x09,0x09,0x09,0x01}},
  /* 71 'G' */ {5, {0x3E,0x41,0x49,0x49,0x7A}},
  /* 72 'H' */ {5, {0x7F,0x08,0x08,0x08,0x7F}},
  /* 73 'I' */ {3, {0x41,0x7F,0x41,0x00,0x00}},
  /* 74 'J' */ {5, {0x20,0x40,0x41,0x3F,0x01}},
  /* 75 'K' */ {5, {0x7F,0x08,0x14,0x22,0x41}},
  /* 76 'L' */ {5, {0x7F,0x40,0x40,0x40,0x40}},
  /* 77 'M' */ {5, {0x7F,0x02,0x0C,0x02,0x7F}},
  /* 78 'N' */ {5, {0x7F,0x04,0x08,0x10,0x7F}},
  /* 79 'O' */ {5, {0x3E,0x41,0x41,0x41,0x3E}},
  /* 80 'P' */ {5, {0x7F,0x09,0x09,0x09,0x06}},
  /* 81 'Q' */ {5, {0x3E,0x41,0x51,0x21,0x5E}},
  /* 82 'R' */ {5, {0x7F,0x09,0x19,0x29,0x46}},
  /* 83 'S' */ {5, {0x26,0x49,0x49,0x49,0x32}},
  /* 84 'T' */ {5, {0x01,0x01,0x7F,0x01,0x01}},
  /* 85 'U' */ {5, {0x3F,0x40,0x40,0x40,0x3F}},
  /* 86 'V' */ {5, {0x1F,0x20,0x40,0x20,0x1F}},
  /* 87 'W' */ {5, {0x7F,0x20,0x18,0x20,0x7F}},
  /* 88 'X' */ {5, {0x63,0x14,0x08,0x14,0x63}},
  /* 89 'Y' */ {5, {0x07,0x08,0x70,0x08,0x07}},
  /* 90 'Z' */ {5, {0x61,0x51,0x49,0x45,0x43}},
};

static inline const Glyph& glyphFor(char c) {
  uint8_t uc = (uint8_t)c;
  if (uc < 32 || uc > 90) uc = 32; // unsupported -> space
  return FONT_5x7[uc - 32];
}

// ------------------- helpers -------------------

// Shift the whole display left by one column and insert one new column at the right.
// colBits: bit0=top row pixel, bit6=row6. (row7 stays 0 for this 5x7 font, but you can use it)
static void pushColumn(uint8_t colBits) {
  for (int r = 0; r < ROWS; r++) {
    uint16_t bit = (colBits >> r) & 0x1;
    rowsBits[r] = uint16_t(((rowsBits[r] << 1) | bit) & ROW_MASK);
  }
}

// Convert rowsBits[] (our 8×13 state) into 4×uint32 buffer bits, row-major, left→right, top→bottom.
// NOTE: If your hardware expects a different packing, adapt ONLY this function.
static void updateBufFromRows() {
  buf[0] = buf[1] = buf[2] = buf[3] = 0;
  int idx = 0;
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      int on = (rowsBits[r] >> (COLS - 1 - c)) & 1; // (leftmost is bit 12)
      if (on) {
        int w = idx / 32;
        int b = idx % 32;
        buf[w] |= (1UL << b);
      }
      idx++;
    }
  }
}

// Push N blank columns (for leading/trailing spacing)
static void pushBlanks(int n, int speed_ms) {
  for (int i = 0; i < n; i++) {
    pushColumn(0x00);
    updateBufFromRows();
    matrixWrite(buf);
    delay(speed_ms);
  }
}

static void scrollText(const char* msg, int speed_ms, int space_cols = 1) {
  // lead-in blanks so text enters from the right
  pushBlanks(COLS, speed_ms);

  for (const char* p = msg; *p; ++p) {
    const Glyph& g = glyphFor(*p);
    for (int i = 0; i < g.w; i++) {
      pushColumn(g.col[i]);
      updateBufFromRows();
      matrixWrite(buf);
      delay(speed_ms);
    }
    for (int s = 0; s < space_cols; s++) {
      pushColumn(0x00);
      updateBufFromRows();
      matrixWrite(buf);
      delay(speed_ms);
    }
  }

  // trail-out blanks so text fully exits left side
  pushBlanks(COLS, speed_ms);
}

// ------------------- Arduino entry points -------------------

void setup() {
  matrixBegin();

  // Clear display state
  for (int r = 0; r < ROWS; r++) rowsBits[r] = 0;
  updateBufFromRows();
  matrixWrite(buf);

  // Your message here (ALL CAPS recommended with this minimal font)
  const char* message = "HELLO, WORLD!";
  const int speed_ms = 60;    // smaller = faster scroll
  const int spacing  = 1;     // blank columns between characters

  scrollText(message, speed_ms, spacing);

  // Optionally loop forever:
  // while (true) { scrollText(message, speed_ms, spacing); }
}

void loop() {
  // intentionally empty; we ran once in setup()
}
