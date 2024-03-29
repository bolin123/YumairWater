#include "GPRS.h"
#include "GNSS.h"
#include "HalGPIO.h"
#include "VTList.h"
#include "VTStaticQueue.h"
#include "SysTimer.h"
#include "PowerManager.h"

#define GPRS_UART_BAUDRATE  9600
#define GPRS_KEYWORD_MAX_NUM 4
#define SIM_ICCID_LEN 20

#define GPRS_ENTER_SLEEP_MODE() HalGPIOSetLevel(HAL_GPRS_SLEEP_PIN, HAL_GPRS_SLEEP_ENTRY_LEVEL)
#define GPRS_QUIT_SLEEP_MODE() HalGPIOSetLevel(HAL_GPRS_SLEEP_PIN, HAL_GPRS_SLEEP_EXIT_LEVEL)

#define GPRS_DATA_LIST_MAX_NUM  10
#define GPRS_TCP_SOCKET_MAX_NUM 4

//AT指令超时时间
#define GPRS_CMD_TIMEOUT_SHORT  2000
#define GPRS_CMD_TIMEOUT_NORMAL 5000
#define GPRS_CMD_TIMEOUT_LONG   10000
#define GPRS_CMD_TIMEOUT_MAX    20000

#define GPRS_ACK_OK "OK"
#define CARRIER_NAME_CHINA_MOBILE "CHINA MOBILE"
#define CARRIER_NAME_CHINA_UNICOM "CHN-UNICOM"

#define FOUND_SEGMENT(text, seg) (strstr(text, seg) != NULL)
/*
AT

ATE0
AT+CCID
AT+COPS?
AT+CREG?

AT+CIPMUX=1
AT+CSTT="CMNET"
AT+CIICR
AT+CIFSR

AT+CIPSTART=0,"TCP","login.machtalk.net",7779

AT+CSQ
AT+CIPSHUT
AT+CFUN=0
AT+CFUN=1

AT+CGNSPWR=1/0
AT+CGNSSEQ="RMC"
AT+CGNSINF
*/

typedef enum
{
    CARRIER_NONE = 0,
    CARRIER_CHINA_MOBILE,
    CARRIER_CHINA_UNICOM,
}GPRSCarrier_t;

typedef struct ATSendList_st
{
    char *atcmd;
    uint8_t len;
    uint8_t cid;
    uint8_t retries;
    uint8_t count;
    uint16_t timeout;
    SysTime_t lastTime;
    VTLIST_ENTRY(struct ATSendList_st);
}ATCmdSendList_t;

typedef struct ATDataSendList_st
{
    uint8_t id;
    uint16_t length;
    uint8_t *contents;
    VTLIST_ENTRY(struct ATDataSendList_st);
}ATDataSendList_t;

typedef enum
{
    CMD_ID_NONE = 0,

    /*GPRS*/
    //power on
    CMD_ID_AT,

    //GSM regist
    CMD_ID_ATE,
    CMD_ID_CSCLK,
    CMD_ID_CCID,
    CMD_ID_COPS,
    CMD_ID_CREG,

    //GPRS regist
    CMD_ID_CIPMUX,
    CMD_ID_CSTT,
    CMD_ID_CIICR,
    CMD_ID_CIFSR,

    //Net connect
    CMD_ID_CIPSTART,
    CMD_ID_CIPSEND,

    //csq
    CMD_ID_CSQ,

    //Net close
    CMD_ID_CIPCLOSE,
    CMD_ID_CIPSHUT,

    //GSM reboot
    CMD_ID_CFUN0,
    CMD_ID_CFUN1,

    CMD_ID_GPRS_COUNT,

    /*GPS*/
    CMD_ID_CGNSPWR_ON,
    CMD_ID_CGNSPWR_OFF,
    CMD_ID_CGNSSEQ, //set format
    CMD_ID_CGNSINF, //query
}GPRSCmdID_t;

typedef void (*keyWordHandle_t)(const char *context);
typedef struct
{
    char *keyWord;
    keyWordHandle_t handle;
}GPRSKeyword_t;

static GPRSKeyword_t g_keyWordInfo[GPRS_KEYWORD_MAX_NUM];
static GPRSCmdID_t g_currentCmdID = CMD_ID_NONE;
static GPRSStatus_t g_status = GPRS_STATUS_NONE;
static SysTime_t g_statusChangeTime;
static GPRSCarrier_t g_carrier = CARRIER_NONE;
static char g_iccid[SIM_ICCID_LEN + 1] = "";
static bool g_poweron = false;
static bool g_startPowerSwitch = false;
static bool g_gprsComOk = false;

static ATCmdSendList_t g_atCmdList;
static ATDataSendList_t g_atDataList;
static bool g_dataSendStart = false;
static bool g_sendFlagGot = false;

static VTSQueueDef(uint8_t, g_recvQueue, HAL_GPRS_RECV_QUEUE_LEN);
static uint8_t g_cmdBuff[128];
static volatile bool g_tcpDataRecv = false;
static uint8_t g_tcpRecvId = 0xff;

static GPRSEventHandle_cb g_eventCallback = NULL;
static uint8_t g_csqValue = 0;
//static bool g_recvMode = false;

