//
// Created by Konstantin Kovalev on 1.12.2025.
//

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include <ctype.h>

#include "hardware/i2c.h"

#define I2C1_SDA 14
#define I2C1_SCL 15

#define FREQ 9600
#define US_TO_S 1000000

#define SLEEP 5
#define CRC_MSB_INDEX 62
#define LAST_MEM_ADDR 0x7C0
#define LOG_MEM_SIZE 64
#define FIRST_MEM_ADDR 0X0000
#define TERM_NULL 0X00
#define MAX_STR_LEN 62 // 61 chars + terminating null

typedef struct ledstate {
    uint8_t state;
    uint8_t not_state;
} ledstate;

//global variables

uint led1=20;
uint led2=21;
uint led3=22;

uint SW_0=7;
uint SW_1=8;
uint SW_2=9;

uint8_t device_addr=0X50;

typedef enum {
    bootScan,
    erase,
    write,
    read,
    userInput
    } eeprom_st;

typedef struct eeprom_sm {
    eeprom_st state;
    uint16_t mem_address;
    bool boot;
} eeprom_sm;

// Part 1

bool debounce(uint pin);

void set_led_state(ledstate *ls, uint8_t value);

void led_logic(bool init, uint64_t power_up, const uint16_t * mem_address);

void print_states(uint64_t power_up, ledstate * structs_);

bool led_state_is_valid(ledstate *ls);

// Part 2

bool read_input(char *str, int max_len);

void init(void);

static inline void add_mem_addr(uint8_t *buffer, const uint16_t * mem_address);

void eeprom_cmd_sm(eeprom_sm * machine);

void i2c_read(char * str_null_crc, const uint16_t  mem_address);

bool i2c_write(const uint8_t * buffer, size_t total_len);

bool validate_log(const char * str_null_crc);

bool validate_crc(const uint8_t * str_null_crc);

eeprom_st boot_scan_state(uint16_t * mem_address, bool * boot);

eeprom_st erase_state(void);

eeprom_st write_state(uint16_t * mem_address, bool * boot);

eeprom_st read_state(void);

eeprom_st user_input_state(void);

uint16_t calculate_crc(const uint8_t *data_p, size_t length);

int main(void) {
    init();
    eeprom_sm machine = { .state=bootScan, .mem_address = FIRST_MEM_ADDR, .boot = false };
    while (true)     // Loop forever
    {
        eeprom_cmd_sm(&machine);
        sleep_ms(SLEEP);
    }

    return 0;
}

void eeprom_cmd_sm(eeprom_sm * machine)
{
    switch (machine->state)
    {
        case bootScan:
        {
            machine->state=boot_scan_state(&machine->mem_address, &machine->boot);
            break;
        }
        case erase:
        {
            machine->state=erase_state();
            break;
        }
        case write:
        {
            machine->state=write_state(&machine->mem_address, &machine->boot);
            break;
        }
        case read:
        {
            machine->state=read_state();
            break;
        }
        case userInput:
        {
            machine->state=user_input_state();
            break;
        }
    }
}

void i2c_read(char * str_null_crc, const uint16_t  mem_address)
{
    uint8_t mem_addr_buff[2];
    add_mem_addr(mem_addr_buff, &mem_address);
    //then send the internal memory address AGAIN (with repeated start)

    i2c_write_blocking(i2c1 ,device_addr, mem_addr_buff, 2, true);

    //then finally read

    i2c_read_blocking(i2c1, device_addr, (uint8_t*)str_null_crc, LOG_MEM_SIZE, false);
}

bool i2c_write(const uint8_t * buffer, size_t total_len)
{
    int retries=5;
    int ack=0;

    while (retries--)
    {
        ack=i2c_write_blocking(i2c1, device_addr, buffer, total_len, false);
        if (ack>=0) return true; //success
        sleep_ms(SLEEP);
    }
    return false;
}

bool validate_log(const char * str_null_crc)
{
    const char * null_index_ptr=strchr(str_null_crc, TERM_NULL); // look for terminating null
    if (null_index_ptr==NULL) return false; // no terminating null - invalid string
    int null_index = null_index_ptr - str_null_crc; // since str_null_crc points to 0 index to get the index as a number, we subtract each pointer;

    if (str_null_crc[0] != TERM_NULL && null_index < CRC_MSB_INDEX) //if 1st char is not 0 & there's '\0' before index 62?
    {
        if (validate_crc((const uint8_t*)str_null_crc)) return true;
    }

   return false;
}

uint16_t calculate_crc(const uint8_t *data_p, size_t length)
{
    uint8_t x;
    uint16_t crc = 0xFFFF;
    while (length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t) (x << 12)) ^ ((uint16_t) (x << 5)) ^ ((uint16_t) x);
    }
    return crc;
}

