#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci{
    // #@@range_begin(config_addr)
    /** @brief CONFIG_ADDRESS  Register IO Port Address */
    const uint16_t kConfigAddress = 0x0cf8;
    /** @brief CONFIG_DATA  Register IO DATA Address */
    const uint16_t kConfigData = 0x0cfc;
    // #@@range_end(config_addr)

    // #@@range_begin(class_code)
    struct ClassCode{
        uint8_t base, sub, interface;

        /** @brief return true if match base class */
        bool Match(uint8_t b){ return b == base; }
        /** @brief return true if match base class and sub class */
        bool Match(uint8_t b, uint8_t s){ return Match(b) && s == sub; }
        /** @brief return true if match base class, sub class and interface */
        bool Match(uint8_t b, uint8_t s, uint8_t i){ return Match(b, s) && i == interface; }
    };

    /** @brief Store data to handle PCI device */
    struct Device{
        uint8_t bus, device, function, header_type;
        ClassCode class_code;
    };
    // #@@range_end(class_code)


    /** @brief Write int value to CONFIG_ADDRESS */
    void WriteAddress(uint32_t address);
    /** @brief Write int value to CONFIG_DATA */
    void WriteData(uint32_t value);
    /** @brief Read int value from CONFIG_DATA */
    uint32_t ReadData();

    /** @brief Read Vendor ID */
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
    /** @brief Read Device ID */
    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);
    /** @brief Read Header type*/
    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);
    /** @brief Read Class Code Register */
    ClassCode  ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

    inline uint16_t ReadVendorId(const Device& dev){
        return ReadVendorId(dev.bus, dev.device, dev.function);
    }

    uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr);

    void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value);

    /** @brief Read bus number
     *
     *  - 23:16 : sub autionate busclass
     *  - 15:8  : secondary bus
     *  - 7:0   : revision
     */
    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);

    /** @brief Return true if Single function */
    bool IsSingleFunctionDevice(uint8_t header_type);


    // #@@range_begin(var_devices)
    /** @brief PCI device list finding by ScanAllBus() */
    inline std::array<Device, 32> devices;
    /** @brief Valid number of devices */
    inline int num_device;

    /** @brief Search PCI devices and store them to devices */
    Error ScanAllBus();
    // #@@range_end(var_devices)

    constexpr uint8_t CalcBarAddress(unsigned int bar_index){
        return 0x10 + 4 * bar_index;
    }

    WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index);

}