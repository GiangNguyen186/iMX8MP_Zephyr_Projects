# Báo cáo Debug: UART3 Echo trên Cortex-M7 (Zephyr) — i.MX8MP LPDDR4 EVK

**Mục tiêu:** Chạy ứng dụng UART Echo trên lõi Cortex-M7 (Zephyr OS), giao tiếp với laptop qua UART3 vật lý (được route qua các chân `ECSPI1_SCLK`/`ECSPI1_MOSI`), baudrate 115200.

**Board:** i.MX8MP LPDDR4 EVK — U-Boot (A53) nạp và khởi động Zephyr trên M7 qua `bootaux`.

---

## Tổng quan các lỗi đã gặp (theo thứ tự xử lý)

|#|Lỗi|Nguyên nhân gốc|Trạng thái|
|---|---|---|---|
|1|`zephyr.bin` đứng im ở 18664 bytes dù sửa code|Build cache / thiếu pristine rebuild|Đã hướng dẫn xử lý|
|2|`bootelf` làm U-Boot crash (`ESR 0xbf000002`)|RDC (Resource Domain Controller) chặn ghi vùng TCM|Đã hướng dẫn né bằng `cp.b` + `bootaux`|
|3|`bootaux` chạy được (PC hợp lệ) nhưng UART3 **im lặng hoàn toàn**|pinctrl overlay dùng node **rỗng**, không cấu hình chân vật lý nào|✅ Đã sửa|
|4|`mmc 0` không tồn tại khi `fatload`|Sai chỉ số thiết bị MMC (nhầm eMMC ↔ khe SD rời)|Đã hướng dẫn `mmc list`|
|5|UART3 có tín hiệu nhưng ký tự ra **vỡ vụn** (baudrate sai)|Overlay đổi `reg`/`interrupts` sang UART3 nhưng **quên đổi `clocks`**, driver vẫn đọc clock root của UART4|✅ Đã sửa — **kết quả cuối cùng: THÀNH CÔNG**|

---

## Chi tiết từng lỗi

### File `zephyr.bin` không cập nhật (đứng im ở 18664 bytes)

**Triệu chứng:** Dù sửa `main.c` (kể cả ghi đè thanh ghi cứng), dung lượng file build ra vẫn không đổi.

**Cách phát hiện:** So sánh kích thước/`md5sum` của `zephyr.bin` trước và sau khi build lại.

**Nguyên nhân khả dĩ:**

- `west build` không nhận diện thay đổi do cache CMake/ninja cũ.
- SD card chưa được `sync`/eject an toàn trước khi rút → hệ điều hành host còn giữ bản cache cũ trong buffer.
- Ghi thanh ghi trực tiếp không khai báo `volatile` → trình biên dịch tối ưu hóa (dead code elimination) loại bỏ đoạn code, khiến các lần sửa "không có tác dụng" dù logic khác nhau.

**Giải pháp:**

```bash
west build -p always -b <board_target> .
ls -la build/zephyr/zephyr.bin
md5sum build/zephyr/zephyr.bin      # xác nhận đổi so với lần trước
```

- Dùng `sys_write32()`/`sys_read32()` (đã có `volatile` built-in) thay vì tự khai báo con trỏ thường khi ghi thanh ghi cứng.
- Luôn `sync` hoặc eject an toàn SD card trước khi rút.

---

### `bootelf` làm U-Boot crash — `"Error" handler, esr 0xbf000002`

**Triệu chứng:** Khi dùng `bootelf` để nạp trực tiếp file `.elf` vào vùng TCM, U-Boot lập tức crash và reset toàn hệ thống.

**Nguyên nhân:** `bootelf` là trình phân giải ELF tổng quát, không biết rằng vùng TCM của M7 bị **RDC (Resource Domain Controller)** khóa quyền ghi từ domain A53. Việc ghi trực tiếp gây ra external abort phần cứng.

**Giải pháp:** Không dùng `bootelf` cho core phụ M7. Dùng cách chuẩn của NXP — convert sang binary phẳng rồi copy thủ công vào alias địa chỉ TCM, sau đó `bootaux` (đã có sẵn cơ chế giữ/release reset M7 đúng chuẩn, tôn trọng RDC):

```bash
fatload mmc <dev>:1 0x48000000 zephyr.bin
cp.b 0x48000000 0x7e0000 <size>
bootaux 0x7e0000
```