static uint8_t g_gnssFixCount = 0;
static GNSSLocation_t g_gnssLocation;
static bool g_gnssStart = false;
static SysTime_t g_gnssStartTime;
static GNSSEventHandle_cb g_gnssEventHandle = NULL;

static GPRSTcpSocket_t *g_tcpSocket[GPRS_TCP_SOCKET_MAX_NUM] = {0};

static void deactKeywordHandle(const char *text);

#if 0
static char *jumpBlank(char *text)
{
    uint8_t i = 0;
    char *pos = text;

    while(pos[i] != '\0')
    {
        if(pos[i] != ' ')
        {
            return &pos[i];
        }
        i++;
    }
    return &pos[i];
}
#endif

static void delayMs(uint32_t time)
{
    uint32_t now = SysTime();

    while(!SysTimeHasPast(now, time));
}

static void lowDataSend(uint8_t *data, uint16_t len)
{
    //wake up
    GPRS_QUIT_SLEEP_MODE();
    delayMs(100);
    HalUartWrite(HAL_UART_GPRS_PORT, data, len);
    //sleep
    GPRS_ENTER_SLEEP_MODE();
}

static void gprsCmdSend(const char *atcmd, bool needATP, GPRSCmdID_t cid, uint8_t retries, uint16_t timeout)
{
    ATCmdSendList_t *atNode = (ATCmdSendList_t *)SysMalloc(sizeof(ATCmdSendList_t));
    char *atPlus;

    //GPRS_QUIT_SLEEP_MODE();

    if(atNode != NULL)
    {
        if(needATP)
        {
            atNode->len = strlen(atcmd) + 5; //"AT+" + "\r\0" = 5byte
            atPlus = "AT+";
        }
        else
        {
            atNode->len = strlen(atcmd) + 2; //"\r\0"
            atPlus = "";
        }

        atNode->atcmd = SysMalloc(atNode->len); //atcmd +\r
        if(atNode->atcmd != NULL)
        {
            sprintf(atNode->atcmd, "%s%s\r", atPlus, atcmd);
        }
        atNode->cid = cid;
        atNode->retries = retries;
        atNode->count = 0;
        atNode->timeout = timeout;
        atNode->lastTime = 0;
        VTListAdd(&g_atCmdList, atNode);
    }
}

static void eventEmitter(GPRSEvent_t event, void *args)
{
    SysLog("event = %d", event);
    if(g_eventCallback != NULL)
    {
        g_eventCallback(event, args);
    }
}

static void gprsStatusSet(GPRSStatus_t status)
{
    if(g_status != status)
    {
        g_statusChangeTime = SysTime();
        eventEmitter(GEVENT_GPRS_STATUS_CHANGED, (void *)status);
    }
    g_status = status;
}

static void clearAllCmd(void)
{
    ATCmdSendList_t *atcmd;

    g_dataSendStart = false;

    SysLog("");
    VTListForeach(&g_atCmdList, atcmd) //reset cmd list
    {
        VTListDel(atcmd);
        if(atcmd->atcmd)
        {
            free(atcmd->atcmd);
        }
        free(atcmd);
    }

}

static void atCmdTimeoutHandle(GPRSCmdID_t cid)
{
    SysLog("cid = %d", cid);

    if(cid > CMD_ID_GPRS_COUNT) //仅对gprs相关指令超时处理
    {
        return;
    }

    clearAllCmd();
    switch(g_status)
    {
        case GPRS_STATUS_NONE:
            //gprsPowerSwitch(); //power on
            eventEmitter(GEVENT_POWER_OFF, NULL);
            break;
        case GPRS_STATUS_INITIALIZED:
        case GPRS_STATUS_ACTIVE:
        case GPRS_STATUS_GSM_DONE:
            gprsCmdSend("CIPSHUT", true, CMD_ID_CIPSHUT, 3, GPRS_CMD_TIMEOUT_LONG);
            gprsStatusSet(GPRS_STATUS_NONE);
            break;
        case GPRS_STATUS_GPRS_DONE:

            gprsCmdSend("CIPSHUT", true, CMD_ID_CIPSHUT, 3, GPRS_CMD_TIMEOUT_LONG);
            gprsStatusSet(GPRS_STATUS_GSM_DONE);
            break;
        default:
            break;
    }
}

static void delAtListNode(ATCmdSendList_t *node)
{
    if(node != NULL)
    {
        VTListDel(node);
        if(node->atcmd != NULL)
        {
           //SysPrintf("Del:%s\n", node->atcmd);
           free(node->atcmd);
        }
        free(node);
        g_currentCmdID = CMD_ID_NONE;
    }
}

static void delCurrentAtNode(void)
{
    delAtListNode(VTListFirst(&g_atCmdList));
}

static void delTcpDataListNode(ATDataSendList_t *node)
{
    if(node != NULL)
    {
        VTListDel(node);
        if(node->contents != NULL)
        {
           free(node->contents);
        }
        free(node);
        //g_atDataListCount--;
    }
}

