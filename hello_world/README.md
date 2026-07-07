### BƯỚC 1: CẤU HÌNH NGOẠI VI UART4 ĐỂ MỞ KÊNH XUẤT LOG (COM3)

Trước khi ra lệnh cho lõi M7 chạy ứng dụng `hello_world`, ta phải chuẩn bị sẵn "màn hình" để M7 in dữ liệu ra, cụ thể là cổng ngoại vi UART4 (được ánh xạ ra cổng vật lý **COM3** trên máy tính).

- **Thao tác tuần tự:**
    
    Gõ các lệnh ghi trực tiếp vào thanh ghi phần cứng của chip i.MX8MP tại dấu nhắc lệnh:
    
    Plaintext
    
    ```
    u-boot=> mw.l 0x303844a0 0x3
    u-boot=> mw.l 0x303300bc 0x0
    u-boot=> mw.l 0x303300c0 0x0
    ```
    

#### Lỗi thực tế thứ 1: Lỗi "màn hình đen kịt" trên cổng COM3

Nếu bỏ qua không gõ 3 dòng lệnh trên mà lập tức nạp và chạy code, hệ thống trên COM4 vẫn báo kích hoạt M7 thành công (`Success`), nhưng cửa sổ giám sát **COM3 hoàn toàn trống trơn**, không có một chữ nào xuất hiện.

- **Nguyên nhân cốt lõi:** Khi chip i.MX8MP vừa khởi động, để tiết kiệm điện, hệ thống mặc định tắt hoàn toàn xung nhịp cấp cho khối UART4. Đồng thời, các chân vật lý truyền nhận tín hiệu (TX/RX) của UART4 chưa được kết nối (Pinmux) ra các chân cắm bên ngoài.
    
- **Cách khắc phục liên tục:** 3 lệnh `mw.l` (Memory Write) phía trên là bắt buộc để mở khóa phần cứng: lệnh thứ nhất cấp xung nhịp (Clock Gating), hai lệnh tiếp theo thực hiện chuyển mạch kết nối chân TX và RX của UART4 ra mạch nạp USB.
    

### BƯỚC 2: NẠP FILE THỰC THI TỪ THỂ NHỚ LÊN BOARD (PHƯƠNG PHÁP BẮC CẦU RAM)

Mục tiêu là đưa tệp `zephyr.bin` trong thẻ nhớ vào bộ nhớ nội tại tốc độ cao (TCM) của lõi M7 (địa chỉ cục bộ là `0x7E0000`).

- **Thao tác tuần tự:**
    
    Sử dụng lệnh đọc file từ phân vùng thẻ nhớ SD, nhưng thay vì nạp thẳng vào TCM, ta nạp tạm vào một dải RAM DDR tự do (địa chỉ `0x48000000`):
    
    Plaintext
    
    ```
    u-boot=> fatload mmc 1:1 0x48000000 zephyr.bin
    ```
    
    _(Màn hình sẽ hiển thị số bytes đọc được, báo hiệu nạp vào RAM tạm thành công)._
    

#### Lỗi thực tế thứ 2: Lỗi chặn ghi "overwrite reserved memory"

Nếu làm theo tư duy thông thường, gõ lệnh nạp thẳng file từ thẻ nhớ vào địa chỉ của lõi M7 bằng lệnh: `fatload mmc 1:1 0x7E0000 zephyr.bin`, U-Boot sẽ lập tức từ chối và ném ra lỗi:

Plaintext

```
Reading file would overwrite reserved memory
```

- **Nguyên nhân cốt ổ:** Hệ thống giám sát bộ nhớ của U-Boot đã đánh dấu dải địa chỉ `0x7E0000` thuộc phân vùng bảo lưu hệ thống (Reserved Memory). Lệnh ngoại vi `fatload` không được phép can thiệp hay ghi trực tiếp lên phân vùng nhạy cảm này.
    
- **Cách khắc phục liên tục:** Phải dùng giải pháp **Bắc Cầu**. Luôn luôn dùng `fatload` nạp file vào dải RAM DDR công cộng tự do `0x48000000` trước (nơi không bị U-Boot cấm đoán), sau đó mới tìm cách dịch chuyển khối dữ liệu này vào phân vùng của M7 ở bước sau.
    

### BƯỚC 3: DỊCH CHUYỂN DỮ LIỆU VÀO VÙNG NHỚ TCM (VƯỢT LỖI SẬP MẠCH CRASH BUS)

