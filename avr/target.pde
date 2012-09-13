
#include <avr/wdt.h>
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <RF12.h>
#include <Ports.h>              //from jeelabs.org

#define FASTADC 0

// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define pin_s0 A0
#define pin_s1 A1
#define pin_s2 A2
#define pin_s3 A3
#define pin_sd_cs 4

#define samples 20

#define myNodeID 10             //node ID of tx (range 0-30)
#define network     210         //network group (can be in the range 1-250).
#define freq RF12_433MHZ        //Freq of RF12B can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. Match freq to module

typedef struct {
    int s_en[4];
    int counter;
    int total_en;
} payload;
payload p;

SdFat sd;
SdFile f;

// 4 sensors (microphones)
int s[4][samples];

int instant_en = 0;

int sample_delay = 1;

int threshold = 10;

// when to actually trigger the conversion
boolean adc_trigger = false;

// ADC conversion timeouts
unsigned long adc_conv_next = 0;
// if a delay is needed before trigger computation
unsigned long adc_conv_trigger_timeout = 0;
unsigned long adc_delay_between_triggers = 400;

// USART timeouts
unsigned long refresh_prev = 0;
unsigned long refresh_delay = 400;

void discharge_inputs()
{
    // discharge the capacitor used for envelope detection
    pinMode(pin_s0, OUTPUT);
    pinMode(pin_s1, OUTPUT);
    pinMode(pin_s2, OUTPUT);
    pinMode(pin_s3, OUTPUT);
    digitalWrite(pin_s0, LOW);
    digitalWrite(pin_s1, LOW);
    digitalWrite(pin_s2, LOW);
    digitalWrite(pin_s3, LOW);
    delay(2);

    // go back into listening mode
    pinMode(pin_s0, INPUT);
    pinMode(pin_s1, INPUT);
    pinMode(pin_s2, INPUT);
    pinMode(pin_s3, INPUT);
}

void setup()
{
    Serial.begin(9600);

#if FASTADC
    // set prescale to 16
    sbi(ADCSRA, ADPS2);
    cbi(ADCSRA, ADPS1);
    cbi(ADCSRA, ADPS0);
#endif
    if (!sd.init(SPI_HALF_SPEED, pin_sd_cs)) {
        //sd.initErrorHalt();
        Serial.println("Card failed, or not present");
        // don't do anything more:
        //return;
    }
    //rf12_initialize(myNodeID,freq,network);

    discharge_inputs();
    wdt_enable(WDTO_8S);
    p.counter = 0;
}

/*
void discharge_input_cap(int pin)
{
    // discharge the capacitor used for envelope detection
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(2);

    // go back into listening mode
    pinMode(pin, INPUT);
}
*/

void read_instant_en()
{
    instant_en =
        analogRead(pin_s0) + analogRead(pin_s1) + analogRead(pin_s2) + analogRead(pin_s3);

    discharge_inputs();
}

void read_inputs_multi()
{
    uint8_t i;

    for (i = 0; i < samples; i++) {
        s[0][i] = analogRead(pin_s0);
        s[1][i] = analogRead(pin_s1);
        s[2][i] = analogRead(pin_s2);
        s[3][i] = analogRead(pin_s3);
        //delay(sample_delay);
    }

    p.s_en[0] = 0;
    p.s_en[1] = 0;
    p.s_en[2] = 0;
    p.s_en[3] = 0;

    for (i = 0; i < samples; i++) {
        p.s_en[0] += s[0][i];
        p.s_en[1] += s[1][i];
        p.s_en[2] += s[2][i];
        p.s_en[3] += s[3][i];
    }
}

void loop()
{
    unsigned long now;
    now = millis();
    wdt_reset();

    if (adc_trigger == true) {
        if (now > adc_conv_next) {
            read_inputs_multi();
            discharge_inputs();
            adc_trigger = false;
            // wait a while if this was a worthy trigger
            if (p.s_en[0] + p.s_en[1] + p.s_en[2] + p.s_en[3]  > 400) {
                adc_conv_next = now + adc_delay_between_triggers;
            } else {
                instant_en = 0;
            }
        }
    } else {
        if (now > adc_conv_next) {
            read_instant_en();
            if (instant_en > 200) {     // threshold
                adc_trigger = true;
                adc_conv_next = now + adc_conv_trigger_timeout;
            }
        }
    }

    if (now - refresh_prev > refresh_delay) {
        refresh_prev = now;

        if (instant_en > 200) { // threshold
            f.open("trigger.log", O_RDWR | O_CREAT | O_AT_END);

            Serial.print(p.counter);
            Serial.print(" ");
            Serial.println(instant_en);
            f.print(p.counter);
            f.print(" ");
            f.println(instant_en);

            uint8_t i,j;
            for (j = 0; j < 4; j++) {
                for (i = 0; i < samples; i++) {
                    Serial.print(s[j][i]);
                    Serial.print(" ");
                    f.print(s[j][i]);
                    f.print(" ");
                }
                Serial.println(p.s_en[j]);
                f.println(p.s_en[j]);
            }

            i = 0;
            //while (!rf12_canSend() && i<10) {
            //    rf12_recvDone(); i++;
            //}
            //rf12_sendStart(0, &p, sizeof p);
            instant_en = 0;
            discharge_inputs();
            f.close();
            p.counter++;
        }
    }
}
