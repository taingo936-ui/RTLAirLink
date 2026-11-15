# RTLAirLink

Bản thử nghiệm driver Realtek RTL8852BE cho macOS, phát triển từ AirPort_RTW89 và các thay đổi Owly cho AWDL/AirDrop.

[![Ủng hộ qua Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/taingo)

Hai kext được phát hành cùng nhau:

- `RTLAirLink.kext`: driver Wi-Fi, chứa frontend `AirPortRTW89Front.kext`.
- `RTLAirBridge.kext`: cầu nối Lilu với Wi-Fi Legacy của Apple.

Đổi tên thư mục kext, CFBundleName/CFBundleDisplayName và nhãn model/driver Wi-Fi. Mục model vẫn ghi đúng chip Realtek được phát hiện. Bundle ID, executable, class, tên thư mục frontend và boot-args giữ nguyên. Cần thay cả hai kext từ cùng một bản build; không nạp thêm bản tên cũ vì bundle ID trùng nhau.

Bản phát hành **v0.1.0 vẫn là thử nghiệm**. Quá trình phát triển đã có truyền file thật hai chiều qua AWDL nhưng còn Waiting và mất dấu thiết bị. Bản này sửa hai lỗi phản hồi BlockAck và bổ sung tên hiển thị RTLAirLink. Kiểm thử mã nguồn và build đã qua; bộ kext đổi tên vẫn cần xác nhận đầy đủ bridge, tên trong System Information và độ ổn định sau boot. Tên/ảnh liên hệ trùng do tài khoản không được thay đổi bởi driver.

Môi trường kiểm tra hiện tại: RTL8852BE, Intel/x86_64, macOS build `25G241`, Legacy UUID `4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB`. Không coi danh sách PCI ID kế thừa là danh sách card đã hỗ trợ hoàn chỉnh.

Xem [hướng dẫn cài](docs/INSTALL.md) và [cách build](docs/BUILD.md).

Gói tương ứng cũng có tại [GitHub Packages](https://github.com/users/taingo936-ui/packages/container/package/rtlairlink), địa chỉ `ghcr.io/taingo936-ui/rtlairlink:v0.1.0`. Gói OCI chứa nguyên năm file của Release; xem [cách tải bằng ORAS](docs/PACKAGES.md). Có thể tải ZIP trực tiếp trong Releases.

Theo hướng dẫn AirPort_RTW89 gốc, phải có thêm **IOSkywalkFamily.kext, IO80211FamilyLegacy.kext và AMFIPass.kext**, ngoài Lilu. Gói Release kèm `Download-Dependencies.command` để tải đúng ba ZIP từ nguồn OCLP và kiểm tra SHA256. Xem [dependency và cấu hình OpenCore](docs/DEPENDENCIES.md) để biết thứ tự nạp, bỏ plugin Broadcom, chặn Skywalk hệ thống, SecureBootModel và boot-arg Tahoe. Ba binary bên ngoài không được đóng lại trong gói của dự án, giống cách phát hành upstream.

Lệnh build:

```bash
bash build.sh
```

Kết quả nằm trong `build/<timestamp>/VERIFIED_KEXTS`. Script không cài vào EFI hay tự reboot. Các dependency Apple, Lilu và Bluetooth cần có riêng; repo không kèm EFI cá nhân, log hay bản kext hệ thống Apple.

Giữ LICENSE, phần ghi công tác giả gốc và các giấy phép dependency khi phân phối lại. Mã nguồn tương ứng với bản phát hành được cung cấp trong repo và source archive của release. Firmware Realtek giữ giấy phép riêng, không chuyển thành GPL.
