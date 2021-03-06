#include <wiringPi.h>
#include <math.h>
#include <iostream>
#include <time.h>
#include "stdlib.h"
#include "main.h"
#include "BCHEncoder.h"
#include "adf7012.h"

unsigned int powerlevel;
int freq_err = -17;
unsigned int pow_amp = 63;

//Registervariablen anlegen: adf_config ===========================================================

struct {
	struct {
		unsigned int  frequency_error_correction;
		unsigned char r_divider;
		unsigned char crystal_doubler;
		unsigned char crystal_oscillator_disable;
		unsigned char clock_out_divider;
		unsigned char vco_adjust;
		unsigned char output_divider;
	} r0;

	struct {
		unsigned int  fractional_n;
		unsigned char integer_n;
		unsigned char prescaler;
	} r1;

	struct {
		unsigned char mod_control;
		unsigned char gook;
		unsigned char power_amplifier_level;
		unsigned int  modulation_deviation;
		unsigned char gfsk_modulation_control;
		unsigned char index_counter;
	} r2;

	struct {
		unsigned char pll_enable;
		unsigned char pa_enable;
		unsigned char clkout_enable;
		unsigned char data_invert;
		unsigned char charge_pump_current;
		unsigned char bleed_up;
		unsigned char bleed_down;
		unsigned char vco_disable;
		unsigned char muxout;
		unsigned char ld_precision;
		unsigned char vco_bias;
		unsigned char pa_bias;
		unsigned char pll_test_mode;
		unsigned char sd_test_mode;
	} r3;
} adf_config;




// Konfiguration des ADF ==========================================================================

// Power up default settings are defined here:

void RadioAdf7012::adf_reset_register_zero(void) {
	adf_config.r0.frequency_error_correction = freq_err;             // Don't bother for now...
	adf_config.r0.r_divider = ADF7012_CRYSTAL_DIVIDER;          // Whatever works best for 2m, 1.25m and 70 cm ham bands
	adf_config.r0.crystal_doubler = 0;                          // W  ho would want that? Lower f_pfd means finer channel steps.
	adf_config.r0.crystal_oscillator_disable = 0;               // Disable internal crystal oscillator because we have an external VCXO
	adf_config.r0.clock_out_divider = 1;                        // Don't bother for now...
	adf_config.r0.vco_adjust = 2;                               // Don't bother for now... (Will be automatically adjusted until PLL lock is achieved)
	adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_2;     // Pre-set div 4 for 2m. Will be changed according tx frequency on the fly
}

void RadioAdf7012::adf_reset_register_one(void) {
	adf_config.r1.integer_n = 179;                              // Pre-set for 144.390 MHz APRS. Will be changed according tx frequency on the fly
	adf_config.r1.fractional_n = 128;                          // Pre-set for 144.390 MHz APRS. Will be changed according tx frequency on the fly
	adf_config.r1.prescaler = ADF_PRESCALER_4_5;                // 8/9 requires an integer_n > 91; 4/5 only requires integer_n > 31
}

void RadioAdf7012::adf_reset_register_two(void) {
	adf_config.r2.mod_control = ADF_MODULATION_GFSK;             // For AFSK the modulation is done through the external VCXO we don't want any FM generated by the ADF7012 itself
	adf_config.r2.gook = 0;                                     // Whatever... This might give us a nicer swing in phase maybe...
	adf_config.r2.power_amplifier_level = pow_amp;              // 16 is about half maximum power. Output −20dBm at 0x0, and 13 dBm at 0x7E at 868 MHz
	adf_config.r2.modulation_deviation = 120;               	// 16 is about half maximum amplitude @ ASK.
	adf_config.r2.gfsk_modulation_control = 1;                  
	adf_config.r2.index_counter = 2;                            // 2 entspricht Wert: 64
}

