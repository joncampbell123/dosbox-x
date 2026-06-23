#include <check.h>
#include <stdlib.h>
#include <string.h>
#include "include/bios_disk.h"

START_TEST(test_bios_disk_memcpy_bounds)
{
    // Invariant: memcpy must never write beyond the bounds of the destination buffer
    uint8_t buffer[2048];
    uint8_t data[512];
    
    // Initialize with known pattern
    memset(buffer, 0xAA, sizeof(buffer));
    memset(data, 0x00, sizeof(data));
    
    // Payload cases: exploit, boundary, valid
    uint32_t test_cases[] = {
        0xFFFFFFFF,  // Exploit: large sectnum causing overflow
        0x00000004,  // Boundary: exactly at buffer limit
        0x00000000   // Valid: within bounds
    };
    
    for (int i = 0; i < 3; i++) {
        uint32_t sectnum = test_cases[i];
        
        // Call the actual function from bios_disk.h
        uint8_t result = bios_disk_read(sectnum, data, buffer);
        
        // Security property: if function returns success (0x00),
        // data must contain only bytes from within buffer bounds
        if (result == 0x00) {
            // Verify no buffer overflow occurred by checking
            // that data contains the expected pattern from buffer
            uint32_t offset = (sectnum & 3) * 512;
            ck_assert_msg(offset + 512 <= sizeof(buffer),
                         "Successful read must be within buffer bounds");
            
            // Verify data matches expected buffer region
            for (int j = 0; j < 512; j++) {
                ck_assert_msg(data[j] == buffer[offset + j],
                             "Data must match source buffer contents");
            }
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_bios_disk_memcpy_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}