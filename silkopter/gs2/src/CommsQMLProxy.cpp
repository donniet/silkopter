#include "CommsQMLProxy.h"
#include "utils/RCP_RFMON_Socket.h"

CommsQMLProxy::CommsQMLProxy(QObject *parent)
    : QObject(parent)
{

}

void CommsQMLProxy::init(silk::Comms& comms)
{
    m_comms = &comms;
}

bool CommsQMLProxy::isConnected() const
{
    return m_comms->is_connected();
}

CommsQMLProxy::ConnectionStatus CommsQMLProxy::getConnectionStatus() const
{
    return isConnected() ? ConnectionStatus::CONNECTED : ConnectionStatus::NOT_CONNECTED;
}

CommsQMLProxy::ConnectionType CommsQMLProxy::getConnectionType() const
{
    return isConnected() ? ConnectionType::RF_MON : ConnectionType::NONE;
}

QList<QString> CommsQMLProxy::getRFMONInterfaces() const
{
    return m_rfmonInterfaces;
}

uint8_t CommsQMLProxy::getRFMONEndpointId() const
{
    return m_rfmonEndpointId;
}

QList<QString> CommsQMLProxy::enumerateRFMONInterfaces() const
{
    if (m_enumeratedRFMONInterfaces.empty())
    {
        std::vector<std::string> interfaces = util::RCP_RFMON_Socket::enumerate_interfaces();
        for (std::string const& i: interfaces)
        {
            m_enumeratedRFMONInterfaces.append(QString(i.c_str()));
        }
    }
    return m_enumeratedRFMONInterfaces;
}
