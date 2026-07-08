### BƯỚC 1: KHỞI TẠO MÔI TRƯỜNG ẢO PYTHON (.VENV)

Để chạy được công cụ `west`, hệ thống bắt buộc phải cài đặt các gói hỗ trợ thông qua trình quản lý `pip3` của Python.

- **Thao tác tuần tự:**
    
    1. Di chuyển vào thư mục dự án chứa Zephyr đã có sẵn:
        
        Bash
        
        ```
        mkdir -p ~/zephyrproject
        cd ~/zephyrproject
        ```
        
    2. Cài đặt gói tạo môi trường ảo của Ubuntu:
        
        Bash
        
        ```
        sudo apt update && sudo apt install python3-venv python3-pip -y
        ```
        
    3. Tiến hành tạo môi trường ảo có tên `.venv`:
        
        Bash
        
        ```
        python3 -m venv .venv
        ```
        

#### Lỗi thực tế thứ 1: Cài đặt tự do bị khóa (PEP 668)

Nếu bạn không chạy môi trường ảo mà gõ thẳng lệnh cài đặt `pip3 install west`, terminal sẽ lập tức ném ra lỗi:

Plaintext

```
error: externally-managed-environment
This environment is externally managed [...]
```

- **Cách khắc phục liên tục:** Phải ép hệ thống chuyển hẳn vào làm việc bên trong môi trường ảo vừa tạo bằng lệnh kích hoạt (`activate`). Sau khi kích hoạt, đầu dòng lệnh terminal sẽ xuất hiện ký tự `(.venv)` – báo hiệu vùng an toàn đã mở:
    
    Bash
    
    ```
    source .venv/bin/activate
    ```
    

### BƯỚC 2: CÀI ĐẶT CÔNG CỤ WEST VÀ ĐỒNG BỘ MÃ NGUỒN PHẦN CỨNG (WEST UPDATE)

Khi đã ở trong môi trường ảo, ta sử dụng `pip3` để nạp công cụ meta-tool `west`.

- **Thao tác tuần tự:**
    
    1. Ra lệnh cài đặt `west` vào `.venv`:
        
        Bash
        
        ```
        pip3 install west
        ```
        
    2. Khởi tạo không gian liên kết cục bộ với thư mục chứa nhân Zephyr:
        
        Bash
        
        ```
        west init -l zephyr
        ```
        
    3. Chạy lệnh đồng bộ để tải toàn bộ Driver ngoại vi (HAL) của NXP dành riêng cho chip i.MX8MP:
        
        Bash
        
        ```
        west update
        ```
        

#### Lỗi thực tế thứ 2: Lỗi biến mất lệnh "west: command not found"

Sau khi cài xong, nếu bạn lỡ tay tắt Terminal đi và mở lại một cửa sổ Terminal mới để gõ lệnh `west update`, hệ thống sẽ báo lỗi:

Bash

```
bash: west: command not found
```

- **Cách khắc phục liên tục:** Lỗi này xảy ra vì cửa sổ mới chưa được nạp biến môi trường của `.venv`. Bạn **không được cài lại**, mà phải di chuyển về thư mục gốc và gọi lại lệnh kích hoạt môi trường ảo. Hãy tạo thói quen gõ lệnh này mỗi khi mở máy làm việc:
    
    Bash
    
    ```
    cd ~/zephyrproject
    source .venv/bin/activate
    ```
    

### BƯỚC 3: CÀI ĐẶT CÁC THƯ VIỆN BỔ TRỢ VÀ BIÊN DỊCH FILE NHỊ PHÂN (ZEPHYR.BIN)

Trước khi compile mã nguồn `hello_world`, ta cần nạp các thư viện Python chuyên xử lý file DeviceTree (phần cứng cấu hình chip).

