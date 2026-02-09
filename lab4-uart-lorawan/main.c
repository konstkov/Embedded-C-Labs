#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "iuart.h"
#include <ctype.h>

#include "pico/util/queue.h"

#define STRLEN 80


// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#if 0
#define UART_NR 0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#else
#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#endif

#define BAUD_RATE 9600
#define BOOT_SLEEP 2000
#define DELAY 2
#define TIMEOUT 500000
#define SLEEP 200
#define STR_LEN 256
#define ASCII_DIFF 32

//give states meaningful names

const uint led_pin = 22;
const uint button = 9;

typedef enum
{
    buttonPress,
    AT,
    firmwareVersion,
    devEui,
    goToStep1
} lora_st;

typedef struct lora_sm
{
    lora_st state;
    uint32_t timer;
} lora_sm;

void remove_colons(const char *str);

bool debounce();

void lora_wan_sm(lora_sm *lora_struct);

bool lora_cmd(const char *cmd, char *response);

int main()
{
    // Initialize LED pin
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    gpio_init(button);
    gpio_set_dir(button, GPIO_IN);
    gpio_pull_up(button);

    // Initialize stdio serial port
    stdio_init_all();

    printf("Boot\r\n");
    sleep_ms(BOOT_SLEEP);

    // setup our own UART
    iuart_setup(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE);

#if 1
   /*for (int i = 0; i < 3; ++i) {
    char response_test[STR_LEN];
    char cmd_test[]="another very long string\r\n";
    lora_cmd(cmd_test, response_test);
    printf("test response!%s\n", response_test);
       if(i==0) sleep_ms(200); // for testing that sending takes place in the background even when we are sleeping
    }*/
#endif

    //declare initial params (first state and timer set to zero):

    lora_sm lora_struct={.state=buttonPress, .timer=0};

    //main loop

    while (true)
    {
        lora_wan_sm(&lora_struct);
        sleep_ms(DELAY);
    }
}
    // main state machine

    void lora_wan_sm(lora_sm *lora_struct)
{
    switch (lora_struct->state)
    {
        case (buttonPress): //State 1
            if (debounce()) lora_struct->state=AT;
            break;

        case (AT): //State 2
            const char send1[] = "AT\r\n";
            char response1[STR_LEN];
            if (lora_cmd(send1, response1)) // if response from LoRaWan received - move to the next state
             {
                sleep_ms(SLEEP);
                 lora_struct->state=firmwareVersion;
             }
            else
            {
                lora_struct->state=buttonPress;
            }
            break;

        case (firmwareVersion): //State 3
            const char send2[] = "AT+VER\r\n";
            char response2[STR_LEN];
            if (lora_cmd(send2, response2)) // if response from LoRaWan received - move to the next state
            {
                sleep_ms(SLEEP);
                lora_struct->state=devEui;
            }
            else
            {
                lora_struct->state=buttonPress;
            }
            break;

        case (devEui): //State 4
            const char send3[] = "AT+ID=DevEui\r\n";
            char response3[STR_LEN];

            if (lora_cmd(send3, response3)) // if response from LoRaWan received - move to the next state
            {
                sleep_ms(SLEEP);
                remove_colons(response3);
                lora_struct->state=goToStep1;
            }
            else
            {
                lora_struct->state=buttonPress;
            }
            break;

        case (goToStep1): //State 5
            lora_struct->state=buttonPress;
            break;
    }
}

bool debounce() //simple debounce logic in a separate function (added while statement to wait until user releases the button)
{
    if (gpio_get(button)==0) //if you detect press first wait
    {
        sleep_ms(DELAY);
        if (gpio_get(button)==0) // if you still detect the press, wait until the button is released
        {
            while (gpio_get(button)==0);
            return true; //means the button was pressed
        }
    }
    return false;
}

bool lora_cmd(const char *cmd, char *response) // handles the char string to iuart.h for further processing
{
    //const char send[] = "at+VER\r\n";
    //const char send[] = "at+ID\r\n";
    //char str[STRLEN];
    int pos = 0;

    for (int i=0; i<5;++i) // try up to 5 times
    {
        uint32_t t = time_us_32();
        iuart_send(UART_NR, cmd); // send the command
        //sleep_ms(200);

        while ((time_us_32() - t) <= TIMEOUT) //compare current time to recorded time stamp and loop until time out is reached
        {
            pos = iuart_read(UART_NR, (uint8_t *) response, STRLEN-1);
            
            if (pos > 0)
            {
                response[pos] = '\0';
                printf("%d, Connected to LoRa module. %s\n", time_us_32() / 1000, response);
                return true;
            }
        }
    }
        printf("Module is not responding!\n");
        return false;
}

void remove_colons(const char *str)
{
    printf("%s\n", str);
    unsigned char c=0; 
    int j=0;
    char output[STR_LEN];
for (int i=0;str[i]!='\0';++i)
{
    c=str[i];
    if (c==':') continue;
    output[j++]=tolower(c);
}
    output[j]='\0';
    printf("%s", output);
}






