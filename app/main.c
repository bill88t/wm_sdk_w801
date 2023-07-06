#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "wm_type_def.h"
#include "wm_cpu.h"
#include "wm_mem.h"
#include "wm_uart.h"
#include "wm_gpio.h"
#include "wm_gpio_afsel.h"

#define RX_BUF_SIZE           1024
#define MSG_BUF_SIZE          1024
#define QUEUE_SIZE            32
#define UART_TAST_STK_SIZE    1024
#define MSG_OPEN_UART         8
#define MSG_UART_RECEIVE_DATA 4

typedef struct uart_t { // UART Type
    tls_os_queue_t *uart_q;
    int bandrate;
    TLS_UART_PMODE_T parity;
    TLS_UART_STOPBITS_T stopbits;
    char *rx_buf;
    int rx_msg_num;
    int rx_data_len;
} uart_t;

// External functions that need definition here.
extern s16 uart_tx_sent_callback(struct tls_uart_port *port);
extern void mdelay(uint32_t ms);

// Some static data.
static OS_STK uart_task_stk[UART_TAST_STK_SIZE];
static uart_t *uart = NULL;
static char *text_buf = NULL; // Stores line to use.
static int text_buf_len = 0;

// Function definitions.
static void shell_task(void *sdata); // The actual function that is used.
static s16 uart_rx(u16 len); // Used for recv.
static void termline(void);
static void printp(void* msg, int len);

// Onboard led pins.
char pins[] = {WM_IO_PB_05, WM_IO_PB_25, WM_IO_PB_26, WM_IO_PB_18, WM_IO_PB_17, WM_IO_PB_16, WM_IO_PB_11};

u8 status = 0; // Global status identifier for user access.

static int CreateShellTask(int bandrate, int parity, int stopbits){ // Use this to schedule it.
    if (uart == NULL) {
        uart = tls_mem_alloc(sizeof(uart_t));
        if (uart == NULL) {
            printf("\nmem error\n");
            return WM_FAILED;
        }
        memset(uart, 0, sizeof(uart_t)); // Zero it out.

        uart->rx_buf = tls_mem_alloc(RX_BUF_SIZE + 1); // Allocate rx buffer.
        text_buf = tls_mem_alloc(MSG_BUF_SIZE + 1); // Allocate line storage.
        if (NULL == uart->rx_buf){
            tls_mem_free(uart);
            uart = NULL;
            printf("\nmem error\n");
            return WM_FAILED;
        }

        tls_os_queue_create(&(uart->uart_q), QUEUE_SIZE);
        tls_os_task_create(
            NULL,
            NULL,
            shell_task,
            (void *) uart,
            (void *) uart_task_stk,
            UART_TAST_STK_SIZE,
            32,
            0
        );
    }

    if (-1 == bandrate) bandrate = 115200;
    if (-1 == parity) parity = TLS_UART_PMODE_DISABLED;
    if (-1 == stopbits) stopbits = TLS_UART_ONE_STOPBITS;

    uart->bandrate = bandrate;
    uart->parity = (TLS_UART_PMODE_T) parity;
    uart->stopbits = (TLS_UART_STOPBITS_T) stopbits;
    uart->rx_msg_num = 0;
    uart->rx_data_len = 0;

    tls_os_queue_send(uart->uart_q, (void *) MSG_OPEN_UART, 0);

    return WM_SUCCESS;
}

static s16 uart_rx(u16 len){
    if (uart == NULL) return WM_FAILED;

    uart->rx_data_len += len;
    if (uart->rx_msg_num < 3){
        uart->rx_msg_num++;
        tls_os_queue_send(uart->uart_q, (void *) MSG_UART_RECEIVE_DATA, 0);
    }

    return WM_SUCCESS;
}