Khi dữ liệu đã nằm trên RAM tạm `0x48000000`, ta phải dùng lệnh sao chép bộ nhớ để đẩy nó về đúng phân vùng TCM của lõi M7.

- **Thao tác tuần tự:**
    
    1. Ra lệnh khóa chặt, ép lõi M7 phải ngủ sâu trong lúc nạp code để tránh xung đột:
        
        Plaintext
        
        ```
        u-boot=> mw.l 0x3039000c 0x0
        ```
        
    2. Tắt hoàn toàn bộ đệm dữ liệu Cache của U-Boot:
        
        Plaintext
        
        ```
        u-boot=> dcache off
        ```
        
    3. Tiến hành copy dữ liệu từ RAM tạm vào **địa chỉ hệ thống tổng** của vùng TCM (`0x3B000000`):
        
        Plaintext
        
        ```
        u-boot=> cp.b 0x48000000 0x3b000000 0x6000
        ```
        

#### Lỗi thực tế thứ 3: Lỗi sập nguồn rác thanh ghi "Error handler, esr 0xbf000002"

Nếu dùng lệnh copy thô sơ: `cp.b 0x48000000 0x7E0000 0x6000`, toàn bộ bo mạch sẽ lập tức bị treo cứng, ném ra một bảng sập nguồn rác thanh ghi và tự động reset lại mạch:

Plaintext

```
"Error" handler, esr 0xbf000002
Resetting CPU ...
```

- **Nguyên nhân cốt lõi:** Lỗi chí mạng này xảy ra do hai sai lầm nghiêm trọng kết hợp:
    
    1. _Sai lệch bản đồ địa chỉ:_ Địa chỉ `0x7E0000` chỉ là địa chỉ cục bộ (Local Address) trong mắt lõi M7. Lõi A53 (nơi U-Boot đang chạy) nhìn vào địa chỉ này sẽ không hiểu và gây lạc đường Bus. Từ góc nhìn hệ thống tổng, địa chỉ toàn cục (Global Address) của vùng nhớ này phải là **`0x3B000000`**.
        
    2. _Xung đột bảo mật Cache:_ Khi dùng lệnh `cp.b`, bộ tối ưu hóa dữ liệu (D-Cache) của U-Boot sẽ can thiệp. Việc này kích hoạt hệ thống bảo mật phần cứng (RDC) hiểu lầm có một cuộc tấn công xâm nhập vùng nhớ, dẫn đến sập cầu dao tổng (Bus Crash) để bảo vệ chip.
        
- **Cách khắc phục liên tục:** Tuân thủ tuyệt đối quy trình 3 lệnh ở phần thao tác: Ép M7 ngủ (`mw.l 0x3039000c 0x0`), tắt bộ đệm dữ liệu (`dcache off`) để ép ghi thô trực tiếp, và dịch chuyển về địa chỉ toàn cục **`0x3B000000`**. Lúc này lệnh copy sẽ chạy mượt mà không còn bị reset mạch.
    

### BƯỚC 4: KÍCH HOẠT LÕI CORTEX-M7 VÀ NGHIỆM THU

Sau khi mã nguồn đã nằm an toàn trong bộ nhớ nội tại, ta tiến hành đánh thức lõi M7 dậy để thực thi hệ điều hành Zephyr RTOS.

- **Thao tác tuần tự:**
    
    Gõ lệnh khởi chạy phụ trợ tại dấu nhắc lệnh COM4:
    
    Plaintext
    
    ```
    u-boot=> bootaux 0x7E0000
    ```
    
    _(Lưu ý: Lệnh `bootaux` là lệnh đặc thù, nó yêu cầu truyền vào địa chỉ theo góc nhìn gốc của lõi M7, tức là địa chỉ cục bộ `0x7E0000`)._
    

### KẾT QUẢ NGHIỆM THU:

- **Tại cổng COM4 (U-Boot):** Hiện thông báo khởi chạy lõi phụ thành công dạng: `## Starting auxiliary core at 0x007E0000`.
    
- **Tại cổng COM3 (M7 Monitor):** Màn hình lập tức bừng sáng và tuôn ra các dòng log hoạt động đầu tiên của hệ điều hành thời gian thực:
    
    Plaintext
    
    ```
    *** Booting Zephyr OS build v4.4.0-xxx ***
    Hello World! imx8mp_evk/mimx8ml8/m7
    ```
