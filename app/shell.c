#define RX_BUF 256
#define MSG_BUF 256

void CreateShellTask(void){
    tls_os_queue_create(&demo_q, DEMO_QUEUE_SIZE);
    tls_os_task_create(NULL, NULL,
                       demo_console_task,
                       NULL,
                       (void *)DemoTaskStk,          /* task's stack start address */
                       DEMO_TASK_SIZE * sizeof(u32), /* task's stack size, unit:byte */
                       DEMO_TASK_PRIO,
                       0);
}
