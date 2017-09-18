//-----------------------------------------------------------
/**
\file               LogAnalysis.c
\brief              U3协议log文件解析模块       
\version            v1.3
\date               2017-9-11            
\author             Zhao Shijie
*/
//-----------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <stdarg.h>
#include "interface.h"

#define LOG_BLOCK_NUM               100000        //允许处理的命令模块数量，方便查看数据开始位置的处理结果
#define CMD_LINE_NUM                100000       //存储命令模块的定义数组行数(应大于MODULE_LINE_MAX)
#define ROW_LENGTH                  200         //一行数据长度，可大于实际长度
#define COMMOM_OVERFLOW             100         //通用溢出宏定义

#define DESCRIPTION_LENGTH          16          //desciption段字节数
#define CMDID_OFFSET                19          //CMD数据的对Data首地址的偏移量    
#define REGADDR_OFFSET              13          //REGADDR数据对Data首地址的偏移量
#define ARR_CMD_PHASE_LENGTH        30          //CMD Phase字符数组size

#define MAX_DEVICE_NUM              100         //最大设备device num
#define MAX_ENDPOINT_NUM            5           //最大端口号

#define READ_ACK_DATA_MAX           15           //READ CMD返回ACK中包含的data数据字节打印个数

#define FILENAME_SIZE               100 		 //文件名字节长度

//保存各数据段偏移量的结构体
typedef struct LOCATION_OFFSET
{
    uint16_t ui16DeviceOff;
    uint16_t ui16PhaseOff;
    uint16_t ui16DataOff;
    uint16_t ui16DescriptOff;
    uint16_t ui16CmdPhaseOff;
    uint16_t ui16TimeOff;
    uint16_t ui16LenOff;

    uint16_t ui16DataLength;
    uint16_t ui16PhaseLength;
    uint16_t ui16CmdPhaseLength;
    uint16_t ui16TimeLength;
    uint16_t ui16LenLength;
}LOCATION_OFFSET;

//设备号端口号结构体
typedef struct DEV_ENP_NUM
{
    uint8_t ui8DeviceNum;
    uint8_t ui8EndpointNum;
}DEV_ENP_NUM;

//特定设备端口对应状态
typedef struct DEV_ENP_STATUS
{
    uint8_t ui8Status;								//设备端口对应状态
    char arrCurrentFileName[FILENAME_SIZE];			//该设备端口对应的当前图像文件绝对打开路径
    uint64_t ui64BlockID;
    uint64_t ui64PayloadSizeSum;
}DEV_ENP_STATUS;

//-----------------------------------------------------------
/*
\brief      根据'-'字符计算log文件中各数据字段的长度
\parm[in]   传入'---- ------'行数据      
\return     返回in参数指针指向的字段长度
*/
//-----------------------------------------------------------
uint16_t CalSegLength(char *pSeg)
{
    uint16_t ui16Len = 0;
    while(*pSeg == '-')
    {   
        ui16Len++;
        pSeg++;
    }
    return ui16Len;
}

//-----------------------------------------------------------
/*
\brief      获取设备号和端口号
\parm[in]   pSrc    要分割的字符串指针
\      
\return     
*/
//-----------------------------------------------------------
int GetDevEnpNum(char *pSrc, DEV_ENP_NUM &stDevEnpNum)
{
    char *pDot = strchr(pSrc, '.');
    char arrTemp[10];
    if (pDot == NULL)
    {
        printf("Not find dot!!!\n");
        return -1;
    }
    else
    {
        memcpy(arrTemp, pSrc, pDot - pSrc);
        arrTemp[pDot - pSrc] = '\0';
        stDevEnpNum.ui8DeviceNum = (uint8_t)strtol(arrTemp, NULL, 10);
        stDevEnpNum.ui8EndpointNum = (uint8_t)strtol(pDot + 1, NULL, 10);
        return 0;
    }
}


