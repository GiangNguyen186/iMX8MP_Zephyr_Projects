#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_app_uart));

#define MSG_MAX_SIZE 128
static char rx_buf[MSG_MAX_SIZE];
static int rx_buf_pos = 0;

static void uart_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

    uart_irq_update(dev);
	// if (!uart_irq_update(dev)) {
	// 	return;
	// }

	if (uart_irq_rx_ready(dev)) {
		while (uart_fifo_read(dev, &c, 1) == 1) {
			
			if (c == '\r' || c == '\n') {
				if (rx_buf_pos > 0) {
					rx_buf[rx_buf_pos] = '\0';
					
					printk("\nM7 Log (COM 5) received character: \"%s\"\n", rx_buf);
					
					rx_buf_pos = 0;
				}
			} 
			else if (rx_buf_pos < (MSG_MAX_SIZE - 1)) {
				rx_buf[rx_buf_pos++] = c;
				uart_poll_out(dev, c);
			}
		}
	}
}

int main(void)
{
	if (!device_is_ready(uart_dev)) {
		printk("Error: UART device is not ready!\n");
		return 0;
	}

	printk("  M7 is ready to receive string...\r\n");

	uart_irq_callback_set(uart_dev, uart_cb);

	uart_irq_rx_enable(uart_dev);

	while (1) {
		k_sleep(K_FOREVER); 
	}

	return 0;
}
