//Simple RFM12B wireless demo - Receiver - no ack
//Glyn Hudson openenergymonitor.org GNU GPL V3 12/4/12
//Credit to JCW from Jeelabs.org for RFM12 

#include <avr/wdt.h>     // Include watchdog library
#include <JeeLib.h>

#include "tb.h"

uint8_t pkt_type;
payload_result p;

void setup() {
    delay(200);
    wdt_enable(WDTO_8S);   // Enable watchdog: max 8 seconds

    Serial.begin(9600); 

    rf12_initialize(30,freq,network);   //Initialize RFM12 with settings defined above  
  
    Serial.print(" Freq: "); 
    if (freq == RF12_433MHZ) Serial.print("433Mhz");
    if (freq == RF12_868MHZ) Serial.print("868Mhz");
    if (freq == RF12_915MHZ) Serial.print("915Mhz"); 
    Serial.print(" Network: "); 
    Serial.println(network);
}

void loop() {
    wdt_reset();           // Reset watchdog
  
    if (rf12_recvDone()){    
        if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0) {
            pkt_type=*(uint8_t*) rf12_data;
            switch (pkt_type) {
                case PKT_RESULT:
                    p=*(payload_result*) rf12_data;
                    Serial.print("node_id: "); Serial.println(p.node_id); 
                    Serial.print("hit_id: "); Serial.println(p.hit_id); 
                    Serial.print("instant_en: "); Serial.println(p.instant_en); 
                    Serial.print("hits: "); 
                    Serial.print(p.s_en[0]); 
                    Serial.print(" "); 
                    Serial.print(p.s_en[1]); 
                    Serial.print(" "); 
                    Serial.print(p.s_en[2]); 
                    Serial.print(" "); 
                    Serial.println(p.s_en[3]); 
                    Serial.println("  "); 
                break;
                default:
                    Serial.println("ERR unk_pck");
                break;
            }
        } else {
            Serial.println("ERR crc_err");
        }

    }
}