void RadioAdf7012::adf_reset_register_three(void) {
	adf_config.r3.pll_enable = 0;                               // Switch off PLL (will be switched on after Ureg is checked and confirmed ok)
	adf_config.r3.pa_enable = 0;                                // Switch off PA  (will be switched on when PLL lock is confirmed)
	adf_config.r3.clkout_enable = 0;                            // No clock output needed at the moment
	adf_config.r3.data_invert = 1;                              // Results in a TX signal when TXDATA input is low
	adf_config.r3.charge_pump_current = ADF_CP_CURRENT_2_1;     // 2.1 mA. This is the maximum
	adf_config.r3.bleed_up = 0;                                 // Don't worry, be happy...
	adf_config.r3.bleed_down = 0;                               // Dito
	adf_config.r3.vco_disable = 0;                              // VCO is on
	adf_config.r3.muxout = ADF_MUXOUT_REG_READY;                // Lights up the green LED if the ADF7012 is properly powered (changes to lock detection in a later stage)
	adf_config.r3.ld_precision = ADF_LD_PRECISION_3_CYCLES;     // What the heck? It is recommended that LDP be set to 1; 0 is more relaxed
	adf_config.r3.vco_bias = 1;                                 // In 0.5 mA steps; Default 6 means 3 mA; Maximum (15) is 8 mA
	adf_config.r3.pa_bias = 4;                                  // In 1 mA steps; Default 4 means 8 mA; Minimum (0) is 5 mA; Maximum (7) is 12 mA (Datasheet says uA which is bullshit)
	adf_config.r3.pll_test_mode = 0;
	adf_config.r3.sd_test_mode = 0;
}




// Resetfunktion ----------------------------------------------------------------------------------
void RadioAdf7012::adf_reset_config(void)
{
	adf_reset_register_zero();
	adf_reset_register_one();
	adf_reset_register_two();
	adf_reset_register_three();
	//    adf_reset();  
	//    while(!adf_reg_ready());
}


void RadioAdf7012::set_params(int f, int p)
{
  freq_err = f;
  pow_amp = p;
}

void RadioAdf7012::get_params(){
  printf("Aktuelle Werte:\n\tFREQ ERR CORR %d\n\tPOW AMP %d\n\n", freq_err,pow_amp);
}

// Configuration writing functions ----------------------------------------------------------------
void RadioAdf7012::adf_write_config(void)
{
	adf_write_register_zero();
	adf_write_register_one();
	adf_write_register_two();
	adf_write_register_three();
}


void RadioAdf7012::adf_write_register_zero(void)
{
	unsigned long reg =
		(0) |
		((unsigned long)(adf_config.r0.frequency_error_correction & 0x7FF) << 2U) |
		((unsigned long)(adf_config.r0.r_divider & 0xF) << 13U) |
		((unsigned long)(adf_config.r0.crystal_doubler & 0x1) << 17U) |
		((unsigned long)(adf_config.r0.crystal_oscillator_disable & 0x1) << 18U) |
		((unsigned long)(adf_config.r0.clock_out_divider & 0xF) << 19U) |
		((unsigned long)(adf_config.r0.vco_adjust & 0x3) << 23U) |
		((unsigned long)(adf_config.r0.output_divider & 0x3) << 25U);

	adf_write_register(reg);
}


void RadioAdf7012::adf_write_register_one(void)
{
	unsigned long reg =
		(1) |
		((unsigned long)(adf_config.r1.fractional_n & 0xFFF) << 2) |
		((unsigned long)(adf_config.r1.integer_n & 0xFF) << 14) |
		((unsigned long)(adf_config.r1.prescaler & 0x1) << 22);

	adf_write_register(reg);
}


void RadioAdf7012::adf_write_register_two(void)
{
	unsigned long reg =
		(2) |
		((unsigned long)(adf_config.r2.mod_control & 0x3) << 2) |
		((unsigned long)(adf_config.r2.gook & 0x1) << 4) |
		((unsigned long)(adf_config.r2.power_amplifier_level & 0x3F) << 5) |
		((unsigned long)(adf_config.r2.modulation_deviation & 0x1FF) << 11) |
		((unsigned long)(adf_config.r2.gfsk_modulation_control & 0x7) << 20) |
		((unsigned long)(adf_config.r2.index_counter & 0x3) << 23);

	adf_write_register(reg);
}