static void clearData(uint8_t socketId)
{
    ATDataSendList_t *node;

    VTListForeach(&g_atDataList, node)
    {
        if(node->id == socketId)
        {
            VTListDel(node);
            if(node->contents)
            {
                free(node->contents);
            }
            free(node);
        }
    }
}

static void socketClosed(uint8_t socketId)
{
    clearData(socketId);
    if(g_tcpSocket[socketId])
    {
        g_tcpSocket[socketId]->sendFailedNum = 0;
        g_tcpSocket[socketId]->connected = false;
        g_tcpSocket[socketId]->disconnetCb(socketId);
    }
}

static void atCmdSendListPoll(void)
{
    ATCmdSendList_t *atcmd = VTListFirst(&g_atCmdList);
    ATDataSendList_t *data;
    uint8_t socketId;
    GPRSCmdID_t cid;

    if(atcmd != NULL)
    {
        if(atcmd->lastTime == 0 || SysTimeHasPast(atcmd->lastTime, atcmd->timeout))
        {
            if(atcmd->count >= atcmd->retries) //超出重发次数
            {
                if(atcmd->retries > 1)
                {
                    cid = (GPRSCmdID_t)atcmd->cid;
                    delAtListNode(atcmd);
                    atCmdTimeoutHandle(cid);
                }
                else
                {
                    if(g_currentCmdID == CMD_ID_CIPSEND)
                    {
                        data = VTListFirst(&g_atDataList);

                        if(data)
                        {
                            if(!g_sendFlagGot && GPRSConnected())
                            {
                                lowDataSend(data->contents, data->length);
                            }

                            socketId = data->id;
                            g_tcpSocket[socketId]->sendFailCb(socketId, data->contents, data->length);
                            delTcpDataListNode(data);
                            g_tcpSocket[socketId]->sendFailedNum++;
                            if(g_tcpSocket[socketId]->sendFailedNum >= 3) //连续发送数据三次失败，认为链接断开
                            {
                                SysLog("send failed >= 3 times, socket maybe disconnect!");
                                //socketClosed(socketId);
                                deactKeywordHandle(NULL);
                            }
                        }

                        g_sendFlagGot = false;
                        g_dataSendStart = false;
                    }
                    delAtListNode(atcmd);
                }
            }
            else
            {
                g_currentCmdID = (GPRSCmdID_t)atcmd->cid;
                SysPrintf("%s\n", atcmd->atcmd);
                lowDataSend((uint8_t *)atcmd->atcmd, atcmd->len);
                atcmd->lastTime = SysTime();
                atcmd->count++;
            }
        }
    }
}

static void parseNMEAData(const char *nmea)
{
    char *pos;
    char utcTime[20] = {0};
    long long int time;
    int msec;

//+CGNSINF: 1,0,19800106001554.000,,,,0.00,0.0,0,,,,,,0,0,,,,,

    pos = strchr(nmea, ',');
    if(pos)
    {
        if(*(pos -1) == '1') //power on?
        {
            if(*(pos + 1) == '1')
            {
                g_gnssFixCount++;
            }
            else
            {
                g_gnssFixCount = 0;
            }

            pos = strchr(pos + 1, ',');
            memcpy(utcTime, pos + 1, 18);
            SysPrintf("utc:%s\n", utcTime);
            sscanf(utcTime, "%lld.%d", &time, &msec);
            SysPrintf("time:%lld, msec:%d\n", time, msec);

            g_gnssLocation.time.year = time / 10000000000;
            time = time % 10000000000;
            g_gnssLocation.time.month = time / 100000000;
            time = time % 100000000;
            g_gnssLocation.time.day = time / 1000000;
            time = time % 1000000;
            g_gnssLocation.time.hour = time / 10000;
            g_gnssLocation.time.hour = (g_gnssLocation.time.hour + 8) % 24;
            time = time % 10000;
            g_gnssLocation.time.min = time / 100;
            time = time % 100;
            g_gnssLocation.time.sec = time;

            pos = strchr(pos + 1, ',');//Latitude & Longitude
            sscanf(pos, ",%f,%f,", &g_gnssLocation.location.latitude, &g_gnssLocation.location.longitude);

            if(g_gnssFixCount >= 3)
            {
                g_gnssEventHandle(GEVENT_GNSS_FIXED, &g_gnssLocation);
            }
        }
    }
}

static bool filterKeyword(const char *text)
{
    uint8_t i;

    for(i = 0; i < GPRS_KEYWORD_MAX_NUM; i++)
    {
        if(strstr(text, g_keyWordInfo[i].keyWord) != NULL)
        {
            if(g_keyWordInfo[i].handle)
            {
                g_keyWordInfo[i].handle(text);
                return true;
            }
        }
    }

    return false;
}