//-----------------------------------------------------------
/*
\brief      从单个命令模块中取出协议data数据，并转换为所代表实际数值存入字符串中，以用于协议结构体类型转换
\parm[in]   pArrStr[][ROW_LENGTH]    单独一个模块的数据
\parm[in]   nIndex                  模块的行数
\parm[in]   stOffset                各数据段偏移量结构体
\parm[out]  pCmdPacket              保存字符串转换为实际数值的数据
\return     
*/
//-----------------------------------------------------------
int GetPacket(char **pArrStr, int nIndex, const LOCATION_OFFSET &stOffset, char *pCmdPacket)
{
    uint8_t ui8Temp = 0;
    uint8_t ui8Count = 0;
    for (int i = 0; i < nIndex; ++i)
    {
        for (int j = stOffset.ui16DataOff; j < stOffset.ui16DataOff+stOffset.ui16DataLength; ++j)
        {
            ui8Temp = 0;
            if ((pArrStr[i][j] >= '0') && (pArrStr[i][j] <= '9'))
            {
                ui8Temp = pArrStr[i][j] - '0';
                ui8Count++;
            }
            else if((pArrStr[i][j] >= 'a') && (pArrStr[i][j] <= 'f'))
            {
                ui8Temp = 0x0a + pArrStr[i][j] - 'a';
                ui8Count++;
            }
            else if ((pArrStr[i][j] >= 'A') && (pArrStr[i][j] <= 'F'))
            {
                ui8Temp = 0x0A + pArrStr[i][j] - 'A';
                ui8Count++;
            }
            else if (pArrStr[i][j] == ' ')
            {
                continue;
            }
            else
            {
                printf("Wrong Num!\n");
                return -1;   
            }

            if(ui8Count%2 != 0)
            {
                *pCmdPacket = ui8Temp;
            }   
            else
            {
                *pCmdPacket = ((*pCmdPacket) << 4) | ui8Temp;
                pCmdPacket++;
            }
        }
    }
    return 0;
}

//-----------------------------------------------------------
/*
\brief		打开图像文件，追加写入数据
\parm   	pArrStr 				动态二维数组存储模块内容
\parm   	nIndex					二维数组中有效数据行数
\parm   	pArrDevEnpInfo			设备端口状态
\parm 		stOffset				各数据段偏移量结构体	
\parm　		stDevEnpNum				当前模块设备号和端口号
\return		
*/
//-----------------------------------------------------------
uint64_t WriteByteToImg(char **pArrStr, int nIndex, DEV_ENP_STATUS (*pArrDevEnpInfo)[MAX_ENDPOINT_NUM], const LOCATION_OFFSET &stOffset, const DEV_ENP_NUM &stDevEnpNum)
{
	//payload写入文件
    FILE *fpImage = fopen(pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].arrCurrentFileName, "a+");
    if (fpImage == NULL)
    {
        printf("Open image file fail!!!\n");
        return 0;
    }
    //计算当前payload size
    char arrLength[30] = {0};
    memcpy(arrLength, pArrStr[0] + stOffset.ui16LenOff, stOffset.ui16LenLength);
    arrLength[stOffset.ui16LenLength] = '\0';
    uint32_t ui16PayloadSize = strtol(arrLength, NULL, 10);

    char *pImgPayLoad = new char[nIndex*ROW_LENGTH];
    GetPacket(pArrStr, nIndex, stOffset, pImgPayLoad);
    uint32_t ui32WriteByte = fwrite(pImgPayLoad, ui16PayloadSize, 1, fpImage);
    printf("ActualWriteByte = %d\n", ui32WriteByte*ui16PayloadSize);
    fclose(fpImage);
    fpImage = NULL;

    delete pImgPayLoad;
    pImgPayLoad = NULL;
    return (ui32WriteByte*ui16PayloadSize);
}

//-----------------------------------------------------------
/*
\brief		打开文本文件，追加写入log解析结果
\parm   	pArrResultFileName 			文本文件绝对路径
\parm   	pFileLineContent			计划写入文件的字符串数据
\return		写入状态
*/
//-----------------------------------------------------------
int WriteToFile(char *pArrResultFileName, char *pFileLineContent)
{
	FILE *fp = fopen(pArrResultFileName, "a+");
    if (fp == NULL)
    {
        printf("Open File Fail !!!\n");
        return -1;
    }
    int status = fputs(pFileLineContent, fp);
    if (status == -1)
    {
    	printf("Write File Fail!!!\n");
    	return -1;
    }

    fclose(fp);
    fp = NULL;
    return 0;
}

