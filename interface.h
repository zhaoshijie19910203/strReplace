#ifndef __INTERFACE_H_
#define __INTERFACE_H_

#include <stdint.h>

///单字节对齐
#pragma pack(push, 1)

///ui32Prefix的几个预定义值
#define U3V_PREFIX_COMMAND_PACKET   0x43563355  ///< 命令包
#define U3V_PREFIX_EVENT_PACKET     0x45563355  ///< 事件包

///U3V数据包command id的预定义值
#define U3V_READ_MEM_CMD            0x800        ///< 读内存请求包
#define U3V_READ_MEM_ACK            0x801        ///< 读内存应答包
#define U3V_WRITE_MEM_CMD           0x802        ///< 写内存请求包
#define U3V_WRITE_MEM_ACK           0x803        ///< 写内存应答包
#define U3V_PENDING_ACK             0x805        ///< Pending包
#define U3V_EVENT_CMD               0x0C00        ///< 事件包请求包


#define U3V_FLAGS_REQUEST_NO_ACK    0x0000 ///< 不要求应答包
#define U3V_FLAGS_REQUEST_ACK       0x4000  ///< 要求应答包

//写入字节数
#define WRITE_CMD_LENGTH                4
#define EVENT_CMD_LENGTH                4

//寄存器ADDR
#define SBRM_ADDR                   0x001D8
#define EIRM_ADDR                   (SBRM_ADDR + 0x2C)


typedef struct PREFIX
{
    uint32_t ui32Prefix;        ///< U3V协议前缀校验码
}PREFIX;

typedef struct CCD_CMD
{
    uint16_t ui16Flag;       ///< 命令选项，例如是否要求应答包
    uint16_t ui16CommandID;  ///< 命令ID，用于区分命令类型，例如读内存还是写内存
    uint16_t ui16Length;     ///< 命令长度，仅指SCD的长度，不含prefix和CCD的长度
    uint16_t ui16RequestID;  ///< 命令序列号
}CCD_CMD;

///U3V 读内存请求包特有数据（SCD：Specific Command Data）
typedef struct SCD_READ_MEM_CMD
{
    uint64_t     ui64RegisterAddress;    ///< 读内存起始地址
    uint16_t     ui16Reserved;           ///< 保留，必须为零
    uint16_t     ui16ReadLength;         ///< 读取字节数

}SCD_READ_MEM_CMD;

///读内存请求包
typedef struct READ_MEM_CMD
{
    PREFIX              m_stPrefix;         ///< U3V校验码
    CCD_CMD             m_stCCDCmd;         ///< 请求包的CCD
    SCD_READ_MEM_CMD    m_stSCDReadMemCmd;  ///< 读内存请求包的SCD
}READ_MEM_CMD;

//U3V 应答包通用数据(CCD)
typedef struct CCD_ACK
{
    uint16_t ui16StatusCode; ///< 状态码，含成功或失败信息
    uint16_t ui16CommandID;  ///< 命令ID，用于区分命令类型，例如读内存还是写内存
    uint16_t ui16Length;     ///< 命令长度，仅指SCD的长度，不含prefix和CCD的长度
    uint16_t ui16RequestID;  ///< 命令序列号

}CCD_ACK;

 ///U3V 读内存应答包特有数据（SCD）
typedef struct SCD_READ_MEM_ACK
{
    uint8_t      arrData[1024];             ///< 读到的内存数据（柔性数组）
}SCD_READ_MEM_ACK;

///读内存应答包
typedef struct READ_MEM_ACK
{
    PREFIX              m_stPrefix;         ///< U3V校验码
    CCD_ACK             m_stCCDAck;         ///< 应答包的CCD
    SCD_READ_MEM_ACK    m_stSCDReadMemAck;  ///< 读内存应答包的SCD
}READ_MEM_ACK;

///U3V 写内存请求包特有数据（SCD）
typedef struct SCD_WRITE_MEM_CMD
{
    uint64_t     ui64RegisterAddress;    ///< 写内存起始地址
    uint8_t      arrData[0];             ///< 写入内存的数据（柔性数组）
}SCD_WRITE_MEM_CMD;

///写内存请求包
typedef struct WRITE_MEM_CMD
{
    PREFIX              m_stPrefix;         ///< U3V校验码
    CCD_CMD             m_stCCDCmd;         ///< 请求包的CCD
    SCD_WRITE_MEM_CMD   m_stSCDWriteMemCmd; ///< 写内存请求包的SCD
}WRITE_MEM_CMD;

///U3V 写内存应答包特有数据（SCD）
typedef struct SCD_WRITE_MEM_ACK
{
    uint16_t     ui16Reserved;           ///< 保留，必须为零
    uint16_t     nWriteLength;           ///< 实际写入的字节数
}SCD_WRITE_MEM_ACK;

///写内存应答包
typedef struct _WRITE_MEM_ACK
{
    PREFIX              m_stPrefix;         ///< U3V校验码
    CCD_ACK             m_stCCDAck;         ///< 应答包的CCD
    SCD_WRITE_MEM_ACK   m_stSCDWriteMemAck; ///< 写内存应答包的SCD
}WRITE_MEM_ACK;

///U3V 事件请求包特有数据（SCD）
typedef struct SCD_EVENT_CMD
{
    uint16_t     ui16Reserved;           ///< 保留，必须为零
    uint16_t     ui16EventID;            ///< 事件ID
    uint64_t     ui32TimeStamp;          ///< 时间戳
    uint8_t      arrData[1024];             ///< 事件数据（柔性数组）
}SCD_EVENT_CMD;

///事件请求包
typedef struct _EVENT_CMD
{
    PREFIX              m_stPrefix;         ///< U3V校验码
    CCD_CMD             m_stCCDCmd;         ///< 请求包的CCD
    SCD_EVENT_CMD       m_stSCDEventCmd;    ///< 事件请求包的SCD
}EVENT_CMD;


///流数据
typedef struct  IMAGE_LEADER
{
	uint32_t ui32MagicKey;
	uint16_t ui16Reserved1;
	uint16_t ui16LeaderSize;
	uint64_t ui64BlockID;
	uint16_t ui16Reserved2;
	uint16_t ui16PayloadType;
	uint64_t ui64Timestamp;
	uint32_t ui32PixelFormat;
	uint32_t ui32SizeX;
	uint32_t ui32SizeY;
	uint32_t ui32OffsetX;
	uint32_t ui32OffsetY;
	uint16_t ui16PaddingX;
	uint16_t ui16Reserved3;
	
}IMAGE_LEADER;

typedef struct IMAGE_TRAILER
{
	uint32_t ui32MagicKey;
	uint16_t ui16Reserved1;
	uint16_t ui16TrailerSize;
	uint64_t ui64BlockID;
	uint16_t ui16Status;
	uint16_t ui16Reserved2;
	uint64_t ui64ValidPayloadSize;
	uint32_t ui32SizeY;
	
}IMAGE_TRAILER;


#       pragma pack(pop)

#endif
