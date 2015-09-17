#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>
#include <printf.h>
#include <packetinterface.h>
#include <utility.h>

uint64_t address_client = 0xF0F0F0AA01LL;
uint64_t address_server = 0xF0F0F0BB02LL;
RF24 radio(9, 10);

// channel hopping
uint8_t channels[] = {0, 95, 31, 127, 63};
const uint8_t nChannels = sizeof(channels)/sizeof(uint8_t);
uint8_t i_channel = 0;
bool is_connected = true;

const unsigned long wi_timeout = 20000;

const unsigned int maxBufLen = 100;
unsigned char buf[maxBufLen];
PacketInterface pi;
unsigned int count = 0;

const uint16_t payload_size = 16;
uint8_t in_data[payload_size];
uint8_t out_data[payload_size];

unsigned long started_waiting_at;

void setup()
{
    Serial.begin(57600);
    printf_begin();
    radio.begin();
    radio.setDataRate(RF24_250KBPS);
    radio.setChannel(channels[i_channel]);
    radio.setPALevel(RF24_PA_HIGH);
    radio.setRetries(15,15);
    radio.setPayloadSize(payload_size);
    radio.openWritingPipe(address_server);
    radio.openReadingPipe(1, address_client);
    radio.startListening();
 //   radio.printDetails();

    started_waiting_at = micros();
}


const unsigned char req_set_current = 1;
const unsigned char req_get_info = 2;
const unsigned char req_check_channel = 3;

const unsigned char ans_error = 1;
const unsigned char ans_values = 2;
const unsigned char ans_error_2 = 3;
const unsigned char ans_check_channel = 4;

void findFreeChannel()
{
    bool ok = false;
    while(!ok) {
        radio.stopListening();
//        radio.setChannel(channels[i_channel]);
        radio.startListening();
        bool timeout = false;
        unsigned long _started_waiting_at = micros();
        while ((!radio.available()) && (!timeout)) {
            if ((micros() - _started_waiting_at) > 10*wi_timeout) {
                timeout = true;
            }
        }
        if (!timeout) {
            in_data[0] = 0;
            radio.read(&(in_data[0]), payload_size);
            if (in_data[0] == ans_check_channel) {
                radio.stopListening();
                out_data[0] = req_check_channel;
                if (radio.write(&(out_data[0]), payload_size)) {
                    radio.startListening();
                        return;                    
                }
                radio.startListening();
            }
        }
        i_channel = ((i_channel + 1) % nChannels);
    }
}


void loop()
{
    if (radio.available()) {

        for (uint8_t i = 0; i < payload_size; ++i) { in_data[i] = 0; }        
        radio.read(&(in_data[0]), payload_size);
        unsigned char what = in_data[0];

        if (what == req_set_current) {
            started_waiting_at = micros();

            // set current
            int16_t current = read_16(in_data + 1);

            if (pi.setCurrent(current/100.)) {
                Serial.write(pi.Buffer.buf, pi.Buffer.len);
                Serial.flush();
            }
        
        } else if (what == req_get_info) {
            // return readings

            if (pi.getValues()) {
                Serial.write(pi.Buffer.buf, pi.Buffer.len);
                Serial.flush();
            }

            delay(10);

            radio.stopListening();
            
//            radio.flush_tx();

            if (Serial.available()) {
                unsigned int len = 0;
                while (Serial.available() && len < maxBufLen) {
                    int b = Serial.read();
                    buf[len] = b;
                    ++len;
                }
                
                pi.processData(reinterpret_cast<unsigned char *>(buf), len);
        
                if (pi.new_r_values) {
                    uint16_t v_in = 100 * pi.r_values.v_in;
                    uint16_t amp_hours = 100 * pi.r_values.amp_hours;
                    uint16_t amp_hours_charged = 100 * pi.r_values.amp_hours_charged;
                    int32_t rpm = pi.r_values.rpm;
                    uint32_t tachometer_abs = pi.r_values.tachometer_abs;

                    out_data[0] = ans_values;

                    save_u16(out_data + 1, v_in);
                    save_32(out_data + 3, rpm);
                    save_u16(out_data + 8, amp_hours);
                    save_u16(out_data + 10, amp_hours_charged);
                    save_u32(out_data + 12, tachometer_abs);

                    radio.write(out_data, payload_size);
                    
                    pi.new_r_values = false;
                } else {
                    out_data[0] = ans_error;
                    radio.write(out_data, payload_size);
                }
            } else {
                    out_data[0] = ans_error_2;
                    radio.write(out_data, payload_size);
            }

//            radio.flush_tx();
            
            radio.startListening();
            
        } else {
            // error
        }
    }
    if (micros() - started_waiting_at > 1000000) {
        for (uint8_t i = 0; i < 10; ++i) {
            if (pi.setCurrent(0)) {
                Serial.write(pi.Buffer.buf, pi.Buffer.len);
                Serial.flush();
            }
        }
        findFreeChannel();
        started_waiting_at = micros();
    }
}
 
