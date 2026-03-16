# BitString library (bitstr.h)

### This library provides a BitString class for representing and manipulating arbitrary-precision binary floating-point numbers.

## Feature milestones

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
- [x] Next prime
- [x] Utils (Find, Sort, Avg)

## Optimization milestones

- [ ] Karatsuba multiplication
- [x] Portable for x64 systems (currently wastes their potential by using 32-bit limbs)
- [x] Knuth's division algorithm

## Principle

- Works pretty much the same as double or float, but has:
  - Sign: 1 bit (bool)
  - Exponent: 64 bits (uint64_t)
  - Mantissa infinite bits (vector\<uint32_t\>)

## Precision

### Config

- Configurable via macro definitions:
  - `#define DEC_FRAC_OUT 100` (number of decimal digits in toString, alternatively, pass it as second argument)
  - `#define BIN_FRAC_IN 384` (number of bits fromString reads, alternatively, pass it as second argument)
  - `#define DIV_PRECISION 384` (number of bits to use for division, alternatively, use **div(a,b,precision)**)
  - `#define SQRT_PRECISION 384` (number of bits to use for square root, alternatively, use **sqrt(a,precision)**)
  - `#define SIN_PRECISION 384` (number of bits to use for sine and cosine functions)
  - `#define LN_PRECISION 384` (number of bits to use for natural logarithm)

- Hardcoded constants: (200 decimal digits of precision)
  - trigonometry: `PI`, `TWO_PI`, `HALF_PI`
  - logarithm: `LN_2`, `SQRT_2`, `INV_SQRT_2`

- `double` to `string` conversion assumes human input in decimal (double's `0.1` will be converted to `"0.1"` instead of `"0.1000...55511..."`)

### Uncertainty

*Uncertainty propagation applies here as well*

| Operation         | Formula     | Uncertainty Rule             |
| ----------------- | ----------- | ---------------------------- |
| Addition          | z = x + y   | Δz = Δx + Δy                 |
| Subtraction       | z = x − y   | Δz = Δx + Δy                 |
| Multiplication    | z = x * y   | Δz / z = (Δx / x) + (Δy / y) |
| Division          | z = x / y   | Δz / z = (Δx / x) + (Δy / y) |
| Power             | z = x^n     | Δz / z = \|n\| * (Δx / x)    |
| Root              | z = x^(1/n) | Δz / z = (1/n) * (Δx / x)    |
| Constant multiply | z = a * x   | Δz = \|a\| * Δx              |
| Natural log       | z = ln(x)   | Δz = Δx / x                  |
| Exponential       | z = e^x     | Δz / z = Δx                  |
