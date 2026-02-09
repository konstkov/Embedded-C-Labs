//
// Created by Konstantin Kovalev on 30.10.2025.
//

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "pico/util/queue.h"
#include "pico/time.h"

#define QUEUE_COUNT 50
#define WRAP_VALUE 999
#define CC_HIGH 1000
#define CC_LOW 0
#define SLEEP_TIME 30
#define SMOOTHNESS 1
#define ROTA 10
#define ROTB 11
#define ROT_SW 12
#define CLOCK_DIV 125
#define STEP 32
#define CONVERSION 1000

absolute_time_t timestamp();

bool debounce(int *count, absolute_time_t * stamp);

bool debounce2(uint pin);

void ISR (uint gpio, uint32_t event_mask);

static queue_t events;

int main(void)
{
    stdio_init_all();

    //create variables storing LEDS' corresponding pins

    const uint led_pin1 = 20;
    const uint led_pin2 = 21;
    const uint led_pin3 = 22;

    //set the function of LEDS to PWM

    gpio_set_function(led_pin1, GPIO_FUNC_PWM);
    gpio_set_function(led_pin2, GPIO_FUNC_PWM);
    gpio_set_function(led_pin3, GPIO_FUNC_PWM);

    //assign slices to LED pins

    uint slice_num1 = pwm_gpio_to_slice_num(led_pin1);
    uint slice_num2 = pwm_gpio_to_slice_num(led_pin2);
    uint slice_num3 = pwm_gpio_to_slice_num(led_pin3);

    //assign the wrap(top) value after which the value wraps to zero

    pwm_set_wrap(slice_num1, WRAP_VALUE);
    pwm_set_wrap(slice_num2, WRAP_VALUE);
    pwm_set_wrap(slice_num3, WRAP_VALUE);

    //assign channels to LED pins

    uint channel_num1 = pwm_gpio_to_channel(led_pin1);
    uint channel_num2 = pwm_gpio_to_channel(led_pin2);
    uint channel_num3 = pwm_gpio_to_channel(led_pin3);

    //call pwm_set_enabled to start PWM

    pwm_set_enabled(slice_num1, true);
    pwm_set_enabled(slice_num2, true);
    pwm_set_enabled(slice_num3, true);

    // assign corresponding numbers to rot pins

    const uint rota=ROTA;
    const uint rotb=ROTB;
    const uint rot_SW=ROT_SW;

    //create pull up resistor for rot_SW only(reads 0 when pressed)

    gpio_pull_up(rot_SW);

    //Set the clock divider

    pwm_config pwm_config=pwm_get_default_config();
    pwm_config_set_clkdiv_int(&pwm_config,CLOCK_DIV);


    int duty=0; //set the duty cycle value
    int temp=CC_HIGH;
    bool on_state=false;


    //best_effort_wfe_or_timeout();

    //define rotary encoder pins as input pins

    gpio_set_dir(rota, GPIO_IN);
    gpio_set_dir(rotb, GPIO_IN);
    gpio_set_dir(rot_SW, GPIO_IN);

    //irq_set_exclusive_handler();
    queue_init(&events, sizeof(int), QUEUE_COUNT);

    // setting callback function to each pin

    gpio_set_irq_enabled_with_callback (ROT_SW, GPIO_IRQ_EDGE_RISE, true, &ISR);
    gpio_set_irq_enabled_with_callback (ROTA, GPIO_IRQ_EDGE_RISE, true, &ISR);

    //initializing some useful variables

    int value=0;
    int count=0;
    absolute_time_t first_stamp;

    while (true)
    {
    while (queue_try_remove(&events, &value))
        //printf("the event %d has been removed", value);
    {
        if (value==ROT_SW) //means the button was pressed
        {
            if (debounce(&count, &first_stamp)==true)
            {
                while (!gpio_get(ROT_SW)); // wait until the release
                if (on_state==false)
                {
                    duty=temp;
                    on_state=true;
                }
                else   // on_state==true and SW_1 pressed
                {
                    if (duty==0)
                    {
                        duty=CC_HIGH / 2;
                    }
                    else
                    {
                        on_state=false;
                        duty=CC_LOW;
                    }
                }
            }
        }
        if (on_state==true)
        {
                if (value==1)
                { // when user rotates knob clockwise brightness smoothly increases
                    if (duty<CC_HIGH) duty+=STEP;
                }
                if (value==-1)
                { // when user rotates knob counter-clockwise brightness smoothly increases
                    if (duty>CC_LOW) duty-=STEP;
                    if (duty<CC_LOW) //since the step size might be bigger than 1, it might go to negative value and the wrap up going to plus very big number
                    {
                        duty=CC_LOW;
                    }
                }
                temp=duty;
        }
        pwm_set_chan_level (slice_num1, channel_num1, duty); //set the level of the channel
        pwm_set_chan_level (slice_num2, channel_num2, duty); //set the level of the channel
        pwm_set_chan_level (slice_num3, channel_num3, duty); //set the level of the channel
        sleep_ms(SMOOTHNESS); //small delay to adjust the smoothness
    }
    }
    return 0;
}

bool debounce(int *count, absolute_time_t * stamp)
{
    if (*count==0)
    {
        *stamp=timestamp();
        ++*count;
    }
    if (absolute_time_diff_us (*stamp, timestamp())>=CONVERSION * SLEEP_TIME) // actual debounce mechanism
    {
        return true;
    }
    return false;
}

void ISR(uint gpio, uint32_t event_mask)
{
    uint event=0;
    if (gpio==ROTA)
    {
        if (gpio_get(ROTB)) event=-1;
        if (!gpio_get(ROTB)) event=1;
    }
    if (gpio==ROT_SW) event=ROT_SW;
    queue_try_add(&events, &event);
}

absolute_time_t timestamp()
{
    absolute_time_t timestamp=get_absolute_time();
    return timestamp;
}