bool validate_crc(const uint8_t * str_null_crc)
{
    // validate data
    size_t len = strlen((char*)str_null_crc) + 1 + 2; //string + null + 2 byte CRC
    if (len > LOG_MEM_SIZE) return false;
    return calculate_crc((const uint8_t*)str_null_crc, len) == 0;
}

eeprom_st boot_scan_state(uint16_t * mem_address, bool * boot)
{
    char buffer[LOG_MEM_SIZE];
    i2c_read(buffer, *mem_address); // read current memory log

    if (*mem_address==LAST_MEM_ADDR) // is curr.slot last_slot?
    {
        if (validate_log(buffer)) //is it valid?
        {
            return erase;
        }
        else
        {
            *boot=true; // for printing "Boot" message in write state
            printf("Boot.\n");
            return write;
        }
    }
    if (validate_log(buffer))
    {
        *mem_address+=LOG_MEM_SIZE; //if curr.slot is not the last slot
        return bootScan;
    }
    else
    {
        *boot=true;
        return write; // write "Boot";
    }

}

eeprom_st write_state(uint16_t * mem_address, bool * boot)
{
        //char buffer[LOG_MEM_SIZE];
        char boot_str[5]="Boot";
        char test_str[5]="Test";
        uint8_t buffer[LOG_MEM_SIZE];
        add_mem_addr(buffer, mem_address); // add current memory address to first 2 indexes of the buffer

        size_t payload_len = 0; //  initialize
        //size_t payload_len = strlen(str) + 1; //  the str + '\0'
        size_t str_len = 0; // initialize

        if (*boot)
        {
            str_len = strlen(boot_str);
            payload_len = strlen(boot_str) + 1; //  the str + '\0'
            memcpy(&buffer[2], boot_str, str_len);
            *boot=false;
        }
        else
        {
            str_len = strlen(test_str);
            payload_len = strlen(test_str) + 1; //  the str + '\0'
            memcpy(&buffer[2], test_str, str_len);
        }

        size_t total_len = 2 + payload_len + 2; // total len = payload_len + 2 (2 crc bytes)

        /*
        size_t str_len = *boot ? strlen(boot_str) : strlen(test_str);
        memcpy(&buffer[2], *boot ? boot_str : test_str, str_len);
        //size_t str_len = *boot ? strlen(boot_str) : strlen(str); // decide the size of the string
        //memcpy(&buffer[2], *boot ? boot_str : str, str_len); // copy either boot string or normal string to the buffer
        */
        buffer[2 + str_len] = '\0'; // add terminating null
        uint16_t crc=calculate_crc(&buffer[2], payload_len); //returns 16 bit (2 byte) CRC   (excluding 1st 2 bytes of mem addr)

        buffer[2 + payload_len]=(uint8_t)(crc >> 8); //which we then append after terminating null (first high byte)
        buffer[2 + payload_len + 1]=(uint8_t)(crc & 0xFF);

        *mem_address+=LOG_MEM_SIZE;

        i2c_write(buffer, total_len);

        return userInput;
}

eeprom_st erase_state(void)
{
    static uint16_t mem_address=FIRST_MEM_ADDR;
    uint8_t buffer[LOG_MEM_SIZE];
    add_mem_addr(buffer, &mem_address); // add current memory address to first 2 indexes of the buffer
    buffer[2] = TERM_NULL; // add terminating null

    if (mem_address < LAST_MEM_ADDR)
    {
        i2c_write(buffer, 3); //write 2 bytes of memory address plus 0 to the first entry of the log
        mem_address += LOG_MEM_SIZE; //advance memory address to the next log (+64 bytes)
        return erase; //continue erasing
    }
    else return userInput; //erasing complete
}

static inline void add_mem_addr(uint8_t *buffer, const uint16_t * mem_address)
{
    buffer[0] = (uint8_t)(*mem_address >> 8); // high byte
    buffer[1] = (uint8_t)(*mem_address & 0XFF); // low byte
}

eeprom_st read_state(void)
{
    static uint16_t mem_address=FIRST_MEM_ADDR;

    char buffer[LOG_MEM_SIZE];

    if (mem_address <= LAST_MEM_ADDR)
    {
        i2c_read(buffer, mem_address);
        if (validate_log(buffer))
        {
            printf("Log entry: %s. Memory address: 0X%02X\n", buffer, mem_address);
            mem_address+=LOG_MEM_SIZE; //advance memory address to the next log (+64 bytes)
        }
        else
        {
            printf("No valid log entries found!\n");
            mem_address = FIRST_MEM_ADDR;
            return userInput; // reading complete
        }
    }
    return read;
}

