# BÁO CÁO KỸ THUẬT: HIỆN THỰC GIAO THỨC UART3 ECHO ĐỘC LẬP TRÊN LÕI PHỤ CORTEX-M7 (ZEPHYR OS)

---

## 1. Cấu hình File Cấu hình Phần cứng (`app.overlay`)

Để đảm bảo tính toàn vẹn của hệ thống và không chiếm dụng tài nguyên của cổng UART4, file `app.overlay` được thiết kế chia làm 4 khối logic độc lập, tường minh:

### Khối 1: Định nghĩa thực thể độc lập (`&{/soc}`)

```dts
&{/soc} {
	uart3: uart@30880000 {
		compatible = "nxp,imx-iuart";
		reg = <0x30880000 0x10000>;
		interrupts = <28 3>;
		clocks = <&ccm IMX_CCM_UART3_CLK 0x6c 24>;
		status = "disabled";
	};
};

```

* **Mục đích:** Khai báo và cấy một nút phần cứng hoàn toàn mới vào cấu trúc bus trung tâm (`/soc`) của vi xử lý i.MX8MP.


* **Chi tiết thành phần:**
* `uart3: uart@30880000`: Đặt nhãn gọi nhanh là `uart3` gắn với địa chỉ gốc vật lý của UART3.


* `compatible = "nxp,imx-iuart"`: Khai báo định danh để hệ điều hành kích hoạt đúng driver mã nguồn điều khiển cấu trúc UART của NXP.


* `reg`: Ánh xạ dải thanh ghi vật lý của khối UART3 từ địa chỉ `0x30880000` với độ rộng bộ nhớ 64KB.


* `interrupts`: Cấp luồng ngắt phần cứng số 28 (ID ngắt của UART3 trên i.MX8MP).


* `clocks`: Đấu luồng xung nhịp gốc vào đúng trục phát `IMX_CCM_UART3_CLK` với tần số nền 24 MHz để triệt tiêu hoàn toàn lỗi lệch tần số bit gây vỡ ký tự.





### Khối 2: Định tuyến chân vật lý ngoài bo mạch (`&pinctrl`)

```dts
&pinctrl {
	uart3_ecspi1_default: uart3_ecspi1_default {
		group0 {
			pinmux = <&iomuxc_ecspi1_sclk_uart_rx_uart3_rx>,
				 <&iomuxc_ecspi1_mosi_uart_tx_uart3_tx>;
			bias-pull-up;
			slew-rate = "slow";
			drive-strength = "x1";
		};
	};
};

```

* **Mục đích:** Cấu hình ma trận chân (Pin Multiplexing) thuộc khối điều khiển IOMUXC của chip i.MX8MP.


* **Chi tiết thành phần:**
* `pinmux`: Tiến hành mượn chân chức năng `ECSPI1_SCLK` bẻ hướng làm đường nhận dữ liệu `UART3_RX`, và chân `ECSPI1_MOSI` làm đường phát dữ liệu `UART3_TX`.


* `bias-pull-up`: Kích hoạt điện trở kéo lên nội bộ cho chân RX nhằm triệt tiêu nhiễu đường truyền khi không có dữ liệu.





### Khối 3: Kích hoạt ngoại vi (`&uart3`)

```dts
&uart3 {
	status = "okay";
	current-speed = <115200>;
	pinctrl-0 = <&uart3_ecspi1_default>;
	pinctrl-names = "default";
};

```

* **Mục đích:** Đóng gói và đưa khối ngoại vi vào trạng thái sẵn sàng hoạt động.


* **Chi tiết thành phần:** Chuyển `status` từ khóa (`disabled`) sang mở (`okay`), thiết lập tốc độ truyền nhận chuẩn 115200 bps và áp đặt cụm chân vật lý đã cấu hình ở Khối 2 vào làm cửa ngõ giao tiếp chính thức.



### Khối 4: Điều hướng luồng hệ thống (`chosen`)

```dts
/ {
	chosen {
		zephyr,app-uart = &uart3;
		zephyr,console = &uart3;
	};
};

```

* **Mục đích:** Chỉ thị cho tầng ứng dụng của hệ điều hành Zephyr bẻ toàn bộ luồng xuất log màn hình (`console`) và luồng giao tiếp dữ liệu (`app-uart`) đổ dồn về thực thể `uart3` độc lập vừa thiết lập.



---

## 2. Các hàm cốt lõi trong Mã nguồn Ứng dụng (`main.c`)

Mã nguồn C thực hiện tác vụ nhận diện ký tự bằng cơ chế Polling (vòng lặp kiểm tra trạng thái liên tục) thông qua các dòng lệnh bản chất sau:

* **`static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_app_uart));`**
* *Ý nghĩa:* Đây là lệnh liên kết tối cao giữa phần mềm và phần cứng. Macro `DT_CHOSEN` đi tìm xem nhãn `zephyr,app-uart` trong overlay đang trỏ vào đâu (đang trỏ vào `&uart3`). Sau đó, `DEVICE_DT_GET` bốc ra cấu trúc driver điều khiển tương ứng và gán vào biến con trỏ `uart_dev`. Dòng này giúp code C độc lập hoàn toàn với phần cứng (Hardware Agnostic).




* **`device_is_ready(uart_dev)`**
* *Ý nghĩa:* Hàm kiểm tra an toàn hệ thống. Nó xác nhận xem driver cấu hình của UART3 đã hoàn tất khởi tạo thành công trong hệ thống hay chưa trước khi cho phép ứng dụng can thiệp.


