
#ifndef __tb_h_
#define __tb_h_

// rf12 related
#define network     210         //network group (can be in the range 1-250).
#define freq RF12_433MHZ        //Freq of RF12B can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. Match freq to module

// number of samples per channel to read
#define samples 20

#define PKT_RESULT   0x17
#define PKT_CHANNEL  0x21

typedef struct {
    uint8_t  packet_type;
    uint8_t  node_id;
    uint16_t hit_id;
    uint16_t instant_en;
    uint16_t s_en[4];
} payload_result;

typedef struct {
    uint8_t  packet_type;
    uint8_t  node_id;
    uint16_t s[samples];
} payload_channel;


// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#endif
