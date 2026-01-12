#include <cstdint>



struct SlaveStruct
{
    /* data */
    uint16_t alias; /**< Slave alias address. */
    uint16_t pos; /**< Slave position. */
    uint32_t vendor_id; /**< Slave vendor ID. */
    uint32_t product_code; /**< Slave product code. */
    uint16_t index; /**< PDO entry index. */
    uint8_t subindex; /**< PDO entry subindex. */
    unsigned int *offset; /**< Pointer to a variable to store the PDO entry's
                       (byte-)offset in the process data. */
    unsigned int *bit_position = nullptr; /**< Pointer to a variable to store a bit
                                  position (0-7) within the \a offset. Can be
                                  NULL, in which case an error is raised if
                                  the PDO entry does not byte-align. */
    bool motor;                                
};

