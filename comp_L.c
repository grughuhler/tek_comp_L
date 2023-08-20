/* comp_L    (gcc -o comp_L comp_L.c -lm)
 *
 * This program is based on concepts from a Tektronix application note:
 *
 *      Capacitance and Inductance Measurements Using an Oscilloscope
 *      and a Function Generator
 *
 *      https://tinyurl.com/4svu9spp (or just google for it)
 * 
 * The idea is to measure an unknown capacitor or inductor using a
 * known reference resistor, a funtion or signal generator, and an
 * oscilloscope.  Three measurements are needed: amplitudes of Vin,
 * and Vdut, and the time between zero crossings of Vdut and Vin.
 *
 * Test setup: Connect output of a signal generator to
 *   1. Reference resistor (1K is a good starting point)
 *   2. Device under test (DUT), a capacitor or inductor
 * all in series to GND.
 * 
 *                        Vin        Vdut
 *                         |          |
 *  signal generator out ----- Rref ----- DUT ----- GND.
 *
 *  Usage:
 *   1.  Select a frequency for test signal, a sine wave with no offset.
 *   2.  Use oscillicope to measure the time between rising 0 crossings
 *       of Vdut and Vin.  Delta T will be negative for capacitors and
 *       positive for inductors.
 *   3.  Measure the amplitudes of Vin and Vdut. Peak to peak is fine.
 *
 * Then use this program for example to measure a 1 mH inductor using
 * a 1 kHz test frequency and a 327.8 Ohm reference resistor with
 * measurements delta T = 217 uSec, Vin = 8.81 V, and Vdut = 0.17827 V.
 *
 *    % ./comp_L -r 327.8 1e3 217e-6 8.81 0.17827
 *    Inputs:
 *      Rref: 327.8 Ohms
 *      freq: 1.000 kHz
 *      delta_t: 217.0 uSec
 *      V_in: 8.810 V
 *      V_dut: 178.3 mV
 *    Outputs:
 *      theta: 1.363451 rad (78.120000 deg)
 *      phi: 1.383333 rad (79.259141 deg)
 *      Z: 6.659 Ohms
 *      Ls: 1.041 mH
 *      Lp: 1.079 mH
 *      Rs (Resr): 1.241 Ohms
 *      Rp: 35.73 Ohms
 *      X: 6.543 Ohms
 *      Q: 5.271741
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

/* eng funtion derived from from https://jkorpela.fi/c/eng.html and Mr. Hoerl */

#define PREFIX_START (-24)
/* Smallest power of then for which there is a prefix defined.
   If the set of prefixes will be extended, change this constant
   and update the table "prefix". */

#define PREFIX_END (PREFIX_START+(int)((sizeof(prefix)/sizeof(char *)-1)*3))

static const char *prefix[] = {
    "y", "z", "a", "f", "p", "n", "u", "m", "",
    "k", "M", "G", "T", "P", "E", "Z", "Y"
  }; 


/* eng
 *
 * Format "value" in engineering notation using "digits" signicant digits.
 * Store result in string "result", not writing more than rlen characters.
 * If numeric, use conventional exponents else use suffixes like "u"
 *
 * Be careful calling this from printf.  Each call would need its own result
 * storage */

static char *eng(double value, int digits, int numeric, char *result, char rlen)
{
  double display, fract;
  int expof10;
  char *sign;

  assert(isnormal(value)); // could also return NULL

  if (value < 0.0) {
    sign = "-";
    value = -value;
  } else {
    sign = "";
  }

  // correctly round to desired precision
  expof10 = lrint( floor( log10(value) ) );
  value *= pow(10.0, digits - 1 - expof10);

  fract = modf(value, &display);
  if(fract >= 0.5) display += 1.0;

  value = display * pow(10.0, expof10 - digits + 1);


  if(expof10 > 0)
    expof10 = (expof10/3)*3;
  else
    expof10 = ((-expof10+3)/3)*(-3);

  value *= pow(10.0, -expof10);
  if (value >= 1000.0) {
    value /= 1000.0;
    expof10 += 3;
  }
  else if(value >= 100.0)
    digits -= 2;
  else if(value >= 10.0)
    digits -= 1;

  if(numeric || (expof10 < PREFIX_START) || (expof10 > PREFIX_END))
    snprintf(result, rlen, "%s%.*fe%d", sign, digits-1, value, expof10);
  else
    snprintf(result, rlen, "%s%.*f %s", sign, digits-1, value,
	    prefix[(expof10-PREFIX_START)/3]);

  return result;
}

