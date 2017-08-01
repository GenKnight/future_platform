/******************************************************************************
Copyright (c) 2016. All Rights Reserved.

FileName: Trade.cpp
Version: 1.0
Date: 2017.4.25

History:
shengkaishan     2017.4.25   1.0     Create
******************************************************************************/


#include "Trade.h"
#include "iTapAPIError.h"
#include "TradeConfig.h"
#include <iostream>
#include <string.h>
#include "common.h"
#include "applog.h"
#include "trader_server.h"
#include "future_platform.h"

using namespace std;
namespace future
{
    Trade::Trade(void) :
        m_pAPI(NULL),
        m_bIsAPIReady(false),
        m_bContract(false),
        m_bposition(false),
        m_border(false),
        m_connect_state(false),
        m_chk_thread(nullptr),
        m_running(true)
    {
        m_map_contract.clear();
        m_map_order.clear();
    }

    Trade::~Trade(void)
    {
        m_map_contract.clear();
        m_map_order.clear();
    }

    void Trade::SetAPI(ITapTradeAPI *pAPI)
    {
        m_pAPI = pAPI;
    }

    void Trade::Run()
    {
        if (NULL == m_pAPI) {
            cout << "Error: m_pAPI is NULL." << endl;
            return;
        }

        TAPIINT32 iErr = TAPIERROR_SUCCEED;

        //��¼������
        QString log_str = "���ڵ�¼���׷���";
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);
        string key = "trader_info/userid";
        QString userid = common::get_config_value(key).toString();
        key = "trader_info/passwd";
        QString passwd = common::getXorEncryptDecrypt(
            common::get_config_value(key).toString());

        TapAPITradeLoginAuth stLoginAuth;
        memset(&stLoginAuth, 0, sizeof(stLoginAuth));
        strcpy(stLoginAuth.UserNo, userid.toStdString().c_str());
        strcpy(stLoginAuth.Password, passwd.toStdString().c_str());
        stLoginAuth.ISModifyPassword = APIYNFLAG_NO;
        iErr = m_pAPI->Login(&stLoginAuth);
        if (TAPIERROR_SUCCEED != iErr) {
            cout << "Login Error:" << iErr << endl;
            return;
        }

        //�ȴ�APIReady
        m_Event.WaitEvent();
        if (!m_bIsAPIReady) {
            cout << "API is not ready";
            return;
        }