static void receiveContentsParse(const char *text)
{
    bool delCurrentNode = false;
    char *pos = NULL;
    uint8_t socketId;
    ATDataSendList_t *data = NULL;

    SysPrintf("%s\n", text);
    if(filterKeyword(text))
    {
        return;
    }

    switch(g_currentCmdID)
    {
        case CMD_ID_AT:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                g_poweron = true;
                eventEmitter(GEVENT_POWER_ON, NULL);
                gprsStatusSet(GPRS_STATUS_INITIALIZED);
                g_gprsComOk = true;
                delCurrentNode = true;
                gprsCmdSend("CFUN=1", true, CMD_ID_CFUN1, 5, GPRS_CMD_TIMEOUT_NORMAL);
            }
            break;
        case CMD_ID_ATE:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                gprsCmdSend("CSCLK=1", true, CMD_ID_CSCLK, 3, GPRS_CMD_TIMEOUT_SHORT);
            }
            break;
        case CMD_ID_CSCLK:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                gprsCmdSend("CCID", true, CMD_ID_CCID, 5, GPRS_CMD_TIMEOUT_SHORT);
            }
            break;
        case CMD_ID_CCID:
            if(text[0] == '8' && text[1] == '9')
            {
                memcpy(g_iccid, text, SIM_ICCID_LEN);
                g_iccid[SIM_ICCID_LEN] = '\0';
                delCurrentNode = true;
                gprsCmdSend("COPS?", true, CMD_ID_COPS, 10, GPRS_CMD_TIMEOUT_SHORT);
            }
            break;
        case CMD_ID_COPS:
            if(FOUND_SEGMENT(text, CARRIER_NAME_CHINA_MOBILE))
            {
                g_carrier = CARRIER_CHINA_MOBILE;
                delCurrentNode = true;
                gprsCmdSend("CREG?", true, CMD_ID_CREG, 10, GPRS_CMD_TIMEOUT_SHORT);
            }
            else if(FOUND_SEGMENT(text, CARRIER_NAME_CHINA_UNICOM))
            {
                g_carrier = CARRIER_CHINA_UNICOM;
                delCurrentNode = true;
                gprsCmdSend("CREG?", true, CMD_ID_CREG, 10, GPRS_CMD_TIMEOUT_SHORT);
            }
            else
            {
                //SysPrintf("Carrier not support!\n");
            }
            break;
        case CMD_ID_CREG:
            if(FOUND_SEGMENT(text, "+CREG"))
            {
                pos = strchr(text, ',');
                if(*(pos+1) == '1' || *(pos+1) == '5')
                {
                    gprsStatusSet(GPRS_STATUS_GSM_DONE);
                    delCurrentNode = true;
                    gprsCmdSend("CIPMUX=1", true, CMD_ID_CIPMUX, 5, GPRS_CMD_TIMEOUT_SHORT);
                }
            }
            break;
        case CMD_ID_CIPMUX:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                if(g_carrier == CARRIER_CHINA_MOBILE)
                {
                    gprsCmdSend("CSTT=\"CMNET\"", true, CMD_ID_CSTT, 5, GPRS_CMD_TIMEOUT_SHORT);
                }
                else if(g_carrier == CARRIER_CHINA_UNICOM)
                {
                    gprsCmdSend("CSTT=\"UNINET\"", true, CMD_ID_CSTT, 5, GPRS_CMD_TIMEOUT_SHORT);
                }
                else
                {
                    SysPrintf("No carrier!\n");
                }
            }
            break;
        case CMD_ID_CSTT:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                gprsCmdSend("CIICR", true, CMD_ID_CIICR, 5, GPRS_CMD_TIMEOUT_LONG);
            }
            break;
        case CMD_ID_CIICR:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                gprsCmdSend("CIFSR", true, CMD_ID_CIFSR, 5, GPRS_CMD_TIMEOUT_NORMAL);
            }
            break;
        case CMD_ID_CIFSR:
            if(text[0] > '0' && text[0] < '9')
            {
                delCurrentNode = true;
                SysPrintf("Got IP:%s", text);
                gprsStatusSet(GPRS_STATUS_GPRS_DONE);
            }
            break;
        case CMD_ID_CIPSTART:
            if(FOUND_SEGMENT(text, "CONNECT OK"))// || FOUND_SEGMENT(text, "ALREADY CONNECT"))
            {
                delCurrentNode = true;
                socketId = text[0] - '0';
                if(g_tcpSocket[socketId])
                {
                    g_tcpSocket[socketId]->sendFailedNum = 0;
                    g_tcpSocket[socketId]->connected = true;
                    g_tcpSocket[socketId]->connectCb(socketId, true);
                }
            }
            if(FOUND_SEGMENT(text, "CONNECT FAIL"))
            {
                delCurrentNode = true;
                socketId = text[0] - '0';
                if(socketId < GPRS_TCP_SOCKET_MAX_NUM && g_tcpSocket[socketId])
                {
                    g_tcpSocket[socketId]->sendFailedNum = 0;
                    g_tcpSocket[socketId]->connected = false;
                    g_tcpSocket[socketId]->connectCb(socketId, false);
                }

            }
            break;
        case CMD_ID_CIPSEND:
            data = VTListFirst(&g_atDataList);
            if(FOUND_SEGMENT(text, ">"))
            {
                if(data)
                {
                    SysPrintf("\n%s\n", data->contents);
                    lowDataSend(data->contents, data->length);
                }
                g_sendFlagGot = true;
            }
            if(FOUND_SEGMENT(text, "SEND OK"))
            {
                if(data)
                {
                    g_tcpSocket[data->id]->sendFailedNum = 0;
                    delTcpDataListNode(data);
                }
                delCurrentNode = true;
                g_sendFlagGot = false;
                g_dataSendStart = false;
            }
            break;
        case CMD_ID_CSQ:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
            }
            if(FOUND_SEGMENT(text, "+CSQ"))
            {
                pos = strchr(text, ':');
                g_csqValue = strtol(pos + 1, NULL, 10);
            }
            break;
        case CMD_ID_CIPSHUT:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                if(g_status == GPRS_STATUS_GSM_DONE)
                {
                    gprsCmdSend("CIPMUX=1", true, CMD_ID_CIPMUX, 5, GPRS_CMD_TIMEOUT_SHORT);
                }
                else
                {
                    gprsCmdSend("CFUN=0", true, CMD_ID_CFUN0, 5, GPRS_CMD_TIMEOUT_NORMAL);
                }
            }
            break;
        case CMD_ID_CIPCLOSE:
            if(FOUND_SEGMENT(text, "CLOSE OK"))
            {
                delCurrentNode = true;
                socketId = text[0] - '0';
                socketClosed(socketId);
                #if 0
                if(g_tcpSocket[socketId])
                {
                    g_tcpSocket[socketId]->sendFailedNum = 0;
                    g_tcpSocket[socketId]->connected = false;
                    g_tcpSocket[socketId]->disconnetCb(socketId);
                }
                #endif
            }
            break;
        case CMD_ID_CFUN0:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                gprsStatusSet(GPRS_STATUS_INITIALIZED);
                gprsCmdSend("CFUN=1", true, CMD_ID_CFUN1, 5, GPRS_CMD_TIMEOUT_NORMAL);
            }
            break;
        case CMD_ID_CFUN1:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                gprsStatusSet(GPRS_STATUS_ACTIVE);
                gprsCmdSend("ATE0", false, CMD_ID_ATE, 5, GPRS_CMD_TIMEOUT_SHORT);
            }
            break;
        case CMD_ID_CGNSPWR_ON:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                delCurrentNode = true;
                //AT+CGNSSEQ="RMC"
                gprsCmdSend("CGNSSEQ=\"RMC\"", true, CMD_ID_CGNSSEQ, 3, GPRS_CMD_TIMEOUT_SHORT);
            }
            break;
        case CMD_ID_CGNSPWR_OFF:
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                //g_gnssStart = false;
                delCurrentNode = true;
                g_gnssEventHandle(GEVENT_GNSS_STOP, NULL);
            }
            break;
        case CMD_ID_CGNSSEQ: //set format
            if(FOUND_SEGMENT(text, GPRS_ACK_OK))
            {
                //g_gnssStart = true;
                delCurrentNode = true;
                g_gnssEventHandle(GEVENT_GNSS_START, NULL);
            }
            break;
        case CMD_ID_CGNSINF:
            if(FOUND_SEGMENT(text, "+CGNSINF:"))
            {
                parseNMEAData(text);
                delCurrentNode = true;
            }
            break;
        default:
            break;
    }

    if(delCurrentNode)
    {
        delCurrentAtNode();
    }
}

