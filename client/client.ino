#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>
#include <SPI.h>
#include <printf.h>
#include <OLED_I2C.h> 
#include <utility.h>

const uint16_t payload_size = 16;
uint8_t in_data[payload_size];
uint8_t out_data[payload_size];

uint64_t address_client = 0xF0F0F0AA01LL;
uint64_t address_server = 0xF0F0F0BB02LL;
RF24 radio(9, 10);

// channel hopping
uint8_t channels[] = {0, 95, 31, 127, 63};
const uint8_t nChannels = sizeof(channels)/sizeof(uint8_t);
uint8_t i_channel = 0;
bool is_connected = true;


OLED  myOLED(2, 3, 2); // SDA - 2pin, SCL - 3pin
extern uint8_t TinyFont[];
extern uint8_t SmallFont[];
extern uint8_t RusFont[]; // Русский шрифт
extern uint8_t MediumNumbers[];
extern uint8_t BigNumbers[];
extern uint8_t MegaNumbers[];

#define gas A0
#define left_right A1
#define v_tr_pin A2
#define but_C 7
#define but_Z 6

int vs_tr[10];

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
    radio.openWritingPipe(address_client);
    radio.openReadingPipe(1, address_server); 
    radio.startListening();
    radio.printDetails();
    pinMode(gas, INPUT);
    pinMode(left_right, INPUT);
    pinMode(v_tr_pin, INPUT);
    pinMode(but_C, INPUT);
    pinMode(but_Z, INPUT);
    digitalWrite(but_C, HIGH);
    digitalWrite(but_Z, HIGH);
    myOLED.begin();
    analogReference(DEFAULT);
    draw();
    on_disconnect();
}

const double k_rpm = 3.6*0.083*3.1415/7./3./60.;
const double k_r_to_km = 0.001*0.083*3.1415/7/3./3.;

const unsigned char req_set_current = 1;
const unsigned char req_get_info = 2;
const unsigned char req_check_channel = 3;

const unsigned char ans_error = 1;
const unsigned char ans_values = 2;
const unsigned char ans_error_2 = 3;
const unsigned char ans_check_channel = 4;

const uint8_t drive_mode = 0;
const uint8_t current_info_mode = 1;
const uint8_t n_modes = 2;
uint8_t mode = 0;

const unsigned long wi_timeout = 20000;

double map_gas(int x) {
    const int low_idle = 500;
    const int high_idle = 515;
    const double max_current = 60;
    const double max_break = 50;
    if (x < low_idle) {
        return static_cast<double>((x - low_idle))/low_idle * max_break;
    } else if (x > high_idle) {
        return static_cast<double>((x - high_idle))/(1023 - high_idle) * max_current;
    } else {
        return 0;
    }
}

struct t_current_info {
    double v_in;
    int32_t vel;
//    double vel;
    double ah;
    double ah_charged;
    double tahometer;
        } current_info;

double read_v_tr() {
    int N = 3;
    for (uint8_t i = 0; i < N-1; ++i) {
        vs_tr[i] = vs_tr[i + 1];
    }
    vs_tr[N-1] = analogRead(v_tr_pin);

    int S = 0;
    for (uint8_t i = 0; i < N; ++i) { 
        S += vs_tr[i];
    }
   
    return static_cast<double>(S)/10/1023.*2.*3.3;
}


uint8_t screen_i = 0;
void draw() {
    double v_tr = read_v_tr();

    myOLED.clrScr(); // Стираем все с экрана
    if (mode == drive_mode) {
        myOLED.setFont(SmallFont);
        myOLED.print("Drive mode", CENTER, 10);
        myOLED.print("look at the road", CENTER, 30);
        if (is_connected) {
           myOLED.print("connected", LEFT, 50);
           myOLED.printNumI(channels[i_channel], RIGHT, 50);
        } else {
           myOLED.print("NOT CONNECTED!", CENTER, 50);
        }
    } else if (mode == current_info_mode) {
        if (is_connected) {
            // print velocity
            myOLED.setFont(BigNumbers);
            myOLED.printNumF(current_info.vel, 1, RIGHT, 20);
        
            myOLED.setFont(SmallFont);
            myOLED.print("km", 50, 21);
            myOLED.drawLine(50, 31, 64, 31);;
            myOLED.print("h", 54, 34);
    
            myOLED.printNumF(v_tr, 2, 10, 20);
            myOLED.print("V tr", 10, 32);
                    
            myOLED.printNumF(current_info.v_in/12, 2, 10, 0);
            myOLED.print("V", 36, 0);
        
            myOLED.printNumF(current_info.tahometer, 2, 65, 0);
            myOLED.print("km", 100, 0);
        
            myOLED.printNumF(current_info.ah, 2, 10, 53);
            myOLED.print("ah", 40, 53);
        
            myOLED.printNumF(current_info.ah_charged, 2, 65, 53);
            myOLED.print("ah ch", 94, 53);
        } else {
           myOLED.print("NOT CONNECTED!", CENTER, 25);
           myOLED.print("NOT CONNECTED!", CENTER, 50);
        }
    }
    
    myOLED.update(); // Обновляем информацию на дисплее    
}

