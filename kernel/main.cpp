#include <cstdint>
#include <cstddef>
#include <cstdio>

// #@@range_begin(includes)
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "mouse.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"
// #@@range_end(includes)

// #@@range_begin(placement_new)
// void* operator new(size_t size, void* buf){
//     return buf;
// }

void operator delete(void* obj) noexcept {
}
// #@@range_end(placement_new)

const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

// #@@range_begin(console_buf)
char console_buf[sizeof(Console)];
Console *console;
// #@@range_end(console_buf)

// #@@range_begin(printk)
int printk(const char* format, ...){
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
}
// #@@range_end(printk)

// #@@range_begin(mouse_observer)
char mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;

void MouseObserver(int8_t displacement_x, int8_t displacement_y){
    mouse_cursor->MoveRelative({displacement_x, displacement_y});
}
// #@@range_end(mouse_observer)

// range_begin(switch_echi2xhci)
void SwitchEhci2Xhci(const pci::Device& xhc_dev){
    bool intel_ech_exist = false;
    for(int i = 0; i < pci::num_device; i++){
        if(pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) && 0x8086 == pci::ReadVendorId(pci::devices[i])){
            intel_ech_exist = true;
            break;
        }
    }
    if(!intel_ech_exist){
        return;
    }

    uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc);
    pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports);
    uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4);
    pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports);
    Log(kDebug, "SwitchEhci2Xhci: SS = %02d, xHCI = %02x\n", superspeed_ports, ehci2xhci_ports);
}
// #@@range_end(switch_echi2xhci)

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config){
    switch(frame_buffer_config.pixel_format){
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf)
                RGBResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf)
                BGRResv8BitPerColorPoxelWriter{frame_buffer_config};
            break;
    }

    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;

    FillRectangle(*pixel_writer, {0, 0}, {kFrameWidth, kFrameHeight - 50}, kDesktopBGColor);
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth, 50}, {1, 8, 17});
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth / 5, 50}, {80, 80, 80});
    DrawRectangle(*pixel_writer, {10, kFrameHeight - 40}, {30, 30}, {160, 160, 160});

    // #@@range_begin(new_console)
    console = new(console_buf) Console{*pixel_writer, kDesktopFGColor, kDesktopBGColor};
    // #@@range_end(new_console)

    printk("Wlcome to MikanOS!\n");
    SetLogLevel(kWarn);

    // #@@range_begin(new_mouse_cursor)
    mouse_cursor = new (mouse_cursor_buf) MouseCursor{
        pixel_writer, kDesktopBGColor, {300, 200}
    };
    // #@@range_end(new_mouse_cursor)

    // #@@range_begin(scan_devices)
    auto err = pci::ScanAllBus();
    printk("ScanAllBus: %s\n", err.Name());

    for(int i = 0; i < pci::num_device; i++){
        const auto& dev = pci::devices[i];
        auto vendor_id  = pci::ReadVendorId(dev.bus, dev.device, dev.function);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        Log(kDebug, "%d.%d.%d: vend %04x, class %08x, head %02x\n", dev.bus, dev.device, dev.function, vendor_id, class_code, dev.header_type);
    }
    // #@@range_end(scan_devices)

    pci::Device* xhc_dev = nullptr;
    for(int i = 0; i < pci::num_device; i++){
        if(pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)){
            xhc_dev = &pci::devices[i];

            if(0x8086 == pci::ReadVendorId(*xhc_dev)){
                break;
            }
        }
    }

    if(xhc_dev){
        Log(kInfo, "xHC has been found: %d.%d.%d\n", xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    }

    // #@@range_begin(read_bar)
    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
    Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);
    // #@@range_end(read_bar)

    // #@@range_begin(init_xhc)
    usb::xhci::Controller xhc{xhc_mmio_base};

    if(0x8086 == pci::ReadVendorId(*xhc_dev)){
        SwitchEhci2Xhci(*xhc_dev);
    }
    {
        auto err = xhc.Initialize();
        Log(kDebug, "xhc.Initialize: %s\n", err.Name());
    }

    Log(kInfo, "xHC starting\n");
    xhc.Run();
    // #@@range_end(init_xhc)

    // #@@range_begin(configure_port)
    usb::HIDMouseDriver::default_observer = MouseObserver;

    for(int i = 1; i <= xhc.MaxPorts(); i++){
        auto port = xhc.PortAt(i);
        Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

        if(port.IsConnected()){
            if(auto err = ConfigurePort(xhc, port)){
                Log(kError, "failed to configure port: %s at %s:%d\n", err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }
    // #@@range_end(configure_port)

    // #@@range_begin(receive_event)
    while(1){
        if(auto err = ProcessEvent(xhc)){
            Log(kError, "Error while ProcessEvent: %s at %s:%d\n", err.Name(), err.File(), err.Line());
        }
    }
    // #@@range_end(receive_event)

    while(1) __asm__("hlt");
}

extern "C" void __cxa_pure_virtual(){
    while(1) __asm__("hlt");
}