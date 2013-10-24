
#include <avr/wdt.h>
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>

//from jeelabs.org
#include <RF12.h>
#include <Ports.h>

#include "tb.h"

#define NODE_ID 10 // 0 to 29
#define FASTADC 0

#define pin_s0 A0
#define pin_s1 A1
#define pin_s2 A2
#define pin_s3 A3
#define pin_sd_cs 4

payload_result p;
payload_channel sen[4];

SdFat sd;
SdFile f;

int sample_delay = 1;

int threshold = 10;

// when to actually trigger the conversion
boolean adc_trigger = false;

// ADC conversion timeouts
unsigned long adc_conv_next = 2000;
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
    uint8_t i;
    wdt_enable(WDTO_8S);

    delay(200); // allow 3v3 caps to fill up

    Serial.begin(9600);

#if FASTADC
    // set prescale to 16
    sbi(ADCSRA, ADPS2);
    cbi(ADCSRA, ADPS1);
    cbi(ADCSRA, ADPS0);
#endif

    if (!sd.init(SPI_HALF_SPEED, pin_sd_cs)) {
        Serial.println("ERR sd.init failed");
    }

    rf12_initialize(NODE_ID,freq,network);

    discharge_inputs();

    Serial.print("target butler node ");
    Serial.print(NODE_ID);
    Serial.println(" started");
    //Serial.println(sizeof p);

    p.node_id = NODE_ID;
    p.hit_id = 0;
    p.instant_en = 0;
    p.packet_type = PKT_RESULT;

    for (i=0; i<4; i++) {
        sen[i].packet_type = PKT_CHANNEL;
        sen[i].node_id = NODE_ID;
    }
}

void read_instant_en()
{
    p.instant_en =
        analogRead(pin_s0) + analogRead(pin_s1) + analogRead(pin_s2) + analogRead(pin_s3);

    discharge_inputs();
}

void read_inputs_multi()
{
    uint8_t i;

    for (i = 0; i < samples; i++) {
        sen[0].s[i] = analogRead(pin_s0);
        sen[1].s[i] = analogRead(pin_s1);
        sen[2].s[i] = analogRead(pin_s2);
        sen[3].s[i] = analogRead(pin_s3);
        //delay(sample_delay);
    }

    p.s_en[0] = 0;
    p.s_en[1] = 0;
    p.s_en[2] = 0;
    p.s_en[3] = 0;

    for (i = 0; i < samples; i++) {
        p.s_en[0] += sen[0].s[i];
        p.s_en[1] += sen[1].s[i];
        p.s_en[2] += sen[2].s[i];
        p.s_en[3] += sen[3].s[i];
    }
}

void loop()
{
    unsigned long now;
    uint8_t i,j;
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
                p.instant_en = 0;
            }
        }
    } else {
        if (now > adc_conv_next) {
            read_instant_en();
            if (p.instant_en > 200) {     // threshold
                adc_trigger = true;
                adc_conv_next = now + adc_conv_trigger_timeout;
            }
        }
    }

    if (now - refresh_prev > refresh_delay) {
        refresh_prev = now;
        //Serial.println(now);

        if (p.instant_en > 200) { // threshold
            f.open("trigger.log", O_RDWR | O_CREAT | O_AT_END);

            Serial.print(p.hit_id);
            Serial.print(" ");
            Serial.println(p.instant_en);
            f.print(p.hit_id);
            f.print(" ");
            f.println(p.instant_en);

            for (j = 0; j < 4; j++) {
                for (i = 0; i < samples; i++) {
                    Serial.print(sen[j].s[i]);
                    Serial.print(" ");
                    f.print(sen[j].s[i]);
                    f.print(" ");
                }
                Serial.println(p.s_en[j]);
                f.println(p.s_en[j]);
            }

            float result;
            int dr, st, max;

            dr=p.s_en[0];
            st=p.s_en[2];

            if (dr > st) {
                max=dr;
            } else {
                max=st;
            }

            Serial.println(dr);
            Serial.println(st);
            Serial.println(max);
            result = (dr-st)*100.0/max;
            Serial.print("result ");
            Serial.println(result);

            i = 0;
            while (!rf12_canSend() && i<10) {
                rf12_recvDone();
                i++;
            }
            rf12_sendStart(0, &p, sizeof p);
/*
            for (j = 0; j < 4; j++) {
                i = 0;
                while (!rf12_canSend() && i<10) {
                    rf12_recvDone();
                    i++;
                }
                rf12_sendStart(0, &sen[j], sizeof sen);
            }
*/
            p.instant_en = 0;
            discharge_inputs();
            f.close();
            p.hit_id++;
        }
    }
}