//-----------------------------------------------------------
/*
\brief		将格式字符串（可变参数）在控制台打印，并写入对应路径文件
\parm   	
\parm   	
\parm   	
\parm 			
\parm　		
\return		
*/
//-----------------------------------------------------------
int FileSprintf(char *pArrResultFileName, const char *format, ...)
{
	char pOutBuffer[ROW_LENGTH];
	va_list ap;  //用于保存可变参数
	va_start(ap, format);
	vsprintf(pOutBuffer, format, ap);
	va_end(ap);

	WriteToFile(pArrResultFileName, pOutBuffer);    //写入文件
	printf("%s", pOutBuffer);						//控制台打印
	return 0;
}

//-----------------------------------------------------------
/*
\brief  单模块解析
\parm   arrStr[][ROW_LENGTH]        单独一个模块的数据
\parm   index                       模块的行数
\parm   stOffset                      各数据段偏移量结构体
\return
*/
//-----------------------------------------------------------
int CodeToCMD(char **pArrStr, int nIndex, const LOCATION_OFFSET &stOffset, char *pArrResultFileName)
{
    uint64_t ui64Temp = 0;
    uint16_t ui16Temp = 0;
    uint64_t ui64ADDR = 0;
    uint16_t ui16CMDID = 0;
    char arrDescript[DESCRIPTION_LENGTH];
    char *pCmdPacket = new char[nIndex*ROW_LENGTH];
    
    GetPacket(pArrStr, nIndex, stOffset, pCmdPacket);
    READ_MEM_CMD *pCMD = (READ_MEM_CMD *)pCmdPacket;

    if (pCMD->m_stPrefix.ui32Prefix == U3V_PREFIX_COMMAND_PACKET) //判断是否为U3VC
    {
        if (pCMD->m_stCCDCmd.ui16CommandID == U3V_READ_MEM_CMD)   
        {   
            READ_MEM_CMD *pstReadMemCMD = (READ_MEM_CMD *)pCmdPacket;
            FileSprintf(pArrResultFileName, "#READ CMD:  PREFIX:0x%x Flags:0x%x CommandID:0x%x RequestID:0x%x Address:0x%llx ReadLength:0x%x\n\n", pstReadMemCMD->m_stPrefix.ui32Prefix
                                                                                                   , pstReadMemCMD->m_stCCDCmd.ui16Flag                 
                                                                                                   , pstReadMemCMD->m_stCCDCmd.ui16CommandID
                                                                                                   , pstReadMemCMD->m_stCCDCmd.ui16RequestID
                                                                                                   , pstReadMemCMD->m_stSCDReadMemCmd.ui64RegisterAddress
                                                                                                   , pstReadMemCMD->m_stSCDReadMemCmd.ui16ReadLength);
        }
        else if (pCMD->m_stCCDCmd.ui16CommandID == U3V_READ_MEM_ACK)  //RACK
        {
            READ_MEM_ACK *pstReadMemACK = (READ_MEM_ACK *)pCmdPacket;
            FileSprintf(pArrResultFileName, "#READ ACK:  PREFIX:0x%x Status:0x%x CommandID:0x%x RequestID:0x%x SCDLength:0x%x SCDData:", pstReadMemACK->m_stPrefix.ui32Prefix
                                                                                                    , pstReadMemACK->m_stCCDAck.ui16StatusCode
                                                                                                    , pstReadMemACK->m_stCCDAck.ui16CommandID
                                                                                                    , pstReadMemACK->m_stCCDAck.ui16RequestID
                                                                                                    , pstReadMemACK->m_stCCDAck.ui16Length);
            uint64_t *pui64Temp = NULL;
            if (pstReadMemACK->m_stCCDAck.ui16Length == 8)
            {
                pui64Temp = (uint64_t *)pstReadMemACK->m_stSCDReadMemAck.arrData;
                FileSprintf(pArrResultFileName, "0x%llx 0x%llx", (*pui64Temp) >> 32, (*pui64Temp) & 0x0000ffff);
            }
            else if (pstReadMemACK->m_stCCDAck.ui16Length == 4)
            {
                pui64Temp = (uint64_t *)pstReadMemACK->m_stSCDReadMemAck.arrData;
                FileSprintf(pArrResultFileName, "0x%llx", (*pui64Temp) & 0x0000ffff);
            }
            else
            {
                FileSprintf(pArrResultFileName, "0x");
                for (int i = 0; i < pstReadMemACK->m_stCCDAck.ui16Length; ++i)
                {
                    FileSprintf(pArrResultFileName, "%02x", pstReadMemACK->m_stSCDReadMemAck.arrData[i]);
                    if (i >= READ_ACK_DATA_MAX)
                    {
                        FileSprintf(pArrResultFileName, "......");
                        break;
                    }
                }

            }
            
            FileSprintf(pArrResultFileName, "\n\n");
        }
        else if (pCMD->m_stCCDCmd.ui16CommandID == U3V_WRITE_MEM_CMD)
        {
            WRITE_MEM_CMD *pstWriteMemCMD = (WRITE_MEM_CMD *)pCmdPacket;
            FileSprintf(pArrResultFileName, "#WRITE CMD:  PREFIX:0x%x Flags:0x%x CommandID:0x%x RequestID:0x%x Address:0x%llx", pstWriteMemCMD->m_stPrefix.ui32Prefix
                                                                                                   , pstWriteMemCMD->m_stCCDCmd.ui16Flag
                                                                                                   , pstWriteMemCMD->m_stCCDCmd.ui16CommandID
                                                                                                   , pstWriteMemCMD->m_stCCDCmd.ui16RequestID
                                                                                                   , pstWriteMemCMD->m_stSCDWriteMemCmd.ui64RegisterAddress);
            FileSprintf(pArrResultFileName, " SCDData:");
            uint64_t *pui64Temp = NULL;
            if (pstWriteMemCMD->m_stCCDCmd.ui16Length - 8 == 8)
            {
                pui64Temp = (uint64_t *)(pstWriteMemCMD->m_stSCDWriteMemCmd.arrData + 8);
                FileSprintf(pArrResultFileName, "0x%llx 0x%llx", (*pui64Temp) >> 32, (*pui64Temp) & 0x0000ffff);
            }
            else if (pstWriteMemCMD->m_stCCDCmd.ui16Length - 8 == 4)
            {
                pui64Temp = (uint64_t *)(pstWriteMemCMD->m_stSCDWriteMemCmd.arrData + 8);
                FileSprintf(pArrResultFileName, "0x%llx", (*pui64Temp) & 0x0000ffff);
            }
            else
            {
                FileSprintf(pArrResultFileName, "0x");
                for (int i = 0; i < pstWriteMemCMD->m_stCCDCmd.ui16Length - 8; ++i)
                {
                    FileSprintf(pArrResultFileName, "%02x", pstWriteMemCMD->m_stSCDWriteMemCmd.arrData[i]);
                    if (i >= READ_ACK_DATA_MAX)
                    {
                        FileSprintf(pArrResultFileName, "......");
                        break;
                    }
                }
            }
            FileSprintf(pArrResultFileName, "\n\n");
        }
        else if (pCMD->m_stCCDCmd.ui16CommandID == U3V_WRITE_MEM_ACK)
        {
            WRITE_MEM_ACK *pstWriteMemACK = (WRITE_MEM_ACK *)pCmdPacket;
            FileSprintf(pArrResultFileName, "#WRITE ACK:  PREFIX:0x%x Status:0x%x CommandID:0x%x RequestID:0x%x SCDLength:0x%x nWriteLength:0x%x\n", pstWriteMemACK->m_stPrefix.ui32Prefix
                                                                                                   , pstWriteMemACK->m_stCCDAck.ui16StatusCode
                                                                                                   , pstWriteMemACK->m_stCCDAck.ui16CommandID
                                                                                                   , pstWriteMemACK->m_stCCDAck.ui16RequestID
                                                                                                   , pstWriteMemACK->m_stCCDAck.ui16Length
                                                                                                   , pstWriteMemACK->m_stSCDWriteMemAck.nWriteLength);
            FileSprintf(pArrResultFileName, "\n");
        }

    }
   
    delete pCmdPacket;
    pCmdPacket = NULL;
    return 0;
}