void RadioAdf7012::adf_write_register_three(void)
{
	unsigned long reg =
		(3) |
		((unsigned long)(adf_config.r3.pll_enable & 0x1) << 2) |
		((unsigned long)(adf_config.r3.pa_enable & 0x1) << 3) |
		((unsigned long)(adf_config.r3.clkout_enable & 0x1) << 4) |
		((unsigned long)(adf_config.r3.data_invert & 0x1) << 5) |
		((unsigned long)(adf_config.r3.charge_pump_current & 0x3) << 6) |
		((unsigned long)(adf_config.r3.bleed_up & 0x1) << 8) |
		((unsigned long)(adf_config.r3.bleed_down & 0x1) << 9) |
		((unsigned long)(adf_config.r3.vco_disable & 0x1) << 10) |
		((unsigned long)(adf_config.r3.muxout & 0xF) << 11) |
		((unsigned long)(adf_config.r3.ld_precision & 0x1) << 15) |
		((unsigned long)(adf_config.r3.vco_bias & 0xF) << 16) |
		((unsigned long)(adf_config.r3.pa_bias & 0x7) << 20) |
		((unsigned long)(adf_config.r3.pll_test_mode & 0x1F) << 23) |
		((unsigned long)(adf_config.r3.sd_test_mode & 0xF) << 28);

	adf_write_register(reg);
}




void RadioAdf7012::adf_write_register(unsigned long data)
{
	//adf_reset();
	//digitalWrite(SSpin,   HIGH);
	//digitalWrite(ADF7012_TX_DATA_PIN, HIGH);
	//digitalWrite(SCKpin,  HIGH);
	//digitalWrite(MOSIpin, HIGH);
	//std::cout << "Register = " << data << std::endl;
	int i;
	digitalWrite(CLK, LOW);
	delayMicroseconds(2);
	digitalWrite(LE, LOW);
	delayMicroseconds(10);

	for (i = 31; i >= 0; i--) {
		if ((data & (unsigned long)(1UL << i)) >> i)
			digitalWrite(SDATA, HIGH);
		else
			digitalWrite(SDATA, LOW);
		delayMicroseconds(10);
		digitalWrite(CLK, HIGH);
		delayMicroseconds(30);
		digitalWrite(CLK, LOW);
		delayMicroseconds(30);
	}
	delayMicroseconds(10);
	digitalWrite(LE, HIGH);
}




void RadioAdf7012::adf_reset(void)
{
	digitalWrite(CE, LOW);
	digitalWrite(LE, HIGH);
	digitalWrite(TXDATA, HIGH);
	digitalWrite(CLK, HIGH);
	digitalWrite(SDATA, HIGH);

	delay(5);

	digitalWrite(CE, HIGH);

	delay(100);
}




// Steuerungsfunktionen ===========================================================================

/*

bool RadioAdf7012::ptt_on()
{
	digitalWrite(CE, HIGH);
	digitalWrite(TXDATA, LOW);
	adf_config.r3.pa_enable = 0;
	adf_config.r2.power_amplifier_level = 0;
	adf_config.r3.muxout = ADF_MUXOUT_REG_READY;

	adf_write_config();
	delay(100);
	//analogReference(DEFAULT);

	int adc = digitalRead(MUXOUT);

	//std::cout << "MUXOUT = " << adc << std::endl;

	if (adc == 0U)  // Power is bad
	{
		std::cout << "ERROR: Can't power up the ADF7012!" << std::endl;
	}
	else              // Power is good apparently
	{
		if (adf_lock())
		{
			adf_config.r3.pa_enable = 1;
			adf_config.r2.power_amplifier_level = 63; //63 is max power
			adf_write_config();
			delay(50);
			std::cout << "ADF7012 ist auf Sendung..." << std::endl;
			return true;
/*			digitalRead(MUXOUT); // blank read for equilibration
			powerlevel = digitalRead(MUXOUT);

			if (powerlevel > 0)
			{
				powerlevel = 1;
			}
*//*
		}
		else
		{
			ptt_off();
		}
	}
	return false;
}
*/



