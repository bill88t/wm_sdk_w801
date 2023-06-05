#include "wm_include.h"
//#include "wm_demo.h"
//#include <string.h>

extern void mdelay(uint32_t ms);
//extern int uart_demo(int bandrate, int parity, int stopbits);

char pins[] = {WM_IO_PB_05, WM_IO_PB_25, WM_IO_PB_26, WM_IO_PB_18, WM_IO_PB_17, WM_IO_PB_16, WM_IO_PB_11};

void ledsetup(void){ // Setup led pins
    for (int i=0;i<7;i++){
        tls_gpio_cfg(pins[i], WM_GPIO_DIR_OUTPUT, WM_GPIO_ATTR_FLOATING);
    }
}

void ledtest(void){ // Led test
    tls_gpio_write(pins[0], 0);
    tls_gpio_write(pins[6], 0);
    mdelay(100);
    tls_gpio_write(pins[0], 1);
    tls_gpio_write(pins[6], 1);
    tls_gpio_write(pins[1], 0);
    tls_gpio_write(pins[5], 0);
    mdelay(100);
    tls_gpio_write(pins[1], 1);
    tls_gpio_write(pins[5], 1);
    tls_gpio_write(pins[2], 0);
    tls_gpio_write(pins[4], 0);
    mdelay(100);
    tls_gpio_write(pins[2], 1);
    tls_gpio_write(pins[4], 1);
    tls_gpio_write(pins[3], 0);
    mdelay(100);
    tls_gpio_write(pins[3], 1);
    tls_gpio_write(pins[2], 0);
    tls_gpio_write(pins[4], 0);
    mdelay(100);
    tls_gpio_write(pins[2], 1);
    tls_gpio_write(pins[4], 1);
    tls_gpio_write(pins[1], 0);
    tls_gpio_write(pins[5], 0);
    mdelay(100);
    tls_gpio_write(pins[1], 1);
    tls_gpio_write(pins[5], 1);
    tls_gpio_write(pins[0], 0);
    tls_gpio_write(pins[6], 0);
    mdelay(100);
    tls_gpio_write(pins[0], 1);
    tls_gpio_write(pins[6], 1);
}

void UserMain(void){ // This is the main task
    ledsetup();
    printf("UserMain: START\n");
    ledtest();
    //printf("UserMain: UART\n");
    //uart_demo(-1, -1, -1);
    //printf("UserMain: DEMO\n");
    //CreateDemoTask();
    printf("UserMain: END\n");
}