---

### UART3 im lặng hoàn toàn dù `bootaux` báo PC hợp lệ

**Triệu chứng:** `bootaux` chạy thành công (`pc = 0x80000E6D`, Thumb-mode hợp lệ), nhưng cổng COM5 không có bất kỳ dữ liệu nào ra/vào.

**Cách phát hiện:** Đọc trực tiếp `app.overlay` đang dùng:

```dts
pinctrl {
    uart3_dummy: uart3_dummy {
        /* RỖNG — không có group0/pinmux bên trong */
    };
};
```

**Nguyên nhân gốc (2 lớp):**

1. **Node pinctrl rỗng** → khi Zephyr build, tạo ra pinctrl state có 0 phần tử. Driver UART gọi `pinctrl_apply_state()` lúc init nhưng **không cấu hình bất kỳ chân vật lý nào**. Hệ thống hoàn toàn phụ thuộc vào những gì U-Boot poke tay còn sót lại.
    
2. **Địa chỉ thanh ghi poke tay từ U-Boot trước đó bị sai.** Đối chiếu với bảng pinmux chính thức do NXP tự sinh cho đúng chip (MIMX8ML8DVNLZ, lấy từ repo `hal_nxp`), địa chỉ đúng là:
    
    |Chân|Chức năng|MUX reg|Giá trị|DAISY reg|Giá trị|PAD/config reg|
    |---|---|---|---|---|---|---|
    |ECSPI1_SCLK → UART3_RX|RX|`0x303301e0`|`1`|`0x303305f8`|`4`|`0x30330440`|
    |ECSPI1_MOSI → UART3_TX|TX|`0x303301e4`|`1`|_(không cần)_|—|`0x30330444`|
    
    Các địa chỉ từng dùng trước đó (`0x303300ec`, `0x303300e4`, `0x303302dc`, `0x303305ec`) **không khớp** với bảng chính thức này.
    

**Giải pháp:** Định nghĩa đúng pinmux ngay trong `app.overlay` bằng các phandle có sẵn trong SDK Zephyr (không cần poke tay từ U-Boot nữa — Zephyr tự cấu hình lại đúng mỗi lần boot):

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

&uart4 {                      /* label "uart4" trong Zephyr = UART3 vật lý sau khi override reg */
    /delete-property/ reg;
    /delete-property/ interrupts;
    reg = <0x30880000 0x4000>;
    interrupts = <28 0>;
    status = "okay";
    current-speed = <115200>;
    pinctrl-0 = <&uart3_ecspi1_default>;
    pinctrl-names = "default";
};
```

---

### Bad device specification mmc 0` khi `fatload`

**Triệu chứng:**

```
u-boot=> fatload mmc 0:1 0x48000000 zephyr.bin
** Bad device specification mmc 0 **
```

**Nguyên nhân:** Board có cả eMMC on-board lẫn khe SD rời — chỉ số thiết bị (`mmc0`, `mmc1`, `mmc2`...) không cố định, phụ thuộc cách U-Boot enumerate.

**Giải pháp:**

```
u-boot=> mmc list
u-boot=> mmc dev <đúng_index>
u-boot=> fatls mmc <đúng_index>:1     # xác nhận file có mặt trước khi fatload
```

---

### UART3 có tín hiệu nhưng ký tự ra **vỡ vụn / rác** (lỗi baudrate)

**Triệu chứng:** Sau khi sửa xong pinctrl (mục 3), UART3 đã có tín hiệu thật (không còn im lặng), nhưng ký tự hiển thị trên terminal là các ký tự lạ, không phải baud mismatch do cài sai tốc độ terminal.

**Cách phát hiện nguyên nhân:** Đọc trực tiếp mã nguồn driver `drivers/clock_control/clock_control_mcux_ccm.c` trong Zephyr:

```c
case IMX_CCM_UART1_CLK ... IMX_CCM_UART4_CLK: {
    uint32_t instance = clock_name & IMX_CCM_INSTANCE_MASK;
    clock_root_control_t clk_root = uart_clk_root[instance];
    uint32_t uart_mux = CLOCK_GetRootMux(clk_root);   // đọc THẬT từ thanh ghi CCM phần cứng

    if (uart_mux == 0)      *rate = MHZ(24);
    else if (uart_mux == 1) *rate = CLOCK_GetPllFreq(...) / preDiv / postDiv / 10;
}
```