static void gprsDataParsePoll(void)
{
    uint8_t byte;
    GPRSTcpSocket_t *socket;
    static uint8_t dataCount = 0;

    while(VTSQueueCount(g_recvQueue))
    {
        SysInterruptSet(false);
        byte = VTSQueueFront(g_recvQueue);
        VTSQueuePop(g_recvQueue);
        SysInterruptSet(true);

        if(g_tcpDataRecv)
        {
            if(g_status == GPRS_STATUS_GPRS_DONE && g_tcpSocket[g_tcpRecvId])
            {
                socket = g_tcpSocket[g_tcpRecvId];
                socket->data[socket->recvCount++] = byte;
                if(socket->recvCount == socket->dataLen)
                {
                    g_tcpDataRecv = false;
                    socket->data[socket->recvCount] = '\0';//字符串结束符
                    socket->recvCb(g_tcpRecvId, socket->data, socket->dataLen);
                    if(socket->data)
                    {
                        free(socket->data);
                        socket->data = NULL;
                    }
                    socket->dataLen = 0;
                    socket->recvCount = 0;
                    break;
                }
            }
            else
            {
                g_tcpDataRecv = false;
            }
        }
        else
        {
            g_cmdBuff[dataCount++] = byte;

            if(dataCount >= sizeof(g_cmdBuff))
            {
                dataCount = 0;
            }

            if(byte == '\n' || byte == '>')
            {
                if(dataCount == 2 && g_cmdBuff[0] == '\r') //ignore null
                {
                    dataCount = 0;
                    break;
                }
                else
                {
                    g_cmdBuff[dataCount] = '\0';
                    receiveContentsParse((const char *)g_cmdBuff);
                    dataCount = 0;
                    break;
                }
            }
        }
    }
}


#if 1
static void gprsDataRecv(uint8_t *data, uint16_t len)
{
    uint16_t i;

    for(i = 0; i < len; i++)
    {
        if(VTSQueueHasSpace(g_recvQueue))
        {
            VTSQueuePush(g_recvQueue, data[i]);
        }
    }
}
#endif