//-----------------------------------------------------------
/*
\brief  单模块解析
\parm   pArrStr[][ROW_LENGTH]        单独一个模块的数据
\parm   index                       模块的行数
\parm   stOffset                      各数据段偏移量结构体
\return
*/
//-----------------------------------------------------------
int ModuleAnalysis(char **pArrStr, int nIndex, DEV_ENP_STATUS (*pArrDevEnpInfo)[MAX_ENDPOINT_NUM], const LOCATION_OFFSET &stOffset, char *pArrResultFileName)
{
    char arrFileName[FILENAME_SIZE];
    DEV_ENP_NUM stDevEnpNum = {0, 0};
    int nRet = GetDevEnpNum(pArrStr[0], stDevEnpNum);
    if (nRet != 0)
    {
        printf("Error GetDevEnpNum\n");
        return -1;
    }

    char arrPhase[20];
    memcpy(arrPhase, pArrStr[0] + stOffset.ui16PhaseOff, stOffset.ui16PhaseLength);
    arrPhase[stOffset.ui16PhaseLength] = '\0';

    if (pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui8Status == 0)  //U3VC
    {
        if (strstr(pArrStr[0], "55 33 56 43") != NULL)
        {
            CodeToCMD(pArrStr, nIndex, stOffset, pArrResultFileName);
        }
        else if (strstr(pArrStr[0], "55 33 56 4c") != NULL)
        {
            //打印Leader信息
            char *pCmdPacket = new char[nIndex*ROW_LENGTH];
            GetPacket(pArrStr, nIndex, stOffset, pCmdPacket);
            IMAGE_LEADER *pImageLeader = new IMAGE_LEADER;
            pImageLeader = (IMAGE_LEADER *)pCmdPacket;
            FileSprintf(pArrResultFileName, "#ImageLeader(%d.%d): BlockID:0x%llx PayloadType:0x%x Timestamp:0x%llx PixelFormat:0x%x SizeX:0x%x SizeY:0x%x OffsetX:0x%x OffsetY:0x%x\n\n"
            																						, stDevEnpNum.ui8DeviceNum
            																						, stDevEnpNum.ui8EndpointNum
            																						, pImageLeader->ui64BlockID
                                                                                                    , pImageLeader->ui16PayloadType
                                                                                                    , pImageLeader->ui64Timestamp
                                                                                                    , pImageLeader->ui32PixelFormat
                                                                                                    , pImageLeader->ui32SizeX
                                                                                                    , pImageLeader->ui32SizeY
                                                                                                    , pImageLeader->ui32OffsetX
                                                                                                    , pImageLeader->ui32OffsetY);

            pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui8Status = 1;   //U3VL ImageLeader
            pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui64BlockID = pImageLeader->ui64BlockID;  //保存ImageLeader中的BlockID
            pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui64PayloadSizeSum = 0; //将payload size总和清零

            //创建文件夹和图像文件
            memset(arrFileName, 0, FILENAME_SIZE);
            sprintf(arrFileName, "./%03d", stDevEnpNum.ui8DeviceNum);
            if (access(arrFileName, F_OK) == -1)
            {
                mkdir(arrFileName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            }

            char arrTime[30] = {0};
            memcpy(arrTime, pArrStr[0] + stOffset.ui16TimeOff, stOffset.ui16TimeLength);
            arrTime[stOffset.ui16TimeLength] = '\0';
            char *pTimeMs = strstr(arrTime, ".");
            if (pTimeMs == NULL)
            {
                printf("Not find ms\n");
                return -1;
            }
            uint16_t ui16TimeMs = strtol(pTimeMs + 1, NULL, 10);
            sprintf(arrFileName, "./%03d/%03d_%03lld_%03d.pgm", stDevEnpNum.ui8DeviceNum, stDevEnpNum.ui8DeviceNum, pImageLeader->ui64BlockID, ui16TimeMs);
            strcpy(pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].arrCurrentFileName, arrFileName);
            FILE *fpImage = fopen(arrFileName, "w+");
            if (fpImage == NULL)
            {
                printf("Create image file fail!!!\n");
                return -1;
            }
            fprintf(fpImage, "P5\n%u %u\n255\n", pImageLeader->ui32SizeX, pImageLeader->ui32SizeY);
            fclose(fpImage);
            fpImage = NULL;

            delete pCmdPacket;
            pCmdPacket = NULL;
        }
        else if(strstr(arrPhase, "IN") != NULL)
        {
            FileSprintf(pArrResultFileName, "有残帧!!!\n");
            return -1;
        }
    }
    else if (pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui8Status == 1)
    {
        if (strstr(pArrStr[0], "55 33 56") != NULL)
        {
            FileSprintf(pArrResultFileName, "没有payload\n");
            return -1;
        } 

        pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui64PayloadSizeSum += WriteByteToImg(pArrStr, nIndex, pArrDevEnpInfo, stOffset, stDevEnpNum);
        pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui8Status = 2;     //下一个模块属于payload  
    }
    else if (pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui8Status == 2)  //图像数据payload模块
    {
        if (strstr(pArrStr[0], "55 33 56 54") != NULL)  //U3VT ImageTrailor
        {
            ///打印Trailor信息
            char *pCmdPacket = new char[nIndex*ROW_LENGTH];
            GetPacket(pArrStr, nIndex, stOffset, pCmdPacket);
            IMAGE_TRAILER *pImageTrailer = new IMAGE_TRAILER;
            pImageTrailer = (IMAGE_TRAILER *)pCmdPacket;
            FileSprintf(pArrResultFileName, "#ImageTrailor(%d.%d): TrailerSize:0x%x BlockID:0x%llx Status:0x%x ValidPayloadSize:0x%llx SizeY:0x%x\n\n"
            																						, stDevEnpNum.ui8DeviceNum
            																						, stDevEnpNum.ui8EndpointNum
            																						, pImageTrailer->ui16TrailerSize
                                                                                                    , pImageTrailer->ui64BlockID
                                                                                                    , pImageTrailer->ui16Status
                                                                                                    , pImageTrailer->ui64ValidPayloadSize
                                                                                                    , pImageTrailer->ui32SizeY);

            pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui8Status = 0;
            if (pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui64BlockID != pImageTrailer->ui64BlockID) //判断Leader和Trailor的BlockID是否匹配
            {
            	FileSprintf(pArrResultFileName, "Leader和Trailor BlockID不匹配!!!\n");
            	delete pCmdPacket;
            	pCmdPacket = NULL;
            	return -1;	
            }
            if (pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui64PayloadSizeSum != pImageTrailer->ui64ValidPayloadSize)
            {
            	FileSprintf(pArrResultFileName, "payload数据总大小不等于Trailor中的payload size!!!\n");
            	delete pCmdPacket;
            	pCmdPacket = NULL;
            	return -1;
            }
            delete pCmdPacket;
            pCmdPacket = NULL;
        }   
        else if(strstr(pArrStr[0], "55 33 56") != NULL)  //没有来Trailor
        {
            FileSprintf(pArrResultFileName, "没有Trailor，有残帧!!!\n");
            return -1;
        }
        else  //payload数据
        {
        	//payload写入文件，并计算payload累加和
        	pArrDevEnpInfo[stDevEnpNum.ui8DeviceNum][stDevEnpNum.ui8EndpointNum].ui64PayloadSizeSum += WriteByteToImg(pArrStr, nIndex, pArrDevEnpInfo, stOffset, stDevEnpNum);
        }
    }
    return 0;

}