bool RadioAdf7012::ptt_on()
{
	digitalWrite(CE, HIGH);
	digitalWrite(TXDATA, LOW);
	adf_config.r3.pa_enable = 0;
	adf_config.r2.power_amplifier_level = 0;
	adf_config.r3.muxout = ADF_MUXOUT_REG_READY;

	adf_write_config();
	delay(100);

	int adc = digitalRead(MUXOUT);


	if (adc != 0U)           // Power is good apparently
	{
		if (adf_lock())
		{
			adf_config.r3.pa_enable = 1;
			adf_config.r2.power_amplifier_level = pow_amp; //63 is max power
			adf_write_config();
			delay(50);
			return true;
		}
		else
		{
			ptt_off();
		}
	}
	return false;
}


void RadioAdf7012::ptt_off()
{
	//  divider_test();
	//  change();
	//  digitalWrite(TX_PA_PIN, HIGH); // Switch off the ADL5531 final PA (HIGH = off)
	adf_config.r3.pa_enable = 0;
	adf_config.r2.power_amplifier_level = 0;
	adf_write_config();
	delay(100);

	digitalWrite(CE, LOW);
	digitalWrite(TXDATA, LOW);
}




int RadioAdf7012::adf_lock(void)
{
	int adj = adf_config.r0.vco_adjust; // use default start values from setup
	int bias = adf_config.r3.vco_bias;  // or the updated ones that worked last time

	adf_config.r3.pll_enable = 1;
	adf_config.r3.muxout = ADF_MUXOUT_DIGITAL_LOCK;
	
	adf_write_config();
	delay(500);
	std::cout << "Versuche PLL zu rasten..." << std::endl;
	//adf_locked();

	while (!adf_locked()) {
		adf_config.r0.vco_adjust = adj; 
		adf_config.r3.vco_bias = bias;
		adf_config.r3.muxout = ADF_MUXOUT_DIGITAL_LOCK;
		adf_write_config();
		delay(500);
		if (++bias == 14) {
			bias = 1;
			if (++adj == 4) {

				std::cout << "Couldn't achieve PLL lock :( " << std::endl;

				adf_config.r0.vco_adjust = 0;
				adf_config.r3.vco_bias = 0;
				return 0;
			}
		}
		
	}
	std::cout << "Sender eingeschaltet." << std::endl;
	
	return 1;
}




int RadioAdf7012::adf_locked(void)
{
	//delay(500);
	int adc = digitalRead(MUXOUT);
	//std::cout << adc <<" " << (int)adf_config.r0.vco_adjust << " " << (int)adf_config.r3.vco_bias<< std::endl;
	//delay(500);
	if (adc == 1)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}




int RadioAdf7012::get_powerlevel()
{
	return powerlevel;
}



// hier wird die Frequenz gesetzt und auch die Werte für die PLL berechnet ------------------------

void RadioAdf7012::set_freq(unsigned long freq)
{
	adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_1;
//	if (freq < 450000000) { adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_2; };
//	if (freq < 210000000) { adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_4; };
//	if (freq < 130000000) { adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_8; };
	unsigned long f_pfd = CRYSTALFREQ / adf_config.r0.r_divider;

	unsigned int n = (unsigned int)(freq / f_pfd);

	float ratio = (float)freq / (float)f_pfd;
	float rest = ratio - (float)n;

	unsigned long m = (unsigned long)(rest * 4096);
	
//	std::cout << "INT: " << m << std::endl;
//	std::cout << "FRAC: " << n << std::endl;
	
	adf_config.r1.integer_n = n;
	adf_config.r1.fractional_n = m;
}