static void atCommunicationTest(void)
{
    SysLog("");
    gprsCmdSend("AT", false, CMD_ID_AT, 10, GPRS_CMD_TIMEOUT_SHORT);
}

static void receiveKeywordHandle(const char *text)
{
    char *pos = NULL;
    uint8_t sid;
    GPRSTcpSocket_t *socket;

    pos = strchr(text, ',');
    if(pos != NULL)
    {
        pos += 1;
        sid = strtol(pos, NULL, 10);
        if(sid >= GPRS_TCP_SOCKET_MAX_NUM)
        {
            return;
        }
    }

    socket = g_tcpSocket[sid];
    pos = strchr(pos, ',');
    if(pos != NULL)
    {
        pos += 1;
        if(socket != NULL)
        {
            socket->dataLen = strtol(pos, NULL, 10);
            if(socket->data)
            {
                free(socket->data);
                socket->data = NULL;
            }
            socket->recvCount = 0;
            socket->data = SysMalloc(socket->dataLen + 1);
        }
        g_tcpRecvId = sid;
        g_tcpDataRecv = true;

        PMDeviceWaittimeUpdate(20000);
    }
}

static void closedKeywordHandle(const char *text)
{
    uint32_t id = text[0] - '0';

    ATCmdSendList_t *node = VTListFirst(&g_atCmdList);

    if(node && node->cid == CMD_ID_CIPCLOSE)
    {
        delCurrentAtNode();
    }

    socketClosed(id);
    #if 0
    clearData(id);

    if(g_tcpSocket[id])
    {
        g_tcpSocket[id]->sendFailedNum = 0;
        g_tcpSocket[id]->connected = false;
        g_tcpSocket[id]->disconnetCb(id);
    }
    #endif
}

static void deactKeywordHandle(const char *text)
{
    uint8_t i;

    gprsStatusSet(GPRS_STATUS_GSM_DONE);
    gprsCmdSend("CIPSHUT", true, CMD_ID_CIPSHUT, 3, GPRS_CMD_TIMEOUT_LONG);
    for(i = 0; i < GPRS_TCP_SOCKET_MAX_NUM; i++)
    {
        socketClosed(i);
    #if 0
        if(g_tcpSocket[i] != NULL)
        {
            g_tcpSocket[i]->sendFailedNum = 0;
            g_tcpSocket[i]->connected = false;
            g_tcpSocket[i]->disconnetCb(i);
        }
    #endif
    }
}

#if 0
static void powerOnKeywordHandle(const char *text)
{
    SysLog("");
    g_startPowerSwitch = false;
    g_poweron = true;
    eventEmitter(GEVENT_POWER_ON, NULL);
}
#endif

static void powerDownKeywordHandle(const char *text)
{
    SysLog("");
    //g_startPowerSwitch = false;
    clearAllCmd();
    eventEmitter(GEVENT_POWER_OFF, NULL);
    g_poweron = false;
}

static void gprsCSQValueQuery(void)
{
    static SysTime_t lastQueryTime = 0;

    if(g_status != GPRS_STATUS_NONE && SysTimeHasPast(lastQueryTime, 300000))
    {
        gprsCmdSend("CSQ", true, CMD_ID_CSQ, 1, GPRS_CMD_TIMEOUT_SHORT);
        lastQueryTime = SysTime();
    }
}

static void statusDetectPoll(void)
{
    /*未联网的情况下，如果某个状态持续2分钟未改变，则认为模块或信号异常*/
    if(g_status != GPRS_STATUS_GPRS_DONE && SysTimeHasPast(g_statusChangeTime, 120000))
    {
        SysLog("Error: status[%d] stay too long time!", g_status);
        eventEmitter(GEVENT_POWER_OFF, NULL);
        g_statusChangeTime = SysTime();
    }
}

static void gprsPowerSwitchDone(void *args)
{
    int power = (int)args;

    SysLog("");
    g_startPowerSwitch = false;
    HalGPIOSetLevel(HAL_GPRS_POWER_PIN, HAL_GPRS_POWER_DEACTIVE_LEVEL);

    if(power) //power on
    {
        atCommunicationTest();
    }
}

static void gprsPowerSwitch(int power)
{
    g_startPowerSwitch = true;
    gprsStatusSet(GPRS_STATUS_NONE);
    clearAllCmd();
    HalGPIOSetLevel(HAL_GPRS_POWER_PIN, HAL_GPRS_POWER_ACTIVE_LEVEL);
    //power active
    SysTimerSet(gprsPowerSwitchDone, 2000, 0, (void *)power);
}

static void gprsKeywordRegster(void)
{
    uint8_t i = 0;
    g_keyWordInfo[i].keyWord = "+RECEIVE";
    g_keyWordInfo[i].handle  = receiveKeywordHandle;
    i++;

    g_keyWordInfo[i].keyWord = "CLOSED";
    g_keyWordInfo[i].handle  = closedKeywordHandle;
    i++;

    g_keyWordInfo[i].keyWord = "DEACT";
    g_keyWordInfo[i].handle  = deactKeywordHandle;
    i++;

    g_keyWordInfo[i].keyWord = "POWER DOWN";
    g_keyWordInfo[i].handle  = powerDownKeywordHandle;
    i++;
#if 0
    g_keyWordInfo[i].keyWord = "+CFUN: 1";
    g_keyWordInfo[i].handle  = powerOnKeywordHandle;
    i++;
#endif
}

