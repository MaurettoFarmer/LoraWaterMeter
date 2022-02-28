#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h> 
#include <avr/power.h>
#include <SPI.h>
#include <LoRa.h>

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define MAXWAIT         64 //600 sec 12 //126 sec    
#define DEVICE_NAME     "MMO_ORTO8"

volatile unsigned int   liter;
volatile unsigned int   wdcount;

uint16_t readVcc(void) 
{
  sbi(ADCSRA,ADEN);                                                 // switch Analog to Digitalconverter ON
  ADMUX = (0<<REFS0) | (12<<MUX0);                                  // Read 1.1V reference against Vcc      
  delay(2);                                                         // Wait for Vref to settle
  ADCSRA |= (1<<ADSC);                                              // Convert
  while (bit_is_set(ADCSRA,ADSC));                                
                                  
  uint16_t result = ADCW;                               
  cbi(ADCSRA,ADEN);                                                 // switch Analog to Digitalconverter OFF
  return 1125300L / result;                                         // Back-calculate AVcc in mV
}

bool sendliter()
{
  bool ret = false;
  noInterrupts ();     
  if (LoRa.begin(868E6))
  {
    LoRa.beginPacket();
    LoRa.printf("%s|%d|%d",DEVICE_NAME, liter,readVcc()); 
    LoRa.endPacket();
    LoRa.end();
    ret = true;
  }
  interrupts ();
  return ret;
}

void resetwd ()
{
  wdt_disable();
  MCUSR = 0;                                                        // clear various "reset" flags
  WDTCR = (1 << WDCE) | (1 << WDE);                                 // allow changes, disable reset
  WDTCR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);                  // set WDIE, and 8 seconds delay
  wdt_reset();                                                      // pat the dog
} 

ISR(PCINT0_vect)
{
  if (!(PINB & (1<<PB3))) 
  {
    liter++;
    if(!(WDTCR & WDE))
      resetwd ();
  }
}

ISR(WDT_vect)
{
  wdcount++;
}

void gotosleep()
{
  DDRB = 0x00;                                                      // all input to minor consumption
  PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
  set_sleep_mode ( SLEEP_MODE_PWR_DOWN );                           // set sleep mode Power Down
  power_all_disable ();                                             // turn power off to ADC, TIMER 1 and 2, Serial Interface 
  noInterrupts ();                                                  // turn off interrupts as a precaution
  sleep_enable ();                                                  // allows the system to be commanded to sleep
  byte mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);                     // turn off the brown-out detector
  byte mcucr2 = mcucr1 & ~_BV(BODSE);                               // if the MCU does not have BOD disable capability,
  MCUCR = mcucr1;                                                   // this code has no effect
  MCUCR = mcucr2;                         
  interrupts ();                                                    // turn on interrupts  
  sleep_cpu ();                                                     // send the system to sleep, night night!
  sleep_disable ();                                                 // after ISR fires, return to here and disable sleep
  power_all_enable ();                                              // turn on power to ADC, TIMER1 and 2, Serial Interface
  cbi(ADCSRA,ADEN);                                                 // switch Analog to Digitalconverter OFF
}

void setup()
{
  liter    = 0;
  wdcount  = 0;
  DDRB     = 0;                                                     // all input to minor consumption
  MCUCR |= _BV(ISC01); 
  MCUCR |= _BV(ISC00);
  PORTB |= (1 << PORTB3);                                           // activate internal pull-up resistor for PB3
  GIMSK |= (1 << PCIE);                                             // pin change interrupt enable
	PCMSK |= (1 << PCINT3);                                           // pin change interrupt enabled for PCINT3
  cbi(ADCSRA,ADEN); 
  LoRa.setPins(PB4, -1, -1);
  wdt_disable();
  sendliter();
}

void loop() 
{
  gotosleep();
  if(wdcount > MAXWAIT)
  {
    wdt_disable();
    if(sendliter())
    {
      liter    = 0;
      wdcount  = 0;
    }
  }
}
