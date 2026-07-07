#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_app_uart));

#define MSG_MAX_SIZE 128
static char rx_buf[MSG_MAX_SIZE];
static int rx_buf_pos = 0;

/* Hàm callback xử lý ngắt UART (Chạy độc lập với hàm main) */
static void uart_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

    uart_irq_update(dev);
	/* Kiểm tra xem có phải ngắt do có dữ liệu truyền đến (RX) hay không */
	// if (!uart_irq_update(dev)) {
	// 	return;
	// }

	if (uart_irq_rx_ready(dev)) {
		/* Đọc liên tục toàn bộ ký tự trong hàng đợi phần cứng */
		while (uart_fifo_read(dev, &c, 1) == 1) {
			
			/* Nếu người dùng ấn Enter (\r hoặc \n) -> Kết thúc một tin nhắn (msg) */
			if (c == '\r' || c == '\n') {
				if (rx_buf_pos > 0) {
					rx_buf[rx_buf_pos] = '\0'; // Kết thúc chuỗi C-string
					
					/* In toàn bộ chuỗi msg ra màn hình một lần duy nhất */
					printk("\n[M7 Log] received character successfully: \"%s\"\n", rx_buf);
					
					/* Reset lại vị trí con trỏ để nhận chuỗi tiếp theo */
					rx_buf_pos = 0;
				}
			} 
			/* Nếu là ký tự thường, lưu vào bộ đệm và echo lại màn hình */
			else if (rx_buf_pos < (MSG_MAX_SIZE - 1)) {
				rx_buf[rx_buf_pos++] = c;
				uart_poll_out(dev, c); // Echo ký tự đơn cực nhanh, không gây nghẽn
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

	printk("\r\n============================================\r\n");
	printk("  M7 is ready to receive string...\r\n");
	printk("============================================\r\n\r\n");

	/* 1. Cấu hình hàm callback xử lý ngắt cho bộ UART */
	uart_irq_callback_set(uart_dev, uart_cb);

	/* 2. Kích hoạt ngắt nhận dữ liệu (RX Interrupt) */
	uart_irq_rx_enable(uart_dev);

	/* Vòng lặp main lúc này hoàn toàn rảnh rỗi, không sợ bị block */
	while (1) {
		k_sleep(K_FOREVER); 
	}

	return 0;
}