static void shell_task(void *sdata) {
    uart_t *uart = (uart_t *) sdata;
    tls_uart_options_t opt;
    void *msg;
    int ret = 0;
    int len = 0;
    int rx_len = 0;

    while (1) {
        tls_os_queue_receive(uart->uart_q, (void **) &msg, 0, 0);
        switch ((u32) msg) {
            case MSG_OPEN_UART:
            {
                opt.baudrate = uart->bandrate;
                opt.paritytype = uart->parity;
                opt.stopbits = uart->stopbits;
                opt.charlength = TLS_UART_CHSIZE_8BIT;
                opt.flow_ctrl = TLS_UART_FLOW_CTRL_NONE;

                wm_uart1_rx_config(WM_IO_PB_19);
                wm_uart1_tx_config(WM_IO_PB_20);

                if (WM_SUCCESS == tls_uart_port_init(TLS_UART_0, &opt, 0)){
                    printf("Shell: Registered uart0\n");
                    termline();
                } else {
                    printf("Shell: Fail\n");
                }

                tls_uart_rx_callback_register((u16) TLS_UART_0, (s16(*)(u16, void*))uart_rx, NULL);
                tls_uart_tx_callback_register(TLS_UART_0, (s16(*)(struct tls_uart_port *))uart_tx_sent_callback);
            }
            break;

            case MSG_UART_RECEIVE_DATA:
                {
                    rx_len = uart->rx_data_len;
                    while (rx_len > 0) {
                        len = (rx_len > RX_BUF_SIZE) ? RX_BUF_SIZE : rx_len;
                        memset(uart->rx_buf, 0, (RX_BUF_SIZE + 1));
                        ret = tls_uart_read(TLS_UART_0, (u8 *) uart->rx_buf, len);  /* input */
                        if (ret <= 0) {
                            break;
                        }
                        rx_len -= ret;
                        uart->rx_data_len -= ret;
                        int i;
                        for (i=0;i<len;i++){
                            if ((u8) *(uart->rx_buf+i) == 127) {
                                // Backspace
                                if (text_buf_len) {
                                    text_buf_len--;
                                    printf("\010 \010");
                                    fflush(stdout);
                                }
                            } else if ((u8) *(uart->rx_buf+i) == 13){
                                // Enter
                                printf("\n");
                                if (text_buf_len){
                                    char statset[] = "st";
                                    if (memcmp(text_buf, statset, sizeof(statset)-1) == 0) {
                                        if ((text_buf_len-sizeof(statset)-2 >= 0) &&
                                            ((u8) *(text_buf+sizeof(statset)-1) == 32)
                                        ){
                                            // has space
                                            u8 diff = text_buf_len-sizeof(statset)-2;
                                            printf("diff=%d\n", diff);
                                            fflush(stdout);
                                            if (diff) {
                                                printf("Set_status: ");
                                                fflush(stdout);
                                                //printp(text_buf+sizeof(statset)+1, diff);
                                                printf("\n");
                                            } else {
                                                printf("Set_status: Error.\n");
                                                fflush(stdout);
                                            }
                                        } else {
                                            printf("Get_status: %d\n", status);
                                            fflush(stdout);
                                        }
                                    } else {
                                        printf("Error: Unknown command.\n");
                                    }
                                    fflush(stdout);
                                    printp(text_buf, text_buf_len);
                                }
                                text_buf_len = 0;
                                termline();
                            }
                            else if (text_buf_len < MSG_BUF_SIZE) {
                                memcpy(text_buf+text_buf_len, uart->rx_buf, 1);
                                text_buf_len++;
                                printp(uart->rx_buf+i, 1);
                                //printf("%d", (char) *(uart->rx_buf+i));
                            } else break; // Buf full
                        }
                    }
                    if (uart->rx_msg_num > 0) {
                        uart->rx_msg_num--;
                    }
                }
                break;

            default:
                break;
        }
    }
}

static void termline(void){
    printf("\033[2K\033[0G[");
    if (status < 10) {
        printf("  %d", status);
    } else if (status < 99) {
        printf(" %d", status);
    } else {
        printf("%d", status);
    }
    printf("]> ");
    fflush(stdout);
}

static void printp(void* msg, int len){
    tls_uart_write(TLS_UART_0, msg, len);
}

void ledsetup(void){ // Setup led pins
    for (int i=0;i<7;i++){
        tls_gpio_cfg(pins[i], WM_GPIO_DIR_OUTPUT, WM_GPIO_ATTR_FLOATING);
    }
}

void ledtest(void){ // Led test
    tls_gpio_write(pins[0], 0);
    tls_gpio_write(pins[6], 0);
    mdelay(50);
    tls_gpio_write(pins[0], 1);
    tls_gpio_write(pins[6], 1);
    tls_gpio_write(pins[1], 0);
    tls_gpio_write(pins[5], 0);
    mdelay(50);
    tls_gpio_write(pins[1], 1);
    tls_gpio_write(pins[5], 1);
    tls_gpio_write(pins[2], 0);
    tls_gpio_write(pins[4], 0);
    mdelay(50);
    tls_gpio_write(pins[2], 1);
    tls_gpio_write(pins[4], 1);
    tls_gpio_write(pins[3], 0);
    mdelay(50);
    tls_gpio_write(pins[3], 1);
    tls_gpio_write(pins[2], 0);
    tls_gpio_write(pins[4], 0);
    mdelay(50);
    tls_gpio_write(pins[2], 1);
    tls_gpio_write(pins[4], 1);
    tls_gpio_write(pins[1], 0);
    tls_gpio_write(pins[5], 0);
    mdelay(50);
    tls_gpio_write(pins[1], 1);
    tls_gpio_write(pins[5], 1);
    tls_gpio_write(pins[0], 0);
    tls_gpio_write(pins[6], 0);
    mdelay(50);
    tls_gpio_write(pins[0], 1);
    tls_gpio_write(pins[6], 1);
}

void UserMain(void){ // This is the main task
    ledsetup();
    printf("Init: START\n");
    ledtest();
    printf("Init: SHELL\n");
    CreateShellTask(-1, -1, -1);
    printf("Init: END\n");
}

