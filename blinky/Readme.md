## 1. Phân Tích Sơ Đồ Nguyên Lý & Bản Đồ Chân (Hardware Mapping)

Để điều khiển thiết bị ngoại vi, bước đầu tiên và quan trọng nhất là xác định chính xác mối liên kết giữa chân cắm vật lý và khối quản lý trong ruột chip xử lý (SoC).

- **Vị trí cắm dây vật lý:** Thao tác thực tế kết nối đèn LED vào cụm ghim mở rộng **J18 (EXP GPIO)** tại vị trí **Pin 11**.
    
- **Tra cứu sơ đồ mạch (Schematic):** * Tại Header J18, chân số 11 dẫn ra đường tín hiệu có tên mạng là `EXP_GPIO_IO17`.
    
    - Đường mạch này được định tuyến liên kết trực tiếp với nhãn chức năng mạng hệ thống là `UART3_RTS`.
        
- **Tra cứu file gốc cấu hình SoC (`imx8mp-pinfunc.h`):**
    
    - Tên Pad silicon gốc do hãng NXP thiết kế tại vị trí này là **`ECSPI1_SS0`**.
        
    - Khi chân gốc này được chuyển cấu hình sang chế độ GPIO (Chế độ **ALT5**), nó thuộc quyền quản lý của khối **GPIO5, chân số 9**.
        

> **Kết luận phần cứng:** Để điều khiển dòng điện xuất ra **Pin 11**, mã nguồn phần mềm bắt buộc phải tương tác với đích danh chân **`GPIO5_IO09`** (tức là nhóm `&gpio5`, chân số `9`).

## 2. Cấu Hình Cây Thiết Bị (`app.overlay`)

Trong hệ điều hành Zephyr RTOS, file `.overlay` dùng để ghi đè hoặc bổ sung cấu hình phần cứng cho ứng dụng mà không làm ảnh hưởng đến file cấu hình gốc của bo mạch.

Dưới đây là tệp tin `app.overlay` hoàn chỉnh được tối ưu hóa cho bài Blinky tại Pin 11:

DTS

```
#include <zephyr/dt-bindings/gpio/gpio.h>

/ {
	aliases {
		/* Định nghĩa nhãn alias led0 bắt buộc cho mã nguồn main.c của bài blinky */
		led0 = &pin11_blink;
	};

	leds {
		/* Khai báo tương thích với driver quản lý chuỗi LED của Zephyr */
		compatible = "gpio-leds";
		
		pin11_blink: led_1 {
			/* Chỉ định khối GPIO5, chân số 9, tích cực mức thấp (Active Low) */
			gpios = <&gpio5 9 GPIO_ACTIVE_LOW>;
			label = "Pin 11 Blinky";
		};
	};
};

/* Kích hoạt khối ngoại vi GPIO5 hoạt động trong hệ thống */
&gpio5 {
	status = "okay";
};
```

### Giải thích chi tiết mã nguồn Devicetree:

- `#include <zephyr/dt-bindings/gpio/gpio.h>`: Nhúng tệp định nghĩa các hằng số logic cấu hình chân (như `GPIO_ACTIVE_LOW`).
    
- `aliases { led0 = &pin11_blink; };`: Mã nguồn C mặc định của bài blinky (`main.c`) tìm kiếm phần cứng thông qua cái tên đại diện `led0`. Dòng này giúp ánh xạ tên `led0` vào cấu hình chân vật lý cụ thể phía dưới.
    
- `gpios = <&gpio5 9 GPIO_ACTIVE_LOW>;`: Thiết lập chân điều khiển. Sử dụng cờ hiệu `GPIO_ACTIVE_LOW` vì dây LED còn lại được treo lên nguồn Dương `3V3` (Pin 1 hoặc 17), đèn sẽ sáng khi chân `GPIO5_IO09` xuất ra mức logic `0` (GND).
    

## 3. Quy Trình Biên Dịch Mã Nguồn (Compilation Process)

Quá trình biên dịch được thực hiện trên môi trường máy chủ Ubuntu, yêu cầu dọn dẹp bộ nhớ đệm cũ để hệ thống nạp lại tệp cấu hình chân mới.

- **Bước 1: Di chuyển vào thư mục dự án và kích hoạt môi trường ảo Python**
    
    Bash
    
    ```
    cd ~/zephyrproject
    source .venv/bin/activate
    cd zephyr/samples/basic/blinky
    ```
    