void findFreeChannel()
{
    bool ok = false;
    while(!ok) {
        radio.stopListening();
//        radio.setChannel(channels[i_channel]);
        out_data[0] = req_check_channel;
        if (radio.write(&(out_data[0]), payload_size)) {
            radio.startListening();
            bool timeout = false;
            unsigned long started_waiting_at = micros();
            while ((!radio.available()) && (!timeout)) {
                if ((micros() - started_waiting_at) > wi_timeout) {
                    timeout = true;
                }
            }
            if (!timeout) {
                in_data[0] = 0;
                radio.read(&(in_data[0]), payload_size);
                if (in_data[0] == ans_check_channel) {
                    return;                    
                }
            }
        }
        i_channel = ((i_channel + 1) % nChannels);
    }
}

void set_connected(bool c) {
    if (c != is_connected) {
        is_connected = c;
        draw();
    }
}

void on_disconnect() {
    set_connected(false);
    findFreeChannel();
    set_connected(true);    
}


uint16_t i = 0;
bool C_state = false;
bool Z_state = false;
void loop()
{
    bool ok;

    // read gas
    int raw_gas = analogRead(gas);
    double current = map_gas(raw_gas);
    
    radio.stopListening();

    out_data[0] = req_set_current;
    int16_t current_16 = current*100;
    save_16(out_data + 1, current_16);
    ok = radio.write(&(out_data[0]), payload_size);

    if (ok) { set_connected(true); /*Serial.println("write ok.");*/ }
    else    { on_disconnect(); Serial.println("write failed."); }

    radio.startListening();

    if ((mode != drive_mode) && (i % 5 == 0)) {
    
        radio.stopListening();
    
        out_data[0] = req_get_info;
        bool ok = radio.write(&(out_data[0]), payload_size);
    
        if (ok) { set_connected(true); /*Serial.println("write ok.");*/ }
        else    { on_disconnect(); Serial.println("write failed."); }
    
        radio.startListening();
    
        bool timeout = false;
        unsigned long started_waiting_at = micros();
        while ((!radio.available()) && (!timeout)) {
            if ((micros() - started_waiting_at) > wi_timeout) {
                timeout = true;
            }
        }
    
        if (timeout) {
            on_disconnect();
            Serial.println("wi timeout!");
        } else {
            set_connected(true);
            for (uint8_t i = 0; i < payload_size; ++i) { in_data[i] = 0; }
            radio.read(&(in_data[0]), payload_size);
            unsigned char what = in_data[0];
            if (what == ans_values) {
                current_info.v_in = read_u16(in_data + 1) / 100.;
                current_info.vel = read_32(in_data + 3) * k_rpm;
                current_info.ah = read_u16(in_data + 8) / 100.;
                current_info.ah_charged = read_u16(in_data + 10) / 100.;
                current_info.tahometer = read_u32(in_data + 12) * k_r_to_km;

                draw();
            } else if (what == ans_error) {
                Serial.println("Error reading values 1!");
            } else if (what == ans_error_2) {
                Serial.println("Error reading values 2!");
            } else {
                Serial.print("Wireless error! : ");
                Serial.println(static_cast<int>(what));
            }
        }
    }

    // read buttons
    bool C = !digitalRead(but_C);
    bool Z = !digitalRead(but_Z);
    if (C && !C_state) {
        mode = (mode + 1) % n_modes;
        draw();
    }
    C_state = C;
    Z_state = Z;
    
    delay(20);
    ++i;
}