* **`uart_poll_in(uart_dev, &c)`**
* *Ý nghĩa:* Hàm đọc dữ liệu không chặn (Non-blocking). Hệ thống sẽ liên tục kiểm tra thanh ghi nhận dữ liệu (RX FIFO) của UART3. Nếu bộ đệm trống, hàm trả về giá trị khác 0 và bỏ qua; nếu người dùng gõ một ký tự từ bàn phím, hàm trả về 0 và trích xuất ký tự đó lưu vào biến `c`.




* **`uart_poll_out(uart_dev, c)`**
* *Ý nghĩa:* Hàm phát dữ liệu. Ngay khi biến `c` nhận ký tự mới, hàm này lập tức đẩy ký tự đó vào thanh ghi truyền (TX FIFO) của UART3 để dội ngược dữ liệu hiển thị lên màn hình terminal COM5 của máy tính.





---

## 3. Quy trình Biên dịch (Build Firmware)

Để loại bỏ hoàn toàn hiện tượng kẹt bộ nhớ cache của CMake/Ninja khiến file binary không cập nhật đúng mã máy mới (lỗi đứng im dung lượng file), quy trình biên dịch bắt buộc phải sử dụng tham số làm sạch chuyên dụng (`-p always` / `--pristine`):

```bash
# Di chuyển vào thư mục ứng dụng
cd ~/zephyrproject/uart_echo

# Kích hoạt môi trường ảo chứa West (Nếu chưa kích hoạt)
source ../.venv/bin/activate

# Biên dịch sạch hoàn toàn cho mục tiêu lõi M7
west build -p always -b imx8mp_evk/mimx8ml8/m7 .

```

> **Lưu ý:** Việc sử dụng tham số `-p always` sẽ ép West xóa toàn bộ thư mục `build/` cũ, quét lại toàn bộ file `app.overlay` mới dứt điểm để sinh ra file `zephyr.bin` chuẩn xác nhất.
> 
> 

---

## 4. Kịch bản Tự động Khởi động lõi M7 từ U-Boot (Autoboot Script)

Để hệ thống tự động thiết lập và khởi chạy lõi M7 mỗi khi bật nguồn mà không cần gõ lệnh thủ công, chúng ta sẽ đóng gói toàn bộ quy trình vào các biến môi trường của U-Boot. Phương pháp này sử dụng kỹ thuật nạp trung gian qua RAM rồi sao chép vào bộ nhớ nội bộ TCM để tránh lỗi sập nguồn (RDC crash) khi phân giải file ELF.

Bạn mở cổng **COM4**, bật nguồn bo mạch và nhấn một phím bất kỳ để dừng quá trình boot tự động, truy cập vào dấu nhắc lệnh `u-boot=>`. Tiến hành copy và chạy khối lệnh sau:

```text
u-boot=> setenv mmcdev 1
u-boot=> setenv m7image zephyr.bin
u-boot=> setenv m7loadaddr 0x48000000
u-boot=> setenv m7runaddr 0x7e0000
u-boot=> setenv bootm7 'mmc dev ${mmcdev}; fatload mmc ${mmcdev}:1 ${m7loadaddr} ${m7image}; cp.b ${m7loadaddr} ${m7runaddr} ${filesize}; bootaux ${m7runaddr}'
u-boot=> setenv bootcmd 'run bootm7; run distro_bootcmd'
u-boot=> saveenv

```

### Bản chất hoạt động của Script:

* **Khai báo biến tường minh:** Việc đặt riêng các biến như `mmcdev`, `m7image`, `m7loadaddr` (`0x48000000`), và `m7runaddr` (`0x7e0000`) giúp kịch bản boot trở nên module hóa, dễ dàng thay đổi tên file hoặc địa chỉ nạp khi cần thiết.


* **`mmc dev ${mmcdev}`:** Khởi thám và chuyển sang đúng chỉ số thiết bị thẻ nhớ MMC/SD đang chứa firmware để tránh lỗi nhận diện sai ổ đĩa.


* **`fatload mmc ...`:** Đọc file phẳng `zephyr.bin` từ phân vùng FAT của thẻ nhớ và tải tạm vào địa chỉ RAM trung gian an toàn là `0x48000000`.


* **`cp.b ${m7loadaddr} ${m7runaddr} ${filesize}`:** Thực hiện sao chép từng byte dữ liệu từ RAM vào vùng nhớ nội bộ TCM (`0x7e0000`) của lõi M7 dựa theo đúng dung lượng thực tế của file thông qua biến tự động `${filesize}`. Cơ chế sao chép thô này giúp bypass qua bộ bảo vệ RDC (Resource Domain Controller) vốn là nguyên nhân gây sập nguồn khi ghi trực tiếp từ lệnh phân tích ELF.


* **`bootaux ${m7runaddr}`:** Kích hoạt lõi phụ Cortex-M7 bắt đầu khởi chạy ứng dụng độc lập ngay tại địa chỉ gốc TCM.


* **Tích hợp vào `bootcmd`:** Chèn macro `run bootm7` vào trước lệnh khởi động mặc định `run distro_bootcmd`. Nhờ đó, lõi M7 sẽ luôn được đánh thức trước khi hệ thống tiếp tục tải hệ điều hành của lõi chính A53 một cách song song.


* **`saveenv`:** Lưu vĩnh viễn các cấu hình biến môi trường này vào bộ nhớ Flash/ROM của bo mạch để kịch bản tự chạy ở những lần bật nguồn tiếp theo.
4. **Kích hoạt:** Lệnh `bootaux 0x80000000` lập tức giải phóng trạng thái Reset của lõi Cortex-M7, ép lõi phụ nhảy vào thực thi firmware ứng dụng độc lập. Sau đó, hệ thống tiếp tục trả quyền điều khiển cho lệnh `orig_bootcmd` để lõi chính A53 tải hệ điều hành Linux một cách song song, độc lập và mượt mà.