//-----------------------------------------------------------
/*
\brief  从原始数据中，提取出模块数据，并调用ModuleAnalysis函数解析
\parm   fpData                      文件指针
\parm  	pArrStr  					存储单个模块数据的二维数组
\return
*/
//-----------------------------------------------------------
int LogAnalysis(FILE *fpData, char **pArrStr, char *pArrResultFileName)
{
    //char pArrStr[CMD_LINE_NUM][ROW_LENGTH];
    char arrTemp[ROW_LENGTH];   //存放一行数据
    char *pTemp = NULL;
    char *pStatus = NULL;
    bool bStatus = false;
    LOCATION_OFFSET stOffset;
    int nIndex = 0;
    char arrCmdPhase[ARR_CMD_PHASE_LENGTH];
    int nOverFlow = 0;

    //设备端口号功能注册二维数组
    DEV_ENP_STATUS arrDevEnpInfo[MAX_DEVICE_NUM][MAX_ENDPOINT_NUM];
    memset(arrDevEnpInfo, 0, MAX_DEVICE_NUM*MAX_ENDPOINT_NUM*sizeof(DEV_ENP_STATUS));
    
    memset(arrTemp, 0, ROW_LENGTH*sizeof(char));
    
    while(!bStatus)
    {
        pStatus = fgets(arrTemp, ROW_LENGTH, fpData);
        if (pStatus == NULL)
        {
            printf("Not find Description or Device or Data or Time!\n");
            return -1;
        }   
        bStatus = ((strstr(arrTemp, "Description") != NULL) && (strstr(arrTemp, "Device") != NULL)\
                                                            && (strstr(arrTemp, "Data") != NULL)\
                                                            && (strstr(arrTemp, "Time") != NULL));
    }
    FileSprintf(pArrResultFileName, "%s", arrTemp);

    stOffset.ui16PhaseOff = strstr(arrTemp, "Phase") - strstr(arrTemp, "Device");
    stOffset.ui16DataOff = strstr(arrTemp, "Data") - strstr(arrTemp, "Device");
    stOffset.ui16DescriptOff = strstr(arrTemp, "Description") - strstr(arrTemp, "Device");
    stOffset.ui16CmdPhaseOff = strstr(arrTemp, "Cmd.Phase.Ofs(rep)") - strstr(arrTemp, "Device");
    stOffset.ui16TimeOff = strstr(arrTemp, "Time") - strstr(arrTemp, "Device");
    stOffset.ui16LenOff = strstr(arrTemp, "Length") - strstr(arrTemp, "Device");


    pStatus = fgets(arrTemp, ROW_LENGTH, fpData);
    if (pStatus == NULL)
    {
        printf("Read no data!\n");
        return -1;
    }
    FileSprintf(pArrResultFileName, "%s", arrTemp);

    stOffset.ui16DataLength = CalSegLength(arrTemp + stOffset.ui16DataOff);
    stOffset.ui16PhaseLength = CalSegLength(arrTemp + stOffset.ui16PhaseOff);
    stOffset.ui16CmdPhaseLength = CalSegLength(arrTemp + stOffset.ui16CmdPhaseOff);
    stOffset.ui16TimeLength = CalSegLength(arrTemp + stOffset.ui16TimeOff);
    stOffset.ui16LenLength = CalSegLength(arrTemp + stOffset.ui16LenOff);


    //根据CmdPhase特征提取各个单独数据模块
    while(pTemp == NULL)
    {
        pStatus = fgets(arrTemp, ROW_LENGTH, fpData);
        if (pStatus == NULL)
        {
            printf("Not find data\n");
            return -1;
        }
        memset(arrCmdPhase, 0, ARR_CMD_PHASE_LENGTH);
        memcpy(arrCmdPhase, arrTemp + stOffset.ui16CmdPhaseOff, stOffset.ui16CmdPhaseLength);
        arrCmdPhase[stOffset.ui16CmdPhaseLength] = '\0';
        pTemp = strstr(arrCmdPhase, ".");
    }

    uint16_t ui16CmdPhaseOff = pTemp - arrCmdPhase;
    char arrCMP1[ARR_CMD_PHASE_LENGTH];
    char arrCMP2[ARR_CMD_PHASE_LENGTH];
    
    memset(arrCMP1, 0, ARR_CMD_PHASE_LENGTH);
    memset(arrCMP2, 0, ARR_CMD_PHASE_LENGTH);
    memcpy(arrCMP1, arrTemp + stOffset.ui16CmdPhaseOff, ui16CmdPhaseOff);
    arrCMP1[ui16CmdPhaseOff] = '\0';

    uint16_t ui16LoopOverFlow = 0;
    //结束条件有两个：1 读取文件末尾； 2 模块数超过LOG_BLOCK_NUM则退出解析;
    while ((pStatus != NULL) && (ui16LoopOverFlow < LOG_BLOCK_NUM))
    {
        int nflagCmdPhase = 0;
        //条件1:CmdPhase比较是否为一个命令块; 条件2:一个模块超过MODULE_LINE_MAX行，则认为是图像数据，后续不进行命令解析
        while(nflagCmdPhase == 0) 
        {
        	if (nIndex > CMD_LINE_NUM)
        	{
        		printf("Beyond CMD_LINE_NUM Rows !!!\n");
        		return -1;
        	}
            memcpy(pArrStr[nIndex++], arrTemp, ROW_LENGTH); 
            memset(arrTemp, 0, ROW_LENGTH*sizeof(char));
            pStatus = fgets(arrTemp, ROW_LENGTH, fpData);
            if (pStatus == NULL)
            {
                break;
            }
            memcpy(arrCMP2, arrTemp + stOffset.ui16CmdPhaseOff, ui16CmdPhaseOff);
            arrCMP2[ui16CmdPhaseOff] = '\0';
            nflagCmdPhase = strcmp(arrCMP1, arrCMP2);
        }
        strcpy(arrCMP1, arrCMP2);
        if (strstr(pArrStr[0], "55 33 56") == NULL)
        {
            FileSprintf(pArrResultFileName, "%s", pArrStr[0]);
        }
        else
        {
            for (int i = 0; i < nIndex; ++i)
            {
                FileSprintf(pArrResultFileName, "%s", pArrStr[i]);
            }
        }
        
        ModuleAnalysis(pArrStr, nIndex, arrDevEnpInfo, stOffset, pArrResultFileName);
        nIndex = 0;
        ui16LoopOverFlow++;
    }

    return 0;
}   

