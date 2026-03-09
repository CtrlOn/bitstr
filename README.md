# BitString library (bitstr.h)

## This library provides a BitString class for representing and manipulating arbitrary-precision binary floating-point numbers.

### Feature milestones:

- [x] Basic structure and constructors
- [x] String conversion (was hell of a midnight)
- [x] Bitwise shifting
- [x] Comparison
- [x] Addition and subtraction
- [x] Multiplication
- [x] Division
- [x] Hardcoded PI
- [x] Sine
- [x] Factorial
- [x] Power
- [x] Square root
- [x] Natural logarithm
- [ ] Next prime
- [x] Utils (Find, Sort, Avg)

### Optimizarion milestones:

- [ ] Karatsuba multiplication
- [ ] Portable for x64 systems (currently wastes their potential by using 32-bit limbs)
- [ ] Employ a better enumerable type for mantissa than vector (preferable for O(1) access and O(1) push_back)

### Principle:

- Works pretty much the same as double or float, but has:
  - Sign: 1 bit (bool)
  - Exponent: 64 bits (uint64_t)
  - Mantissa infinite bits (vector\<uint32_t\>)

### Precision:

- Configurable via macro definitions:
  - `#define DEC_FRAC_OUT 99` (number of decimal digits in toString, alternatively, pass it as second argument)
  - `#define BIN_FRAC_IN 340` (number of bits fromString reads, alternatively, pass it as second argument)
  - `#define DIV_PRECISION 340` (number of bits to use for division, alternatively, use **div(a,b,precision)**)
  - `#define SQRT_PRECISION 340` (number of bits to use for square root, alternatively, use **sqrt(a,precision)**, WARNING: this must be not lower than DIV_PRECISION, otherwise sqrt will not diverge causing infinite loop!)
  - `#define SIN_PRECISION 340` (number of bits to use for sine and cosine functions)
  - `#define LN_PRECISION 340` (number of bits to use for natural logarithm)

- Hardcoded constants:
  - `PI` (512 bits of precision = ~154 decimal digits)
    - `TAU`, `HALF_PI`, - derives from PI (only exponent is different)
  - `LN_2` (512 bits of precision = ~154 decimal digits)