- **Thao tác tuần tự:**
    
    1. Cài đặt file yêu cầu hệ thống:
        
        Bash
        
        ```
        pip3 install -r zephyr/scripts/requirements.txt
        ```
        
    2. Di chuyển thẳng vào thư mục dự án mẫu `hello_world`:
        
        Bash
        
        ```
        cd zephyr/samples/hello_world
        ```
        
    3. Gõ lệnh biên dịch chéo hướng vào lõi Cortex-M7:
        
        Bash
        
        ```
        west build -b imx8mp_evk/mimx8ml8/m7 -p
        ```
        

#### Lỗi thực tế thứ 3: Biên dịch gãy vỡ do rác cấu hình cũ

Nếu bạn chỉnh sửa code hoặc đổi mục tiêu biên dịch mà chỉ gõ lệnh `west build`, trình biên dịch sẽ lấy lại các file cache cũ, sinh ra hàng loạt lỗi biên dịch không rõ nguyên nhân (Linker error / Symbol redefined).

- **Cách khắc phục liên tục:** Luôn luôn thêm tham số `-p` (Pristine) vào sau lệnh build. Tham số này sẽ dọn sạch bóng thư mục `build/` cũ, ép hệ thống quét lại cấu hình phần cứng mới từ đầu:
    
    Bash
    
    ```
    west build -b imx8mp_evk/mimx8ml8/m7 -p
    ```
    
    _Sau khi hoàn thành, file thực thi `zephyr.bin` sẽ nằm tại thư mục: `build/zephyr/zephyr.bin`._
    

### BƯỚC 4: CHUẨN BỊ THẺ NHỚ BOOT PHẦN CỨNG VÀ XỬ LÝ PHÂN VÙNG LƯU TRỮ

Để nạp file `zephyr.bin` vào board FRDM-IMX8MPlus, ta cần chuẩn bị một thẻ nhớ MicroSD.

- **Thao tác tuần tự:**
    
    1. Tải file ảnh đĩa Linux nền móng của hãng NXP (định dạng `.wic.zstd`) về máy Ubuntu.
        
    2. Giải nén file ảnh đĩa bằng lệnh:
        
        Bash
        
        ```
        zstd -df imx-image-multimedia-frdm-imx8mp.wic.zstd
        ```
        
    3. Cắm thẻ nhớ vào máy tính, sử dụng công cụ **Rufus** (trên Windows) hoặc **BalenaEtcher** (trên Linux) chọn file `.wic` vừa giải nén để flash trực tiếp vào thẻ nhớ SD.
        

#### Lỗi thực tế thứ 4: Thẻ nhớ trắng – Mạch im lặng hoàn toàn

Nếu bạn lấy một thẻ nhớ mới format FAT32 thông thường, chỉ copy duy nhất file `zephyr.bin` thả vào rồi cắm vào board bật nguồn, **cả hai cổng COM3 và COM4 của mạch sẽ đen kịt**, không xuất hiện bất kỳ log nào.

- **Cách khắc phục liên tục:** Chip i.MX8MP cấu trúc rất phức tạp, cần có phân vùng mồi hệ thống (`SPL` và `U-Boot`) nằm ở các block đầu của thẻ nhớ để kích hoạt RAM LPDDR4 trước. Việc flash file `.wic` ở trên là bắt buộc để "mồi sống" bo mạch, không được dùng thẻ nhớ trống.
    

### KẾT QUẢ NGHIỆM THU:

Cắm thẻ nhớ đã có file mồi và file `zephyr.bin` vào board FRDM-IMX8MPlus. Kết nối cáp micro-USB vào máy tính. Mở phần mềm MobaXterm kết nối cổng **COM4** (Baudrate `115200`), bật công tắc nguồn bo mạch. Màn hình lập tức tuôn log liên tục. Nhấn một phím bất kỳ để chặn đếm ngược, hệ thống dừng lại ở dấu nhắc lệnh:

Plaintext

```
u-boot=>
```

Sẵn sàng cho các bước can thiệp nạp xung lực và bộ nhớ.
