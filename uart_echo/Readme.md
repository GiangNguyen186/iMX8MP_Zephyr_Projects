## Bước 1: Xác định bài toán phần cứng

Mặc định, gói hỗ trợ bo mạch (`imx8mp_evk`) của NXP trên Zephyr chỉ cấu hình sẵn cổng **UART4** làm console để debug cho lõi M7.

Để chuyển sang dùng **UART3**, chúng ta cần nắm rõ các thông số vật lý của chip i.MX8M Plus:

- **Địa chỉ Base vật lý của UART3:** `0x30880000`
    
- **Số ngắt phần cứng (IRQ) của UART3:** `28`
    
- **Vị trí chân trên Kit:** Cụm chân J18 (Chân số 8 là TX, Chân số 10 là RX).
    

## Bước 2: Tạo bản vá cấu hình phần cứng (`app.overlay`)

Thay vì tự định nghĩa một node UART3 hoàn toàn mới (sẽ bị lỗi do hệ thống quản lý xung clock `clocks` của i.MX8MP vô cùng phức tạp), chúng ta sẽ sử dụng kỹ thuật **"mượn vỏ đổi ruột"**. Chúng ta chiếm dụng node `&uart4` (vốn đã được hãng cấp sẵn clock chuẩn) và thay thế ruột địa chỉ/ngắt của UART3 vào.

Vào thư mục `~/zephyrproject/uart_echo/`, tạo hoặc mở file `app.overlay` và dán toàn bộ nội dung sau vào:

DTS

```
/* 1. Can thiệp vào nhãn &uart4 có sẵn trên board để biến nó thành UART3 */
&uart4 {
    /* Xóa bỏ thông tin địa chỉ vật lý và ngắt cũ của UART4 */
    /delete-property/ reg;
    /delete-property/ interrupts;

    /* Ghi đè thông số phần cứng của UART3 vào */
    reg = <0x30880000 0x4000>;  /* Địa chỉ Base của UART3 */
    interrupts = <28 0>;        /* Số ngắt IRQ của UART3 */
    status = "okay";
    current-speed = <115200>;

    /* Đánh lừa trình biên dịch bằng một cụm pinctrl rỗng (dummy) */
    modem-mode = <0>;           
    pinctrl-0 = <&uart3_dummy>; 
    pinctrl-names = "default";
};

/* 2. Tạo cấu trúc node chân rỗng để không bị bắt lỗi thiếu macro */
/ {
    pinctrl {
        uart3_dummy: uart3_dummy {
            /* Để trống, việc mux chân thực tế đã do U-Boot/Linux xử lý trước đó */
        };
    };

    /* 3. Chỉ định cho ứng dụng C (main.c) sử dụng cấu hình này */
    chosen {
        zephyr,app-uart = &uart4;  /* Bản chất tên là uart4 nhưng lõi bên trong đã là UART3 */
        zephyr,console = &uart4;
    };
};
```

## Bước 3: Dọn dẹp thư mục build cũ (Bắt buộc)

Vì Device Tree của Zephyr lưu bộ nhớ đệm (cache) rất kỹ, nếu bạn không xóa sạch bản build cũ, hệ thống sẽ giữ lại cấu hình UART4 và không nhận file `.overlay` mới.

Chạy lệnh Linux này để xóa thư mục `build/` một cách triệt để:

Bash

```
cd ~/zephyrproject/uart_echo
rm -rf build/
```

## Bước 4: Biên dịch ứng dụng (Build)

Tiến hành chạy lệnh biên dịch mới với tham số bo mạch đầy đủ cho lõi Cortex-M7 chạy trên vùng nhớ DDR:

Bash

```
west build -p always -b imx8mp_evk/mimx8ml8/m7/ddr
```

**Kết quả mong đợi:** Hệ thống sẽ biên dịch thành công 100% không báo lỗi CMake hay lỗi thiếu thuộc tính (`clocks`, `interrupts`...). File thực thi `zephyr.bin` sẽ được sinh ra trong thư mục `build/zephyr/`.

## Bước 5: Kết nối phần cứng và Thử nghiệm

1. **Kết nối dây vật lý:** Chuẩn bị một mạch chuyển đổi USB-to-UART (CP2102, FTDI...) và cắm vào cụm chân J18 trên kit i.MX8MP EVK:
    
    - Chân **TXD** của mạch USB-to-UART $\rightarrow$ Cắm vào **Chân số 10 (UART3_RXD)** trên J18.
        
    - Chân **RXD** của mạch USB-to-UART $\rightarrow$ Cắm vào **Chân số 8 (UART3_TXD)** trên J18.
        
    - Chân **GND** của mạch USB-to-UART $\rightarrow$ Cắm vào chân **GND** bất kỳ trên kit.
        
2. **Mở phần mềm máy tính:** Cắm mạch USB-to-UART vào máy tính, mở các phần mềm như Terminal, PuTTY hoặc Hercules, chọn đúng cổng COM và cấu hình tốc độ Baudrate là **115200**.
    
3. **Nạp code và Chạy:** Nạp file `zephyr.bin` xuống lõi Cortex-M7 thông qua U-Boot (bằng lệnh `bootaux`).
    
4. **Kiểm tra tính năng Echo:** Gõ bất kỳ ký tự nào từ bàn phím máy tính vào màn hình Terminal, ký tự đó sẽ lập tức được gửi xuống UART3 của chip, chip xử lý ngắt và truyền ngược lại màn hình.