int main(int argc, char const *argv[])
{
	if (argv[1] == NULL)
	{
		printf("Missing Parm Error: \nPlease Input as \"./logAnalysis *.txt\"\n");
		return 0;
	}
    FILE *fpData = fopen(argv[1], "r");  //只读打开文件
    if (fpData == NULL)
    {
        printf("File Open Fail !!!");
        fclose(fpData);
        fpData = NULL;
        return 0;
    }

    //创建动态二维数组，保存图像数据
    char **pArrStr = new char*[CMD_LINE_NUM];
    for (int i = 0; i < CMD_LINE_NUM; ++i)
    {
        pArrStr[i] = new char[ROW_LENGTH];
    }

    //创建解析结果保存文件
    char arrResultFileName[FILENAME_SIZE];
    char arrNameTemp[FILENAME_SIZE];
    strcpy(arrNameTemp, argv[1]);
    arrNameTemp[strlen(argv[1])] = '\0';
    char *pDot = strstr(arrNameTemp, ".");
    if (pDot == NULL)
    {
    	printf("Filename not find dot!!!\n");
    	return 0;
    }
    *pDot = '\0';
    sprintf(arrResultFileName, "./%s_result.txt", arrNameTemp);
    FILE *fpResult = fopen(arrResultFileName, "w+");
    if (fpResult == NULL)
    {
        printf("Create result file fail!!!\n");
        return 0;
    }
    fclose(fpResult);
    fpResult = NULL;

    int status = LogAnalysis(fpData, pArrStr, arrResultFileName);
    if (status != 0)
    {
        printf("LogAnalysis Error\n");
    }

    //删除二维数组
    for (int i = 0; i < CMD_LINE_NUM; ++i)
    {
        delete pArrStr[i];        
    }
    delete pArrStr;
    pArrStr = NULL;

    fclose(fpData);
    fpData = NULL;
    return 0;
}
