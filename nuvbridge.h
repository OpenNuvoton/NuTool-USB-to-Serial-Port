
#ifndef NUVOTON_BRIDGE_H
#define NUVOTON_BRIDGE_H


#if 0 // for reference
/*---------------------------------------------------------------------------------------------------------*/
/* Message ID Type Constant Definitions                                                                    */
/*---------------------------------------------------------------------------------------------------------*/
#define    CAN_STD_ID    0ul    /*!< CAN select standard ID \hideinitializer */
#define    CAN_EXT_ID    1ul    /*!< CAN select extended ID \hideinitializer */

/*---------------------------------------------------------------------------------------------------------*/
/* Message Frame Type Constant Definitions                                                                 */
/*---------------------------------------------------------------------------------------------------------*/
#define    CAN_REMOTE_FRAME    0ul    /*!< CAN frame select remote frame \hideinitializer */
#define    CAN_DATA_FRAME    1ul      /*!< CAN frame select data frame \hideinitializer */


typedef struct {
    uint32_t  IdType;       /*!< ID type */
    uint32_t  FrameType;    /*!< Frame type */
    uint32_t  Id;           /*!< Message ID */
    uint8_t   DLC;          /*!< Data length */
    uint8_t   Data[8];      /*!< Data */
} STR_CANMSG_T;

#endif

enum {
    CAN_STD_ID = 0,
    CAN_EXT_ID = 1
};

enum {
    CAN_REMOTE_FRAME = 0,
    CAN_DATA_FRAME = 1
};

#pragma pack(push)
#pragma pack(1)
struct STR_CANMSG_T {
    unsigned int    IdType;       /*!< ID type */
    unsigned int    FrameType;    /*!< Frame type */
    unsigned int    Id;           /*!< Message ID */
    unsigned char   DLC;          /*!< Data length */
    char            Data[8];      /*!< Data */
};


struct STR_CANCMD_T {
    unsigned int    IdType;       /*!< ID type */
    unsigned int    FrameType;    /*!< Frame type */
    unsigned int    Id;           /*!< Message ID */
    unsigned char   DLC;          /*!< Data length */
    char            Data[8];      /*!< Data */
};

#pragma pack(pop)

union CANMSG_U {
    STR_CANMSG_T can_frame;
    char can_raw[sizeof (STR_CANMSG_T)];
};



#endif // #ifndef NUVOTON_BRIDGE_H
