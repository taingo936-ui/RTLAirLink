# RTLAirLink

Bản thử nghiệm driver Realtek RTL8852BE cho macOS, phát triển từ AirPort_RTW89 và các thay đổi Owly cho AWDL/AirDrop.

Hai kext được phát hành cùng nhau:

- `RTLAirLink.kext`: driver Wi-Fi, chứa frontend `AirPortRTW89Front.kext`.
- `RTLAirBridge.kext`: cầu nối Lilu với Wi-Fi Legacy của Apple.

Đổi tên thư mục kext, CFBundleName/CFBundleDisplayName và nhãn model/driver Wi-Fi. Mục model vẫn ghi đúng chip Realtek được phát hiện. Bundle ID, executable, class, tên thư mục frontend và boot-args giữ nguyên. Cần thay cả hai kext từ cùng một bản build; không nạp thêm bản tên cũ vì bundle ID trùng nhau.

Fix46 vẫn là **experimental**. Fix45 từng truyền file thật hai chiều qua AWDL nhưng còn Waiting và mất dấu thiết bị. Fix46 sửa hai lỗi phản hồi BlockAck, đã qua kiểm thử mã nguồn và build; backend đã được thấy nạp sau reboot. Chưa xác nhận đầy đủ bridge và độ ổn định truyền hai chiều của Fix46. Tên/ảnh liên hệ trùng do tài khoản không được thay đổi bởi driver.

Môi trường kiểm tra hiện tại: RTL8852BE, Intel/x86_64, macOS build `25G241`, Legacy UUID `4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB`. Không coi danh sách PCI ID kế thừa là danh sách card đã hỗ trợ hoàn chỉnh.

Xem [hướng dẫn cài](docs/INSTALL.md) và [cách build](docs/BUILD.md). Lệnh build:

```bash
bash build.sh
```

Kết quả nằm trong `build/<timestamp>/VERIFIED_KEXTS`. Script không cài vào EFI hay tự reboot. Các dependency Apple, Lilu và Bluetooth cần có riêng; repo không kèm EFI cá nhân, log hay bản kext hệ thống Apple.

Giữ LICENSE, phần ghi công tác giả gốc và các giấy phép dependency khi phân phối lại. Mã nguồn tương ứng với bản phát hành được cung cấp trong repo và source archive của release. Firmware Realtek giữ giấy phép riêng, không chuyển thành GPL.