static void gprsDmaRecv(uint16_t count)
{
    g_recvQueue.count += count;
}

void GPRSConfigIrqRecv(void)
{
    HalUartConfig_t config;

    //HalUartDmaInit(g_recvQueue.items, gprsDmaRecv);
    config.baudrate = GPRS_UART_BAUDRATE;
    config.flowControl = 0;
    config.parity = 0;
    config.wordLength = USART_WordLength_8b;
    config.recvCb = gprsDataRecv;//gprsDataRecv;
    HalUartConfig(HAL_UART_GPRS_PORT, &config);
}

void GPRSConfigDmaRecv(void)
{
    HalUartConfig_t config;

    g_recvQueue.front = 0;
    g_recvQueue.back = 0;
    g_recvQueue.count = 0;
    config.baudrate = GPRS_UART_BAUDRATE;
    config.flowControl = 0;
    config.parity = 0;
    config.wordLength = USART_WordLength_8b;
    config.recvCb = NULL;//gprsDataRecv;
    HalUartDmaInit(HAL_UART_GPRS_PORT, &config, g_recvQueue.items, gprsDmaRecv);
}

const char *GPRSGetICCID(void)
{
    if(g_iccid[0] == 0)
    {
        return NULL;
    }
    return g_iccid;
}

int8_t GNSSStop(void)
{
    //CGNSPWR=0
    SysLog("");
    g_gnssFixCount = 0;
    g_gnssStart = false;
    if(g_poweron)
    {
        gprsCmdSend("CGNSPWR=0", true, CMD_ID_CGNSPWR_OFF, 3, GPRS_CMD_TIMEOUT_NORMAL);
        return 0;
    }
    return -1;
}

int8_t GNSSStart(void)
{
    //CGNSPWR=1
    g_gnssFixCount = 0;
    if(g_poweron)
    {
        SysLog("");
        g_gnssStart = true;
        g_gnssStartTime = SysTime();
        gprsCmdSend("CGNSPWR=1", true, CMD_ID_CGNSPWR_ON, 3, GPRS_CMD_TIMEOUT_NORMAL);
        return 0;
    }
    return -1;
}

static void gnssLocationQuery(void)
{
    static SysTime_t lastQueryTime = 0;
    if(g_gnssStart && SysTimeHasPast(lastQueryTime, 10000))//10s读一次定位信息
    {
        if(SysTimeHasPast(g_gnssStartTime, GNSS_FIX_TIME_MAX)) //3min超时
        {
            g_gnssEventHandle(GEVENT_GNSS_TIMEOUT, NULL);
        }
        else
        {
            gprsCmdSend("CGNSINF", true, CMD_ID_CGNSINF, 1, GPRS_CMD_TIMEOUT_NORMAL);
        }
        lastQueryTime = SysTime();
    }
}

void GNSSEventHandleRegister(GNSSEventHandle_cb handle)
{
    g_gnssEventHandle = handle;
}

bool GNSSLocationFixed(void)
{
    return (g_gnssFixCount >= 3);
}

GNSSLocation_t *GNSSGetLocation(void)
{
    return &g_gnssLocation;
}

void GPRSEventHandleRegister(GPRSEventHandle_cb handle)
{
    g_eventCallback = handle;
}

static void tcpDataSendHandle(void)
{
    ATDataSendList_t *atData;

    if(g_status == GPRS_STATUS_GPRS_DONE /*gprs 联网成功*/
        && !g_dataSendStart)/*没有消息在发送*/
    {
        atData = VTListFirst(&g_atDataList);

        if(atData != NULL)//有待发数据 & 已连接
        {
            char atSend[32] = {0};
            sprintf(atSend, "CIPSEND=%d,%d", atData->id, atData->length);
            gprsCmdSend(atSend, true, CMD_ID_CIPSEND, 1, GPRS_CMD_TIMEOUT_MAX);
            g_dataSendStart = true;
        }
    }
}

uint16_t GPRSTcpSend(GPRSTcpSocket_t *socket, uint8_t *data, uint16_t len)
{
    ATDataSendList_t *atData = NULL;
    ATDataSendList_t *node = NULL;
    uint8_t count = 0;

    if(socket == NULL)
    {
        SysLog("ERR: sock is null!");
        return 0;
    }

    VTListForeach(&g_atDataList, node)
    {
        count++;
    }
    if(count > GPRS_DATA_LIST_MAX_NUM)
    {
        SysLog("list full,%d", count);
        return 0;
    }
    //GPRS_QUIT_SLEEP_MODE();

    atData = (ATDataSendList_t *)SysMalloc(sizeof(ATDataSendList_t));
    if(atData != NULL)
    {
        atData->contents = SysMalloc(len + 1);
        if(atData->contents != NULL)
        {
            memcpy(atData->contents, data, len);
            atData->contents[len] = '\0'; //for print
            atData->length = len;
            atData->id = socket->socketId;

            VTListAdd(&g_atDataList, atData);

            PMDeviceWaittimeUpdate(20000);
            return len;
        }
    }

    return 0;
}

