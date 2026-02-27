#include "udp/udp_config.h"
#include <string.h>

static UdpLogConfig g_cfg = {
    "", "", "0.0.0.0", 10514
};

bool UdpLogConfig::isValid() const
{
    if (targetPort == 0) return false;
    if (targetIp[0] == 0) return false;
    if (ssid[0] == 0) return false;
    if (password[0] == 0) return false;
    return true;
}

void udp_config_apply(const UdpLogConfig& cfg)
{
    g_cfg = cfg;
}

const UdpLogConfig& udp_config_current()
{
    return g_cfg;
}

void udp_config_reset()
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    strcpy(g_cfg.targetIp, "0.0.0.0");
    g_cfg.targetPort = 10514;
}