- **Bước 2: Xóa bỏ dữ liệu biên dịch cũ (Tránh kẹt cache cấu hình vùng nhớ)**
    
    Bash
    
    ```
    rm -rf build
    ```
    
- **Bước 3: Biên dịch ứng dụng cho phân vùng RAM DDR**
    
    Bash
    
    ```
    west build -b imx8mp_evk/mimx8ml8/m7/ddr
    ```
    
    _Tham số `/ddr` cực kỳ quan trọng, nó chỉ định trình liên kết (Linker) biên dịch mã nguồn chạy trên dải địa chỉ RAM công cộng `0x80000000` dành cho lõi M7._
    
- **Bước 4: Cập nhật Firmware vào thẻ nhớ**
    
    Lấy tệp đầu ra tại đường dẫn `build/zephyr/zephyr.bin` dán đè vào thư mục gốc của phân vùng Boot (`332MB`) trên thẻ nhớ MicroSD.
    

## 4. Thiết Lập Kịch Bản Tự Động Khởi Động (U-Boot Autoboot)

Khi bo mạch vừa bật nguồn, lõi xử lý chính (Cortex-A53) sẽ chạy trình khởi động U-Boot trước. Chúng ta cần thiết lập một chuỗi lệnh tự động để U-Boot phân quyền ngoại vi, cấp xung nhịp, nạp file từ thẻ nhớ vào RAM và đánh thức lõi Cortex-M7.

Thao tác thực hiện trên cổng Terminal điều khiển **COM4** (U-Boot):

Plaintext

```
u-boot=> setenv boot_m7_blinky "mw.l 0x303844a0 0x3; mw.l 0x303300bc 0x0; mw.l 0x303300c0 0x0; mw.l 0x303844d0 0x3; mw.l 0x303d05c0 0xff; fatload mmc 1:1 0x80000000 zephyr.bin; bootaux 0x80000000"
u-boot=> setenv bootcmd "run boot_m7_blinky"
u-boot=> saveenv
```

### Giải nghĩa chuỗi lệnh tự động từng bước:

1. `mw.l 0x303844a0 0x3`: Mở xung nhịp (Clock Gating) cho bộ UART4.
    
2. `mw.l 0x303300bc 0x0` & `mw.l 0x303300c0 0x0`: Cấu hình Pinmux cho các chân TX/RX của bộ UART4 xuất tín hiệu Log ra mạch chuyển đổi USB-UART (Cổng Log **COM3**).
    
3. `mw.l 0x303844d0 0x3`: Mở xung nhịp hoạt động cho khối quản lý ngoại vi **GPIO5**.
    
4. `mw.l 0x303d05c0 0xff`: Can thiệp vào bộ kiểm soát miền tài nguyên bảo mật (RDC - Resource Domain Controller), **Hạ cấp bảo mật khối GPIO5** nhằm cho phép lõi phụ Cortex-M7 toàn quyền can thiệp và điều khiển đóng cắt điện áp các chân thuộc khối này.
    
5. `fatload mmc 1:1 0x80000000 zephyr.bin`: Đọc tệp nhị phân hệ điều hành `zephyr.bin` từ phân vùng FAT của thẻ nhớ MicroSD, chép toàn bộ dữ liệu vào vùng RAM trống tại địa chỉ khởi đầu `0x80000000`.
    
6. `bootaux 0x80000000`: Phát lệnh khởi động và chuyển con trỏ lệnh cho lõi Cortex-M7 xử lý trực tiếp tại địa chỉ RAM vừa nạp.
    

## 5. Kết Quả Nghiệm Thu Hệ Thống

Sau khi hoàn tất toàn bộ chuỗi quy trình trên, hệ thống hoạt động khép kín không cần sự can thiệp từ bàn phím máy tính:

- **Bật nguồn bo mạch:** Quá trình đếm ngược kết thúc, U-Boot tự động kích hoạt chuỗi lệnh `boot_m7_blinky`.
    
- **Log hiển thị (Cổng COM3):** Tuôn ra các dòng thông tin thông báo hệ điều hành Zephyr RTOS đã khởi chạy thành công và đang thực hiện vòng lặp thay đổi trạng thái chân.
    
- **Trạng thái LED vật lý (Pin 11):** Đèn LED nhấp nháy đều đặn theo chu kỳ được định sẵn trong mã nguồn (mặc định 1 giây), xác nhận chuỗi liên kết từ cấu hình phần mềm đến mạch điện vật lý hoàn toàn chính xác.