        //�õ����к�Լ
        log_str = "���ڻ�ȡ���׻������ݡ�����";
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);
        m_uiSessionID = 0;
        TapAPICommodity com;
        memset(&com, 0, sizeof(com));
        m_pAPI->QryContract(&m_uiSessionID, &com);
        //�ȴ�QryContract
        m_Event.WaitEvent();
        if (!m_bContract) {
            return;
        }

        qry_postion();
        //�ȴ�qry_postion
        m_Event.WaitEvent();
        if (!m_bposition) {
            return;
        }

        qry_order();
        m_Event.WaitEvent();
        if (!m_border) {
            return;
        }

        log_str = "���׷����¼���";
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);

        m_connect_state = true;
    }

    void Trade::qry_postion()
    {
        string key = "trader_info/userid";
        QString userid = common::get_config_value(key).toString();

        TapAPIPositionQryReq req;
        memset(&req, 0, sizeof(req));
        strcpy(req.AccountNo, userid.toStdString().c_str());

        TAPIINT32 iErr = TAPIERROR_SUCCEED;
        m_uiSessionID = 0;
        iErr = m_pAPI->QryPosition(&m_uiSessionID, &req);
        if (iErr != TAPIERROR_SUCCEED) {
            cout << "QryPosition Error:" << iErr << endl;
        }
    }

    void Trade::qry_order()
    {
        string key = "trader_info/userid";
        QString userid = common::get_config_value(key).toString();

        TapAPIOrderQryReq req;
        memset(&req, 0, sizeof(req));
        strcpy(req.AccountNo, userid.toStdString().c_str());

        TAPIINT32 iErr = TAPIERROR_SUCCEED;
        m_uiSessionID = 0;
        iErr = m_pAPI->QryOrder(&m_uiSessionID, &req);
        if (iErr != TAPIERROR_SUCCEED) {
            cout << "QryOrder Error:" << iErr << endl;
        }
    }


    void Trade::order_open(string& account, string& contract, double price)
    {
        QString log_str = QObject::tr("%1%2 %3").arg("�ҵ���").arg(contract.c_str()).arg(price);
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);

        TAPIINT32 iErr = TAPIERROR_SUCCEED;
        //�µ�
        TapAPINewOrder stNewOrder;

        int i = 0;
        for (i = 0; i < contract.length(); i++) {
            if (contract[i] > '0' && contract[i] < '9') {
                break;
            }
        }
        string commodity_no = contract.substr(0, i);
        string contract_no = contract.substr(i, contract.length() - i);

        strcpy(stNewOrder.AccountNo, account.c_str());
        strcpy(stNewOrder.ExchangeNo, m_map_contract[contract].c_str());
        stNewOrder.CommodityType = DEFAULT_COMMODITY_TYPE;
        strcpy(stNewOrder.CommodityNo, commodity_no.c_str());
        strcpy(stNewOrder.ContractNo, contract_no.c_str());
        //stNewOrder.CallOrPutFlag = TAPI_CALLPUT_FLAG_PUT;
        stNewOrder.OrderType = TAPI_ORDER_TYPE_LIMIT; //�м�/�ּ�
        stNewOrder.OrderSource = TAPI_ORDER_SOURCE_ESUNNY_API;
        stNewOrder.TimeInForce = TAPI_ORDER_TIMEINFORCE_GFD;
        stNewOrder.OrderSide = TAPI_SIDE_SELL; //buy/sell
        stNewOrder.PositionEffect = TAPI_PositionEffect_OPEN; //open/close
        stNewOrder.OrderPrice = price;
        stNewOrder.OrderQty = DEFAULT_ORDER_QTY; //ί������ default 1

        m_uiSessionID = 0;
        iErr = m_pAPI->InsertOrder(&m_uiSessionID, &stNewOrder);
        if (TAPIERROR_SUCCEED != iErr) {
            QString log_str = QObject::tr("%1%2").arg("����ʧ�ܣ�������:").
                arg(iErr);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            return;
        }
    }
    void Trade::order_withdraw()
    {
        TAPIINT32 iErr = TAPIERROR_SUCCEED;
        string key = "order_info/serverflag";
        TAPICHAR server_type = common::get_config_value(key).toString().toStdString()[0];
        key = "order_info/orderno";
        QString orderno = common::get_config_value(key).toString();

        QString log_str = QObject::tr("%1%2").arg("��������ˮ��:").arg(orderno);
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);

        //�µ�
        TapAPIOrderCancelReq CancelOrder;
        memset(&CancelOrder, 0, sizeof(CancelOrder));
        //CancelOrder.ServerFlag = server_type;
        strcpy(CancelOrder.OrderNo, orderno.toStdString().c_str());

        m_uiSessionID = 0;
        iErr = m_pAPI->CancelOrder(&m_uiSessionID, &CancelOrder);
        if (TAPIERROR_SUCCEED != iErr) {
            QString log_str = QObject::tr("%1%2").arg("����ʧ�ܣ�������:").
                arg(iErr);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            return;
        }
    }
    void Trade::order_close(string& account, string& contract)
    {
        QString log_str = QObject::tr("%1%2").arg("�м�ƽ��").arg(contract.c_str());
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);
        TAPIINT32 iErr = TAPIERROR_SUCCEED;

        int i = 0;
        for (i = 0; i < contract.length(); i++) {
            if (contract[i] > '0' && contract[i] < '9') {
                break;
            }
        }
        string commodity_no = contract.substr(0, i);
        string contract_no = contract.substr(i, contract.length() - i);

        //�µ�
        TapAPINewOrder stNewOrder;
        strcpy(stNewOrder.AccountNo, account.c_str());
        strcpy(stNewOrder.ExchangeNo, m_map_contract[contract].c_str());
        stNewOrder.CommodityType = DEFAULT_COMMODITY_TYPE;
        strcpy(stNewOrder.CommodityNo, commodity_no.c_str());
        strcpy(stNewOrder.ContractNo, contract_no.c_str());
        //stNewOrder.CallOrPutFlag = TAPI_CALLPUT_FLAG_CALL;
        stNewOrder.OrderType = TAPI_ORDER_TYPE_MARKET; //�м�/�ּ�
        stNewOrder.OrderSource = TAPI_ORDER_SOURCE_ESUNNY_API;
        stNewOrder.TimeInForce = TAPI_ORDER_TIMEINFORCE_GFD;
        stNewOrder.OrderSide = TAPI_SIDE_BUY; //buy/sell
        stNewOrder.PositionEffect = TAPI_PositionEffect_COVER; //open/close
        stNewOrder.OrderPrice = DEFAULT_ORDER_PRICE;
        stNewOrder.OrderQty = DEFAULT_ORDER_QTY; //ί������ default 1

        m_uiSessionID = 0;
        iErr = m_pAPI->InsertOrder(&m_uiSessionID, &stNewOrder);
        if (TAPIERROR_SUCCEED != iErr) {
            QString log_str = QObject::tr("%1%2").arg("��ƽʧ�ܣ�������:").
                arg(iErr);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            return;
        }
    }

    void Trade::order_state_handle(const TapAPIOrderInfoNotice *info)
    {
        //! ί��״̬����
        //TAPI_ORDER_STATE_SUBMIT = '0'; //! �ն��ύ
        //TAPI_ORDER_STATE_ACCEPT = '1';//! ������
        //TAPI_ORDER_STATE_EXCTRIGGERING = '3';//! ������������
        //TAPI_ORDER_STATE_QUEUED = '4';//! ���Ŷ�
        //TAPI_ORDER_STATE_PARTFINISHED = '5';//! ���ֳɽ�
        //TAPI_ORDER_STATE_FINISHED = '6';//! ��ȫ�ɽ�
        //TAPI_ORDER_STATE_CANCELING = '7';//! ������(�Ŷ���ʱ״̬)
        //TAPI_ORDER_STATE_CANCELED = '9';//! ��ȫ����
        //TAPI_ORDER_STATE_LEFTDELETED = 'A';//! �ѳ��൥
        //TAPI_ORDER_STATE_FAIL = 'B';//! ָ��ʧ��
        //TAPI_ORDER_STATE_SUPPENDED = 'D';//! �ѹ���
        switch (info->OrderInfo->OrderState) {
        case TAPI_ORDER_STATE_FAIL: 
        {
            emit signals_state_changed(TAPI_ORDER_STATE_FAIL,
                TAPI_PositionEffect_NONE,
                QString::number(0, 10, 0));

            QString log_str = QObject::tr("%1%2").arg("ί��ʧ��,��ˮ��:").
                arg(info->OrderInfo->OrderNo);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            break;
        }
        case TAPI_ORDER_STATE_ACCEPT:
        {
            //string key = "order_info/serverflag";
            //stringstream stream;
            //stream << info->OrderInfo->ServerFlag;
            //common::set_config_value(key, stream.str());
            //key = "order_info/orderno";
            //common::set_config_value(key, string(info->OrderInfo->OrderNo));
       
            //emit signals_state_changed(TAPI_ORDER_STATE_ACCEPT,
            //    TAPI_PositionEffect_NONE,
            //    QString::number(0, 10, 0));

            QString log_str = QObject::tr("%1%2").arg("ί��������,��ˮ��:").
                arg(info->OrderInfo->OrderNo);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            break;
        }
        case TAPI_ORDER_STATE_QUEUED:
        {
            string key = "order_info/serverflag";
            stringstream stream;
            stream << info->OrderInfo->ServerFlag;
            common::set_config_value(key, stream.str());
            key = "order_info/orderno";
            common::set_config_value(key, string(info->OrderInfo->OrderNo));

            emit signals_state_changed(TAPI_ORDER_STATE_QUEUED,
                TAPI_PositionEffect_NONE,
                QString::number(0, 10, 0));

            QString log_str = QObject::tr("%1%2").arg("ί�����Ŷ�,��ˮ��:").
                arg(info->OrderInfo->OrderNo);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            break;
        }
        case TAPI_ORDER_STATE_PARTFINISHED:
        {
            APP_LOG(applog::LOG_INFO) << "���ֳɽ�";
            break;
        }
        case TAPI_ORDER_STATE_FINISHED:
        {
            APP_LOG(applog::LOG_INFO) << "��ȫ�ɽ�";
            //emit signals_state_changed(TAPI_ORDER_STATE_FINISHED,
            //    TAPI_SIDE_NONE,
            //    QString::number(0, 10, 0));
            break;
        }
        case TAPI_ORDER_STATE_CANCELING:
        {
            APP_LOG(applog::LOG_INFO) << "������";
            break;
        }
        case TAPI_ORDER_STATE_CANCELED:
        {
            emit signals_state_changed(TAPI_ORDER_STATE_CANCELED,
                TAPI_PositionEffect_NONE,
                QString::number(0, 10, 0));

            QString log_str = QObject::tr("%1%2").arg("�����ɹ�,��ˮ��:").
                arg(info->OrderInfo->OrderNo);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            break;
        }
        case TAPI_ORDER_STATE_LEFTDELETED:
        {
            emit signals_state_changed(TAPI_ORDER_STATE_LEFTDELETED,
                TAPI_PositionEffect_NONE,
                QString::number(0, 10, 0));

            QString log_str = QObject::tr("%1%2").arg("�����ɹ�,��ˮ��:").
                arg(info->OrderInfo->OrderNo);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            break;
        }
        default:
            break;
        }
    }

    void TAP_CDECL Trade::OnConnect()
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspLogin(TAPIINT32 errorCode, const TapAPITradeLoginRspInfo *loginRspInfo)
    {
        if (TAPIERROR_SUCCEED == errorCode) {
            QString log_str = "��¼�ɹ����ȴ�API��ʼ��...";
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
        } else {
            QString log_str = QObject::tr("%1%2").arg("��¼ʧ�ܣ�������:").
                arg(errorCode);
            APP_LOG(applog::LOG_INFO) << log_str.toStdString();
            emit signals_write_log(log_str);
            m_Event.SignalEvent();
        }
    }

    void TAP_CDECL Trade::OnAPIReady()
    {
        QString log_str = "API��ʼ�����";
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);
        m_bIsAPIReady = true;
        m_Event.SignalEvent();
    }

    void Trade::thread_reconnect()
    {
        while (m_running) {
            if (!m_connect_state) {
                Run();
            }
            Sleep(3000);
        }
    }

    void TAP_CDECL Trade::OnDisconnect(TAPIINT32 reasonCode)
    {
        if (!m_running) return;
        QString log_str = QObject::tr("%1%2").arg("API�Ͽ�,�Ͽ�ԭ��:").
            arg(reasonCode);
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);

        m_connect_state = false;
        if (m_chk_thread != nullptr) return;
        m_chk_thread = std::make_shared<std::thread>(
            std::bind(&Trade::thread_reconnect, this));
    }
    //ί�С������ر�
    void TAP_CDECL Trade::OnRtnOrder(const TapAPIOrderInfoNotice *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
        if (NULL == info) return;

        if (info->ErrorCode != 0) {
            cout << "������������һ������ί����Ϣ�Ĵ���" << info->ErrorCode << endl;
        } else {
            //�����ر�
            if (info->OrderInfo &&
                (info->OrderInfo->OrderState == TAPI_ORDER_STATE_CANCELED ||
                info->OrderInfo->OrderState == TAPI_ORDER_STATE_LEFTDELETED)) {
                order_state_handle(info);
            }

            if (info->OrderInfo &&
                info->SessionID == m_uiSessionID) {
                if (0 != info->OrderInfo->ErrorCode) {
                    QString log_str = QObject::tr("����ʧ��,������:%1,ί�б��:%2").
                        arg(info->OrderInfo->ErrorCode).
                        arg(info->OrderInfo->OrderNo);
                    APP_LOG(applog::LOG_INFO) << log_str.toStdString();
                    emit signals_write_log(log_str);
                } else {
                    if (info->OrderInfo->OrderState!= TAPI_ORDER_STATE_FINISHED
                        && info->OrderInfo->OrderState != TAPI_ORDER_STATE_PARTFINISHED) {
                        APP_LOG(applog::LOG_INFO) << "�����ɹ���"
                            << "״̬:" << info->OrderInfo->OrderState << ","
                            << "ί�б��:" << info->OrderInfo->OrderNo;
                        QString log_str = QObject::tr("�����ɹ�,״̬:%1,ί�б��:%2").
                            arg(info->OrderInfo->OrderState).
                            arg(info->OrderInfo->OrderNo);
                        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
                        emit signals_write_log(log_str);
                    }
                }
                order_state_handle(info);
            }
        }
    }

    //�ɽ��ر�
    void TAP_CDECL Trade::OnRtnFill(const TapAPIFillInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
        emit signals_state_changed(TAPI_ORDER_STATE_FINISHED,
            info->MatchSide,
            QString::number(info->MatchPrice, 10, 2));

        QString log_str;
        if (info->MatchSide == TAPI_SIDE_SELL) {
            log_str = QObject::tr("��%1���׳ɹ�����ˮ��%2").
                arg(info->MatchPrice).
                arg(info->OrderNo);
        }
        else {
            log_str = QObject::tr("%1ƽ�ֳɹ�����ˮ��%2").
                arg(info->MatchPrice).
                arg(info->OrderNo);
        }
        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
        emit signals_write_log(log_str);
    }
    //ƽ�����ݱ仯����
    void TAP_CDECL Trade::OnRtnClose(const TapAPICloseInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspChangePassword(TAPIUINT32 sessionID, TAPIINT32 errorCode)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspSetReservedInfo(TAPIUINT32 sessionID, TAPIINT32 errorCode, const TAPISTR_50 info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspQryAccount(TAPIUINT32 sessionID, TAPIUINT32 errorCode, TAPIYNFLAG isLast, const TapAPIAccountInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspQryFund(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIFundData *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRtnFund(const TapAPIFundData *info)
    {
        //cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspQryExchange(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIExchangeInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspQryCommodity(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPICommodityInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspQryContract(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPITradeContractInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
        if (info->CommodityType == TAPI_COMMODITY_TYPE_FUTURES) {
            string key = string(info->CommodityNo) +
                info->ContractNo1;
            m_map_contract[key] = info->ExchangeNo;
        }
        //APP_LOG(applog::LOG_INFO) << info->ExchangeNo << " "
        //    << info->CommodityType << " "
        //    << info->CommodityNo << " "
        //    << info->ContractNo1 << " "
        //    << info->ContractNo2;
        if (isLast == APIYNFLAG_YES) {
            m_bContract = true;
            m_Event.SignalEvent();
        }
    }

    void TAP_CDECL Trade::OnRtnContract(const TapAPITradeContractInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspOrderAction(TAPIUINT32 sessionID, TAPIUINT32 errorCode, const TapAPIOrderActionRsp *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspQryOrder(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIOrderInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
        if (info == NULL) {
            m_border = true;
            m_Event.SignalEvent();

            return;
        }

        if (info->OrderState == TAPI_ORDER_STATE_QUEUED ||
            info->OrderState == TAPI_ORDER_STATE_PARTFINISHED) {
            m_map_order[info->OrderNo] = info->ServerFlag;
        }

        if (isLast == APIYNFLAG_YES) {
            if (m_map_order.size() != 0) {
                auto last = m_map_order.end();
                last--;
                for (auto it = m_map_order.begin(); it != last; it++) {
                    TapAPIOrderCancelReq CancelOrder;
                    memset(&CancelOrder, 0, sizeof(CancelOrder));
                    //CancelOrder.ServerFlag = server_type;
                    strcpy(CancelOrder.OrderNo, (it->first).c_str());

                    m_uiSessionID = 0;
                    int iErr = m_pAPI->CancelOrder(&m_uiSessionID, &CancelOrder);
                    if (TAPIERROR_SUCCEED != iErr) {
                        QString log_str = QObject::tr("%1%2").arg("����ʧ�ܣ�������:").
                            arg(iErr);
                        APP_LOG(applog::LOG_INFO) << log_str.toStdString();
                        emit signals_write_log(log_str);
                        return;
                    }
                }
                string key = "order_info/serverflag";
                stringstream stream;
                stream << last->second;
                common::set_config_value(key, stream.str());
                key = "order_info/orderno";
                common::set_config_value(key, string(last->first));
                emit signals_withdraw_order(last->first.c_str());
            }


            m_border = true;
            m_Event.SignalEvent();
        }
    }

    void TAP_CDECL Trade::OnRspQryOrderProcess(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIOrderInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspQryFill(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIFillInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRspQryPosition(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIPositionInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
        if (info == NULL) {
            m_bposition = true;
            m_Event.SignalEvent();

            return;
        }

        if (isLast != APIYNFLAG_YES) {
            TapAPINewOrder stNewOrder;
            strcpy(stNewOrder.AccountNo, info->AccountNo);
            strcpy(stNewOrder.ExchangeNo, info->ExchangeNo);
            stNewOrder.CommodityType = info->CommodityType;
            strcpy(stNewOrder.CommodityNo, info->CommodityNo);
            strcpy(stNewOrder.ContractNo, info->ContractNo);
            //stNewOrder.CallOrPutFlag = TAPI_CALLPUT_FLAG_CALL;
            stNewOrder.OrderType = TAPI_ORDER_TYPE_MARKET; //�м�/�ּ�
            stNewOrder.OrderSource = TAPI_ORDER_SOURCE_ESUNNY_API;
            stNewOrder.TimeInForce = TAPI_ORDER_TIMEINFORCE_GFD;
            stNewOrder.OrderSide = (info->MatchSide == TAPI_SIDE_BUY ? 
            TAPI_SIDE_SELL : TAPI_SIDE_BUY); //buy/sell
            stNewOrder.PositionEffect = TAPI_PositionEffect_COVER; //open/close
            stNewOrder.OrderPrice = DEFAULT_ORDER_PRICE;
            stNewOrder.OrderQty = DEFAULT_ORDER_QTY; //ί������ default 1

            m_uiSessionID = 0;
            int iErr = m_pAPI->InsertOrder(&m_uiSessionID, &stNewOrder);
            if (TAPIERROR_SUCCEED != iErr) {
                QString log_str = QObject::tr("%1%2").arg("��ƽʧ�ܣ�������:").
                    arg(iErr);
                APP_LOG(applog::LOG_INFO) << log_str.toStdString();
                emit signals_write_log(log_str);
                return;
            }
        } 
        else {
            emit signals_close_position(info->CommodityNo, info->ContractNo);

            m_bposition = true;
            m_Event.SignalEvent();
        }
    }

    void TAP_CDECL Trade::OnRtnPosition(const TapAPIPositionInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
        cout << "Ʒ��" << info->CommodityNo << ","
            << "��Լ" << info->ContractNo << ","
            << "�ֲ���" << info->PositionQty << ","
            << "�ֲּ�" << info->PositionPrice << endl;
    }

    void TAP_CDECL Trade::OnRspQryClose(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPICloseInfo *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }

    void TAP_CDECL Trade::OnRtnPositionProfit(const TapAPIPositionProfitNotice *info)
    {
        cout << __FUNCTION__ << " is called." << endl;
    }
    void TAP_CDECL Trade::OnRspQryCurrency(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPICurrencyInfo *info)
    {

    }

    void TAP_CDECL Trade::OnRspQryTradeMessage(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPITradeMessage *info)
    {
    }

    void TAP_CDECL Trade::OnRtnTradeMessage(const TapAPITradeMessage *info)
    {
    }

    void TAP_CDECL Trade::OnRspQryHisOrder(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIHisOrderQryRsp *info)
    {
    }

    void TAP_CDECL Trade::OnRspQryHisOrderProcess(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIHisOrderProcessQryRsp *info)
    {
    }

    void TAP_CDECL Trade::OnRspQryHisMatch(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIHisMatchQryRsp *info)
    {
    }

    void TAP_CDECL Trade::OnRspQryHisPosition(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIHisPositionQryRsp *info)
    {
    }

    void TAP_CDECL Trade::OnRspQryHisDelivery(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIHisDeliveryQryRsp *info)
    {
    }

    void TAP_CDECL Trade::OnRspQryAccountCashAdjust(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIAccountCashAdjustQryRsp *info)
    {
    }

    void TAP_CDECL Trade::OnRspQryBill(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIBillQryRsp *info)
    {
    }

    void TAP_CDECL Trade::OnExpriationDate(ITapTrade::TAPIDATE date, int days)
    {
    }

    void TAP_CDECL Trade::OnRspQryAccountFeeRent(ITapTrade::TAPIUINT32 sessionID, ITapTrade::TAPIINT32 errorCode, ITapTrade::TAPIYNFLAG isLast, const ITapTrade::TapAPIAccountFeeRentQryRsp *info)
    {
    }

    void TAP_CDECL Trade::OnRspQryAccountMarginRent(ITapTrade::TAPIUINT32 sessionID, ITapTrade::TAPIINT32 errorCode, ITapTrade::TAPIYNFLAG isLast, const ITapTrade::TapAPIAccountMarginRentQryRsp *info)
    {

    }
}