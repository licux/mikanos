#include "pci.hpp"

#include "asmfunc.h"

namespace{
    using namespace pci;

    // #@@range_begin(make_address)
    /** @brief Generate 32bit integer for CONFIG_ADDRESS */
    uint32_t MakeAddress(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_addr){
        auto sh1 = [](uint32_t x, unsigned int bits){
            return x << bits;
        };

        return sh1(1, 31)
            | sh1(bus, 16)
            | sh1(device ,11)
            | sh1(function, 8)
            | (reg_addr & 0xfcu);
    }
    // #@@range_end(make_address)

    // #@@range_begin(add_device)
    /** @brief Write information to devices[num_device] and increment num_device */
    Error AddDevice(uint8_t bus, uint8_t device, uint8_t function, uint8_t header_type){
        if(num_device == devices.size()){
            return MAKE_ERROR(Error::kFull);
        }
        devices[num_device] = Device{bus, device, function, header_type};
        num_device++;
        return MAKE_ERROR(Error::kSuccess);
    }

    Error AddDevice(const Device& device){
        if(num_device == devices.size()){
            return MAKE_ERROR(Error::kFull);
        }

        devices[num_device] = device;
        num_device++;
        return MAKE_ERROR(Error::kSuccess);
    }
    // #@@range_End(add_device)

    Error ScanBus(uint8_t bus);

    // #@@range_begin(scan_function)
    /** @brief Add specific function to devices
     *  If PCI-PCI bridge, execute Scanbus for secondary bus
     */
    Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function){
        auto class_code = ReadClassCode(bus, device, function);
        auto header_type = ReadHeaderType(bus, device, function);
        Device dev{bus, device, function, header_type, class_code};
        if(auto err = AddDevice(dev)){
            return err;
        }

        if(class_code.Match(0x06u, 0x04u)){
            // standard PCI-PCI bridge
            auto bus_numbers = ReadBusNumbers(bus, device, function);
            uint8_t secondary_bus = (bus_numbers >> 8) & 0xffu;
            return ScanBus(secondary_bus);
        }

        return MAKE_ERROR(Error::kSuccess);
    }
    // #@@range_end(scan_function)


    // #@@range_begin(scan_device)
    /** @brief Scan each function for specific device number
     *  If valid function is found, execute ScanFunction()
     */
    Error ScanDevice(uint8_t bus, uint8_t device){
        if(auto err = ScanFunction(bus, device, 0)){
            return err;
        }
        if(IsSingleFunctionDevice(ReadHeaderType(bus, device, 0))){
            return MAKE_ERROR(Error::kSuccess);
        }

        for(uint8_t function = 1; function < 8; function++){
            if(ReadVendorId(bus, device, function) == 0xffffu){
                continue;
            }
            if(auto err = ScanFunction(bus, device, function)){
                return err;
            }
        }
        return MAKE_ERROR(Error::kSuccess);
    }
    // #@@range_end(scan_device)

    // #@@range_begin(scan_bus)
    /** @brief Scan specified bus number for each device
     *  If valid device is found, execute ScanDevice()
     */
    Error ScanBus(uint8_t bus){
        for(uint8_t device = 0; device < 32; device++){
            if(ReadVendorId(bus, device, 0) == 0xffffu){
                continue;
            }
            if(auto err = ScanDevice(bus, device)){
                return err;
            }
        }
        return MAKE_ERROR(Error::kSuccess);
    }
    // #@@range_end(scan_bus)

}

namespace pci {
    // #@@range_begin(config_addr_data)
    void WriteAddress(uint32_t address){
        IoOut32(kConfigAddress, address);
    }

    void WriteData(uint32_t value){
        IoOut32(kConfigData, value);
    }

    uint32_t ReadData(){
        return IoIn32(kConfigData);
    }

    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function){
        WriteAddress(MakeAddress(bus, device, function, 0x00));
        return ReadData() & 0xffffu;
    }
    // #@@range_end(config_addr_data)

    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function){
        WriteAddress(MakeAddress(bus, device, function, 0x00));
        return ReadData() >> 16;
    }

    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function){
        WriteAddress(MakeAddress(bus, device, function, 0x0c));
        return (ReadData() >> 16) & 0xffu;
    }

    ClassCode  ReadClassCode(uint8_t bus, uint8_t device, uint8_t function){
        WriteAddress(MakeAddress(bus, device, function, 0x08));
        auto reg = ReadData();
        ClassCode cc;
        cc.base       = (reg >> 24) & 0xffu;
        cc.sub        = (reg >> 16) & 0xffu;
        cc.interface = (reg >>  8) & 0xffu;
        return cc;
    }

    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function){
        WriteAddress(MakeAddress(bus, device, function, 0x18));
        return ReadData();
    }

    bool IsSingleFunctionDevice(uint8_t header_type){
        return (header_type & 0x80u) == 0;
    }
    
    // #@@range_begin(scan_all_bus)
    Error ScanAllBus(){
        num_device = 0;

        auto header_type = ReadHeaderType(0 , 0, 0);
        if(IsSingleFunctionDevice(header_type)){
            return ScanBus(0);
        }

        for(uint8_t function = 1; function < 8; function++){
            if(ReadVendorId(0, 0, function) == 0xffffu){
                continue;
            }
            if(auto err = ScanBus(function)){
                return err;
            }
        }
        return MAKE_ERROR(Error::kSuccess);
    }
    // #@@range_end(scan_all_bus)

    uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr){
        WriteAddress(MakeAddress(dev.bus, dev.device, dev.function, reg_addr));
        return ReadData();
    }

    void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value){
        WriteAddress(MakeAddress(dev.bus, dev.device, dev.function, reg_addr));
        WriteData(value);
    }

    WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index){
        if(bar_index >= 6){
            return {0, MAKE_ERROR(Error::kIndexOutOfRange)};
        }

        const auto addr = CalcBarAddress(bar_index);
        const auto bar = ReadConfReg(device, addr);

        if((bar & 4u) == 0){
            return {bar, MAKE_ERROR(Error::kSuccess)};
        }

        if(bar_index >= 5){
            return {0, MAKE_ERROR(Error::kIndexOutOfRange)};
        }

        const auto bar_upper = ReadConfReg(device, addr + 4);
        return {bar | (static_cast<uint64_t>(bar_upper) << 32), MAKE_ERROR(Error::kSuccess)};
    }
}