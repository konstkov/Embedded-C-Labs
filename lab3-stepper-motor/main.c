//
// Created by Konstantin Kovalev on 9.2.2026.
//
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define DELAY 1
#define LONG_DELAY 1000
#define STR_LENGTH 256

#define A 13
#define B 6
#define C 3
#define D 2

#define Opt 28

void init_pins();

void rotate_motor();

void run_steps(int count);

bool read_input(char *str, int max_len);

void action_control();

// Motor pins
const uint8_t pins[4] = {D, C, B, A};
// Motor steps
const uint8_t steps[8] = {0b1000, 0b1100, 0b0100, 0b0110, 0b0010, 0b0011, 0b0001, 0b1001};

int main() {
    stdio_init_all();
    sleep_ms(LONG_DELAY); // wait for USB to enumerate

    printf("Boot complete!\n> ");
    fflush(stdout);
    sleep_ms(LONG_DELAY);
    printf("Please type 'calib' to perform calibration,\n followed by run N (N optional) or 'status'\n");

    init_pins();
    action_control();
}

//Initialize pins

void init_pins()
{
    gpio_init(Opt);
    gpio_set_dir(Opt, GPIO_IN);
    gpio_pull_up(Opt);

    for (int i = 0; i < 4; ++i) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
    }
}

void rotate_motor() {
    static int phase = 0;
    phase = (phase + 1) % 8;

    for (int i = 0; i < 4; ++i) {
        bool on = (steps[phase] >> i) & 1;
        gpio_put(pins[i], on);
    }
    sleep_ms(DELAY);
}

void run_steps(int count) {
    while (count-- > 0) {
        rotate_motor();
    }
}

// Reads a line of input non-blocking using getchar_timeout_us
// Returns true if a full line was entered
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

void action_control() {
    char user_input[STR_LENGTH];
    long int step_count = 0;
    long int avg_steps = 0;
    bool calib_status = false;

    while (true) {
        // Poll USB input
        if (read_input(user_input, STR_LENGTH)) {
            user_input[strcspn(user_input, "\n")]='\0';
            char *endPtr;
            char *inputPtr=user_input;
            bool not_int=false;
            inputPtr=user_input;
            fflush(stdout);
            bool sensor_detected=false;

            // Calibration

            if (strcmp(user_input, "calib") == 0) {
                step_count = 0;
                    while (!gpio_get(Opt)) //wait until rising edge
                    {
                        rotate_motor() ;
                    }
                    while (gpio_get(Opt)) //detect falling edge
                    {
                        rotate_motor();
                    }

                printf("Optical sensor activated, doing 3 full revolutions\n");
                fflush(stdout);
                sleep_ms(LONG_DELAY); // sleep a bit between finding 0 position and doing 3 full revs

                    for (int i = 0; i < 3; ++i)
                    {
                        while (!gpio_get(Opt))
                        {
                            rotate_motor(); //as long as motor near 0 position and block the light of the sensor rotate it to move it out of that position
                            ++step_count;
                        }
                        while (gpio_get(Opt))
                        {
                            rotate_motor(); //after that continue rotating until the optical sensor is triggered again; after that, next for loop iteration executes
                            ++step_count;
                        }
                    }

                avg_steps = (long int)round(step_count / 3.0);
                calib_status = true;
                printf("Calibration complete! Steps per revolution: %ld\n", avg_steps);
                user_input[0]='\0';
            }

            // Run command
            /*if (strncmp (user_input, "run", 3)==0)
            {
                run_steps(avg_steps / 8);
                printf("Motor run 1 time.\n");
                user_input[0]='\0';
                continue;
            }*/

            else if (strncmp(user_input, "run ", 4)==0 || strcmp(user_input, "run") == 0) {
                //while (*inputPtr && !isdigit((unsigned char)(*inputPtr))) ++inputPtr;
                long int N=0;
                if (strcmp(user_input, "run")!=0)
                {
                    inputPtr=user_input+4;
                    N=strtol(inputPtr, &endPtr, 10);
                    while (*inputPtr!='\0')
                    {
                        if (!isdigit((unsigned char)(*inputPtr)))
                        {
                            not_int=true;
                        }
                        ++inputPtr;
                    }
                    if (not_int)
                    {
                        printf("Invalid input!\n");
                        continue;
                    }

                    if (!N)
                    {
                        printf("Invalid input!");
                        continue;
                    }
                }


                if (!avg_steps) {
                    printf("Motor not calibrated! Enter 'calib' first.\n");
                } else {
                    int times = (N > 0) ? N : 8;
                    run_steps(avg_steps / 8 * times);
                    printf("Motor run %d times.\n", times);
                    user_input[0]='\0';
                }
            }
            // Status command
            else if (strcmp(user_input, "status") == 0) {
                if (calib_status) {
                    printf("Calibrated. Steps per revolution: %ld\n", avg_steps);
                } else {
                    printf("Not calibrated.\n");
                    user_input[0]='\0';
                }
            }
            else if (strcmp(user_input, "stop") == 0) //stop the program
            {
            continue;
            }
            // Invalid command
            else {
                printf("Invalid input!\n");
            }
        }
    }
}


