#pragma once
#include <stdint.h>
#include "pico/bootrom.h"

namespace zoned_otp {
    typedef enum class _otp_zone_t : uint8_t {
        // These are all the valid "zones" that can be messed with
        // defined as `starting row / 0x100`
        zone1  = 0x1u, // OTP Rows 0x100..0x1FF
        zone2  = 0x2u, // OTP Rows 0x200..0x2FF
        zone3  = 0x3u, // OTP Rows 0x300..0x3FF
        zone4  = 0x4u, // OTP Rows 0x400..0x4FF
        zone5  = 0x5u, // OTP Rows 0x500..0x5FF
        zone6  = 0x6u, // OTP Rows 0x600..0x6FF
        zone7  = 0x7u, // OTP Rows 0x700..0x7FF
        zone8  = 0x8u, // OTP Rows 0x800..0x8FF
        zone9  = 0x9u, // OTP Rows 0x900..0x9FF
        zone10 = 0xAu, // OTP Rows 0xA00..0xAFF
        zone11 = 0xBu, // OTP Rows 0xB00..0xBFF
        zone12 = 0xCu, // OTP Rows 0xC00..0xCFF
        zone13 = 0xDu, // OTP Rows 0xD00..0xDFF
        // zone14 = 0xEu, // reserve this zone for the directory entries
    } otp_zone_t;
    static constexpr uint8_t MIN_USABLE_ZONE = 0x1u; // Rows 0x100u .. 0x1FFu;
    static constexpr uint8_t MAX_USABLE_ZONE = 0xDu; // Rows 0xD00u .. 0xDFFu;

    static constexpr uint16_t OTP_ZONE_ROWCOUNT { 0x100u };
    static constexpr uint16_t get_zone_start_row(otp_zone_t zone) {
        return static_cast<uint8_t>(zone) * OTP_ZONE_ROWCOUNT;
    }

    typedef enum class _otp_zone_state_t : uint8_t {
        unknown = 0u,
        all_zero = 1u,
        max_single_bit_flipped_per_row = 2u,
        multiple_flipped_bits_per_row = 3u,
    } otp_zone_state_t;

    typedef enum class _otp_zone_data_type_t : uint8_t {
        ecc      = 0u,
        brbp_ecc = 1u,
        raw      = 2u,
    } otp_zone_data_type_t;
    typedef enum class _otp_bit_flip_t : uint8_t {
        none = 0u,                    // default without adding errors
        single_bit_flip_0_to_1 = 1u,  // coded already ... easy situation
        single_bit_flip_1_to_0 = 2u,  // TODO: implement as an option
        multiple_bits_flipped  = 3u,  // TODO: implement as an option
    } otp_bit_flip_t;

    typedef struct _zone_config_t {
        otp_zone_data_type_t base_data_type;
        otp_bit_flip_t       bit_flip_type;
        const char*          name;
    } zone_config_t;

    typedef struct _otp_zone_ecc_buffer_t {
        uint16_t ecc_data[OTP_ZONE_ROWCOUNT];
    } otp_zone_ecc_buffer_t;
    typedef struct _otp_zone_raw_buffer_t {
        uint32_t raw_data[OTP_ZONE_ROWCOUNT];
    } otp_zone_raw_buffer_t;

    namespace _bootrom_read {
        int read_zone(otp_zone_t zone, otp_zone_ecc_buffer_t& data);
        int read_zone(otp_zone_t zone, otp_zone_raw_buffer_t& data);
    }
    namespace _saferotp_read {
        int read_zone(otp_zone_t zone, otp_zone_ecc_buffer_t& data);
        int read_zone(otp_zone_t zone, otp_zone_raw_buffer_t& data);
    }

    // easy way to switch between "actually writes" and "does not actually write"
    namespace _saferotp_write {
        int write_zone(otp_zone_t zone, otp_zone_ecc_buffer_t& data);
        int write_zone(otp_zone_t zone, otp_zone_raw_buffer_t& data);
    }

    namespace _actually_writes {
        int write_zone(otp_zone_t zone, otp_zone_ecc_buffer_t& data);
        int write_zone(otp_zone_t zone, otp_zone_raw_buffer_t& data);
    }
    namespace _does_not_actually_write {
        int write_zone(otp_zone_t zone, otp_zone_ecc_buffer_t& data);
        int write_zone(otp_zone_t zone, otp_zone_raw_buffer_t& data);
    }
    otp_zone_state_t determine_zone_state(otp_zone_t zone);
    bool is_zone_ecc_writable(otp_zone_t zone);
    bool is_zone_all_zero(otp_zone_t zone);
    bool set_brbp_bitflips(otp_zone_t zone, otp_zone_ecc_buffer_t& data);
    bool find_blank_zone(otp_zone_t& zone);
    bool flip_two_bits(otp_zone_raw_buffer_t& data);
    bool add_single_bit_errors(otp_zone_raw_buffer_t& data, bool flip_from_0_to_1);
    bool add_bit_errors(otp_zone_raw_buffer_t& data, otp_bit_flip_t bitflip_type);
    bool initialize_ecc_values(otp_zone_raw_buffer_t& out_buffer, bool force_brbp = false, otp_bit_flip_t bitflip_type = otp_bit_flip_t::none);
    bool initialize_3way_majority_vote(otp_zone_raw_buffer_t& out_buffer, otp_bit_flip_t bitflip_type = otp_bit_flip_t::none);
    void hexdump(const otp_zone_raw_buffer_t& data, const char* line_prefix = nullptr);
    void dump_c_struct(const otp_zone_raw_buffer_t& data, const char* name);
    bool prepare_zone_buffer(const zone_config_t& cfg, otp_zone_raw_buffer_t& out_buffer);

    namespace _data {

        extern const otp_zone_raw_buffer_t raw3x_none;
        extern const otp_zone_raw_buffer_t raw3x_0_to_1;
        extern const otp_zone_raw_buffer_t raw3x_1_to_0;
        extern const otp_zone_raw_buffer_t raw3x_multibit;
        extern const otp_zone_raw_buffer_t ecc_none;
        extern const otp_zone_raw_buffer_t ecc_0_to_1;
        extern const otp_zone_raw_buffer_t ecc_1_to_0;
        extern const otp_zone_raw_buffer_t ecc_multibit;
        extern const otp_zone_raw_buffer_t brbp_none;
        extern const otp_zone_raw_buffer_t brbp_0_to_1;
        extern const otp_zone_raw_buffer_t brbp_1_to_0;
        extern const otp_zone_raw_buffer_t brbp_multibit;
    }
}