bool GPRSDatalistEmpty(void)
{
    return (VTListFirst(&g_atDataList) == NULL);
}

#if 0
static void sleepModePoll(void)
{
    if(GPRSDatalistEmpty() && VTListFirst(&g_atCmdList) == NULL)
    {
        GPRS_ENTER_SLEEP_MODE();
    }
}


void GPRSSetRecvMode(bool entry)
{
    g_recvMode = entry;
}
#endif

void GPRSTcpClose(GPRSTcpSocket_t *socket)
{
    char atClose[20];

    SysLog("%p", socket);
    if(socket)
    {
        clearData(socket->socketId);
        sprintf(atClose, "CIPCLOSE=%d,0", socket->socketId);
        gprsCmdSend(atClose, true, CMD_ID_CIPCLOSE, 1, GPRS_CMD_TIMEOUT_LONG);
    }
}

void GPRSTcpRelease(GPRSTcpSocket_t *socket)
{
    uint8_t id;

    if(socket != NULL)
    {
        id = socket->socketId;
        clearData(id);
        if(socket->data != NULL)
        {
            free(socket->data);
            socket->data = NULL;
        }
        free(socket);
        g_tcpSocket[id] = NULL;
    }
}

GPRSTcpSocket_t *GPRSTcpCreate(void)
{
    uint8_t i;

    for(i = 0; i < GPRS_TCP_SOCKET_MAX_NUM; i++)
    {
        if(g_tcpSocket[i] == NULL)
        {
            g_tcpSocket[i] = (GPRSTcpSocket_t *)SysMalloc(sizeof(GPRSTcpSocket_t));
            if(g_tcpSocket[i] != NULL)
            {
                g_tcpSocket[i]->socketId = i;
                g_tcpSocket[i]->sendFailedNum = 0;
                g_tcpSocket[i]->data = NULL;
                g_tcpSocket[i]->recvCb = NULL;
                g_tcpSocket[i]->connectCb = NULL;
                g_tcpSocket[i]->disconnetCb = NULL;
                SysLog("id = %d", i);
                return g_tcpSocket[i]; //socket id
            }
            else
            {
                return NULL;
            }
        }
    }
    return NULL;
}

int8_t GPRSTcpConnect(GPRSTcpSocket_t *socket, const char *url, uint16_t port)
{
    char atConnect[100];

    if(socket == NULL)
    {
        SysLog("ERR: socket is null!");
        return -1;
    }

    //AT+CIPSTART=0,"TCP","login.machtalk.net",7779
    sprintf(atConnect, "CIPSTART=%d,\"TCP\",\"%s\",%d", socket->socketId, url, port);
    gprsCmdSend(atConnect, true, CMD_ID_CIPSTART, 1, GPRS_CMD_TIMEOUT_MAX);
    return 0;
}

bool GPRSConnected(void)
{
    return (g_status == GPRS_STATUS_GPRS_DONE);
}

GPRSStatus_t GPRSGetStatus(void)
{
    return g_status;
}

uint8_t GPRSGetSignalValue(void)
{
    return g_csqValue;
}

bool IsGPRSComOk()
{
    return g_gprsComOk;
}

int8_t GPRSStop(void)
{
/*
    if(!g_poweron)
    {
        SysLog("already power off");
        return -1; //already power off
    }
*/
    if(g_startPowerSwitch)
    {
        SysLog("Busy...");
        return -2; //switching ...
    }

    gprsPowerSwitch(0);
    return 0;
}

int8_t GPRSStart(void)
{
/*
    if(g_poweron)
    {
        SysLog("already power on");
        return -1; //already power on
    }
*/
    if(g_startPowerSwitch)
    {
        SysLog("Busy...");
        return -2; //switching ...
    }

    SysLog("start ...");
    gprsPowerSwitch(1);
    return 0;
}

void GPRSInitialize(void)
{
    VTListInit(&g_atCmdList);
    VTListInit(&g_atDataList);
    GPRSConfigIrqRecv();
    //GPRSConfigDmaRecv();
    gprsKeywordRegster();

    HalGPIOConfig(HAL_GPRS_POWER_PIN, HAL_IO_OUTPUT);
    HalGPIOSetLevel(HAL_GPRS_POWER_PIN, HAL_GPRS_POWER_DEACTIVE_LEVEL);

    HalGPIOConfig(HAL_GPRS_SLEEP_PIN, HAL_IO_OUTPUT);
    GPRS_QUIT_SLEEP_MODE();
}

void GPRSPoll(void)
{
    atCmdSendListPoll();
    if(g_poweron)// && !g_recvMode)
    {
        gprsCSQValueQuery();
        tcpDataSendHandle();
        gnssLocationQuery();
        statusDetectPoll();
        //sleepModePoll();
    }
    gprsDataParsePoll();
}