eeprom_st user_input_state(void)
{
    printf("Type 'erase' to erase EEPROM, 'write' to write or 'read' to read every valid entry:\n");
    char user_input[MAX_STR_LEN];

    while (!read_input(user_input, MAX_STR_LEN));

    if (!strcmp(user_input, "erase"))
    {
        printf("Erasing EEPROM...\n");
        return erase;
    }
    else if (!strcmp(user_input, "read"))
    {
        printf("Reading EEPROM...\n");
        return read;
    }
    else if (!strcmp(user_input, "write"))
    {
        printf("Writing to EEPROM...\n");
        return write;
    }
    else if (user_input[0]=='\0')
    {
        return userInput;
    }
    else
    {
        printf("Invalid input!\n");
        return userInput;
    }
}

bool read_input(char *str, int max_len) {
    static int pos = 0;
    int c = getchar_timeout_us(0); // non-blocking read
    if (pos==0) str[0]='\0'; // empty the string before writing the new one
    if (c == PICO_ERROR_TIMEOUT) {
        return false; // no input yet
    }

    if (c == '\r' || c == '\n') {
        str[pos] = '\0';
        pos = 0; //return to 0 position
        putchar('\n'); // write newline
        fflush(stdout);
        return true; // line complete
    }

    if (pos < max_len - 1) { // if the current index is less than string length minus terminating null continue reading
        str[pos++] = (char)c;
        str[pos] = '\0';
        putchar(c); // write character
        fflush(stdout);
    }
    return false;
}

void init(void)
{
    // set direction of button pins to input

    gpio_set_dir(SW_0, GPIO_IN);
    gpio_set_dir(SW_1, GPIO_IN);
    gpio_set_dir(SW_2, GPIO_IN);

    //create pull up resistor for buttons (reads 0 when pressed)

    gpio_pull_up(SW_0);
    gpio_pull_up(SW_1);
    gpio_pull_up(SW_2);

    // Initialize all the present standard stdio types that are linked into the binary.
    stdio_init_all();

    // initialize i2c
    i2c_init(i2c1, FREQ);

    //create gpio pins
    uint sda=I2C1_SDA;
    uint scl=I2C1_SCL;

    // initialize pins
    gpio_init(sda);
    gpio_init(scl);

    gpio_init(led1);
    gpio_init(led2);
    gpio_init(led3);

    //set function/direction of pins
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);

    gpio_set_dir(led1, true);
    gpio_set_dir(led2, true);
    gpio_set_dir(led3, true);

    //create pull up resistors
    gpio_pull_up(sda);
    gpio_pull_up(scl);
}

#if 0
void led_logic(bool init, uint64_t power_up, const uint16_t * mem_address)
{
    static bool on_state1=false;
    static bool on_state2=false;
    static bool on_state3=false;

    static ledstate led_structs[3];

    const uint leds[3]={led1, led2, led3};

    print_states(power_up, led_structs);
    for (int i=0;i<3;++i)
    {
        gpio_put(leds[i], led_structs[i].state);
    }
    print_states(power_up, led_structs);

    if (debounce(SW_0))
    {
        on_state1=!on_state1; //toggle state
        set_led_state(&led_structs[0], on_state1);
        gpio_put(led1, on_state1);
        write_state(led_structs, mem_address, false); // save led state to EEPROM
        print_states(power_up, led_structs);
    }
    if (debounce(SW_1))
    {
        on_state2=!on_state2; //toggle state
        set_led_state(&led_structs[1], on_state2);
        gpio_put(led2, on_state2);
        //i2c_write(led_structs, mem_address); // save led state to EEPROM
        print_states(power_up, led_structs);
    }
    if (debounce(SW_2))
    {
        on_state3=!on_state3; //toggle state
        set_led_state(&led_structs[2], on_state3);
        gpio_put(led3, on_state3);
        //i2c_write(led_structs, mem_address); // save led state to EEPROM
        print_states(power_up, led_structs);
    }
}

bool debounce(uint pin) //simple debounce logic in a separate function (added while statement to wait until user releases the button)
{
    if (!gpio_get(pin)) //if you detect press first wait
    {
        sleep_ms(SLEEP);
        if (!gpio_get(pin)) // if you still detect the press, wait until the button is released
        {
            while (!gpio_get(pin));
            return true; //means the button was pressed
        }
    }
    return false;
}

void set_led_state(ledstate *ls, uint8_t value)
{
    ls->state = value;
    ls->not_state = ~value;
}

bool led_state_is_valid(ledstate *ls) {
    return ls->state == (uint8_t) ~ls->not_state;
}

void print_states(uint64_t power_up, ledstate * structs)
{
    uint64_t since_boot_time=(time_us_64() - power_up) / US_TO_S;
    printf("%llu Seconds since power up.\n", since_boot_time);
    for (int i=0;i<3;++i)
    {
        printf("The state of LED%d: %d\n", i+1, structs[i].state);
    }
}
#endif