void usage(void)
{
  fprintf(stderr, "usage: comp [-r resistor_val] freq delta_t V_in V_dut\n");
  fprintf(stderr, "  delta_t: time from V_dut to V_in zero crossings\n");
  exit(EXIT_FAILURE);
}
  
int main(int argc, char **argv)
{
  double R = 992.3;
  double freq, phi, theta, V_in, V_dut, Z, Resr, Ls, Cs, delta_t, Q, X;
  int option_index = 1;
  char res[32];

  if (argc == 7) {
    if (strncmp(argv[option_index], "-r", 2) == 0) {
      option_index += 1;
      R = atof(argv[option_index++]);
    } else {
      usage();
    }
  }

  if (argc - option_index != 4) usage();

  freq = atof(argv[option_index++]);
  delta_t = atof(argv[option_index++]);
  V_in = atof(argv[option_index++]);
  V_dut = atof(argv[option_index++]);

  printf("Inputs:\n");
  printf("  Rref: %sOhms\n", eng(R, 4, 0, res, sizeof(res)));
  printf("  freq: %sHz\n", eng(freq, 4, 0, res, sizeof(res)));
  printf("  delta_t: %sSec\n", eng(delta_t, 4, 0, res, sizeof(res)));
  printf("  V_in: %sV\n", eng(V_in, 4, 0, res, sizeof(res)));
  printf("  V_dut: %sV\n", eng(V_dut, 4, 0, res, sizeof(res)));

  printf("Outputs:\n");

  theta = 2*M_PI*freq*delta_t;
  phi = theta - atan2(-V_dut*sin(theta), V_in - V_dut*cos(theta));

  /* It's easy for measurement errors to give impossible angles when
   * Resr is small */
  
  if (phi < -M_PI/2) {
    printf("  **Warning: phi < -pi/2 by %le rad.\n", phi + M_PI/2);
    printf("  ** Setting it to -pi/2\n");
    phi = -M_PI/2 + 1e-15; // small fudge to avoid div by zero
  }
  if (phi > M_PI/2) {
    printf("  **Warning: phi > pi/2 by %le rad.\n", phi - M_PI/2);
    printf("  ** Setting it to pi/2\n");
    phi = M_PI/2 - 1e-15; // small fudge to avoid div by zero
  }
  
  Z = V_dut*R/sqrt(V_in*V_in - 2*V_in*V_dut*cos(theta) + V_dut*V_dut);
  Resr = Z*cos(phi);
  X = Z*sin(phi);
  Q = fabs(X)/Resr;
  
  printf("  theta: %lf rad (%lf deg)\n", theta, theta*360.0/(2*M_PI));
  printf("  phi: %lf rad (%lf deg)\n", phi, phi*360.0/(2*M_PI));
  printf("  Z: %sOhms\n", eng(Z, 4, 0, res, sizeof(res)));
  if (phi > 0.0) {
    Ls = X/(2*M_PI*freq);
    printf("  Ls: %sH\n", eng(Ls, 4, 0, res, sizeof(res)));
    printf("  Lp: %sH\n", eng(Ls*(1 + 1/Q/Q), 4, 0, res, sizeof(res)));
  } else {
    Cs = -1/(2*M_PI*freq*X);
    printf("  Cs: %sF\n", eng(Cs, 4, 0, res, sizeof(res)));
    printf("  Cp: %sF\n", eng(Cs/(1 + 1/Q/Q), 4, 0, res, sizeof(res)));
  }
  printf("  Rs (Resr): %sOhms\n", eng(Resr, 4, 0, res, sizeof(res)));
  printf("  Rp: %sOhms\n", eng(Resr*(1+Q*Q), 4, 0, res, sizeof(res)));
  printf("  X: %sOhms\n", eng(X, 4, 0, res, sizeof(res)));
  printf("  Q: %lf\n", Q);

  return EXIT_SUCCESS;
}