**Nguyên nhân gốc:** `app.overlay` đã override `reg` và `interrupts` sang địa chỉ vật lý UART3, **nhưng không override `clocks`** — property này vẫn kế thừa từ node gốc:

```dts
clocks = <&ccm IMX_CCM_UART4_CLK 0x6c 24>;   /* vẫn là UART4! */
```

→ Driver đọc **thanh ghi clock root thật của UART4** (CCM_TARGET_ROOT) để tính bộ chia baudrate, trong khi phần cứng đang chạy thực sự là **UART3** (có thể có mux/divider clock khác). Baudrate tính sai → ký tự ra bị lệch tần số bit, thành ký tự vỡ vụn — chứ không mất tín hiệu hoàn toàn (đúng như quan sát).

**Giải pháp:**

```dts
&uart4 {
    ...
    clocks = <&ccm IMX_CCM_UART3_CLK 0x6c 24>;   /* đổi đúng sang UART3 */
    ...
};
```

Header `imx_ccm.h` (chứa định nghĩa `IMX_CCM_UART3_CLK`) đã được include sẵn từ `nxp_imx8ml_m7.dtsi` gốc, không cần thêm `#include` trong overlay.

---

## `app.overlay` cuối cùng (đã hoạt động hoàn chỉnh)

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

&uart4 {
	/delete-property/ reg;
	/delete-property/ interrupts;

	reg = <0x30880000 0x4000>;
	interrupts = <28 0>;
	clocks = <&ccm IMX_CCM_UART3_CLK 0x6c 24>;
	status = "okay";
	current-speed = <115200>;

	pinctrl-0 = <&uart3_ecspi1_default>;
	pinctrl-names = "default";
};

/ {
	chosen {
		zephyr,app-uart = &uart4;
		zephyr,console = &uart4;
	};
};
```

---

## Boot scripting
Sau khi đã sửa toàn bộ chỉ cần boot bằng 
	fatload mmc 1:1 0x48000000 zephyr.bin
	cp.b 0x48000000 0x7e0000 <zephyr.bin size>
   	bootaux 0x7e0000

Hoặc gõ trên U-boot console như sau:
	u-boot=> setenv mmcdev 1
	u-boot=> setenv m7image zephyr.bin
	u-boot=> setenv m7loadaddr 0x48000000
	u-boot=> setenv m7runaddr 0x7e0000
	u-boot=> setenv bootm7 'mmc dev ${mmcdev}; fatload mmc ${mmcdev}:1 ${m7loadaddr} ${m7image}; cp.b ${m7loadaddr} ${m7runaddr} ${filesize}; bootaux ${m7runaddr}'
	u-boot=> setenv bootcmd 'run bootm7; run distro_bootcmd'
	u-boot=> saveenv

## Bài học rút ra

1. **Không nên poke thanh ghi IOMUXC thủ công từ U-Boot** rồi hy vọng Zephyr "không đụng vào" — cách bền vững là định nghĩa đúng trong devicetree để chính Zephyr tự cấu hình lại mỗi lần boot, tránh phụ thuộc trạng thái sót lại từ bootloader.
2. Khi **remap một peripheral node** (đổi `reg`/`interrupts` để trỏ sang phần cứng vật lý khác), phải rà soát **toàn bộ property phụ thuộc phần cứng** đi kèm — không chỉ `reg`/`interrupts`/`pinctrl`, mà cả `clocks`, vì driver có thể đọc trực tiếp thanh ghi phần cứng dựa trên ID clock khai báo trong devicetree, độc lập với `reg`.
3. Triệu chứng **"im lặng hoàn toàn"** thường chỉ ra vấn đề ở tầng **pinmux/routing** (chưa có đường tín hiệu vật lý); triệu chứng **"có tín hiệu nhưng dữ liệu sai/vỡ"** thường chỉ ra vấn đề ở tầng **clock/baudrate** — hai tầng lỗi độc lập, cần tách riêng để debug đúng hướng.
4. Bảng pinmux tự sinh bởi công cụ chính thức của NXP (trong `hal_nxp`, dùng bởi Zephyr SDK) là nguồn tham chiếu đáng tin cậy nhất cho địa chỉ thanh ghi IOMUXC — nên đối chiếu tại đây trước khi tự tra cứu bằng tay từ reference manual.