void RadioAdf7012::setup()
{
	/*std::cout << "==============================================================" << std::endl;
	std::cout << "ADF7012 setup start" << std::endl;
	std::cout << "==============================================================" << std::endl;*/

	pinMode(CE, OUTPUT);
	pinMode(CLK, OUTPUT);
	pinMode(LE, OUTPUT);
	pinMode(SDATA, OUTPUT);
	pinMode(TXDATA, OUTPUT);

	adf_reset_config();
	set_freq(FREQUENCY); // Set the default frequency
	adf_write_config();
	
	digitalWrite(TXDATA, LOW);
	//  digitalWrite(TX_PA_PIN, HIGH);  // HIGH = off => make sure the PA is off after boot.
	delay(100);

	std::cout << "ADF7012 setup done" << std::endl;
}





/*

int main(void)
{
	wiringPiSetup();

	pinMode(LE, OUTPUT);
	pinMode(CE, OUTPUT);
	pinMode(CLK, OUTPUT);
	pinMode(SDATA, OUTPUT);
	pinMode(TXDATA, OUTPUT);
	pinMode(CLKOUT, OUTPUT);

	pinMode(MUXOUT, INPUT);
	pinMode(TXCLK, INPUT);

	RadioAdf7012 radio = *new RadioAdf7012();

	radio.setup();

	delayMicroseconds(2000);

	radio.ptt_on();
	std::cout << "PTT_ON" << std::endl;
	/*	
	long delay_ns = 833000;
	struct timespec next;
	clock_gettime(CLOCK_MONOTONIC, &next);
	
	for(int i=0; i<5576; i++){
	  
	digitalWrite(TXDATA, HIGH);
	 next.tv_nsec += delay_ns;
	  next.tv_nsec %= 1000000000;
	  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
	
	digitalWrite(TXDATA, LOW);
	 next.tv_nsec += delay_ns;
	  next.tv_nsec %= 1000000000;
	  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
	
	
	}
	
	clock_gettime(CLOCK_MONOTONIC, &next);
	for(int i=0; i<500; i++){
	
	  next.tv_nsec += delay_ns;
	  next.tv_nsec %= 1000000000;
	  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
	  
	  
	  int x = rand() % 2;
	
	digitalWrite(TXDATA, x);
	
	}	

	

	
	
	delay(10000);	
	
	

/*
//Präambel erstellen (mindestens 576 bit):

	for (int i=0; i<700; i++){
		
	digitalWrite(TXDATA, HIGH);
	delayMicroseconds(733);
	digitalWrite(TXDATA, LOW);
	delayMicroseconds(733);
	}
	
//Synchronwort erstellen 01111100110100100001010111011000:

for (int k = 30; k>=0; k--){

	for (int i = 31; i >= 0; i--) {
		if (((unsigned long)POCSAG_SYNCH_CODEWORD & (unsigned long)(1UL << i)) >> i){
			digitalWrite(TXDATA, HIGH);
			delayMicroseconds(633);
		}
		else{
			digitalWrite(TXDATA, LOW);
			delayMicroseconds(633);
		}
	}
//Nachricht erstellen
	
	for (int i=0; i<100; i++){
	int x = rand() % 2;
	
	digitalWrite(TXDATA, x);
	delayMicroseconds(633);
	}
	
}

*/	

/*
	for (int i=0; i<2000; i++){
	int x = rand() % 2;
	
	digitalWrite(TXDATA, x);
	delayMicroseconds(733);
	}
	
	radio.ptt_off();

	std::cout << "ENDE" << std::endl;

	/*
	  for (;;)
	  {
	  digitalWrite (LE, HIGH) ; delay (500) ;
	  digitalWrite (LE,  LOW) ; delay (500) ;
	  std::cout << "LED blinkt" << std::endl;
	  }
	  
	return 0;
}
*/
