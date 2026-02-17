#include "udp_config_store.h"
#include <Preferences.h>
#include <string.h>

static const char* NS = "udp";

static const char* KEY_SSID = "ssid";
static const char* KEY_PW   = "pw";
static const char* KEY_IP   = "ip";
static const char* KEY_PORT = "port";

bool udp_cfg_load(UdpLogConfig& out)
{
    Preferences prefs;
    if (!prefs.begin(NS, true))
        return false;

    bool ok = false;

    if (prefs.isKey(KEY_IP) && prefs.isKey(KEY_PORT))
    {
        strncpy(out.ssid, prefs.getString(KEY_SSID, "").c_str(), sizeof(out.ssid));
        strncpy(out.password, prefs.getString(KEY_PW, "").c_str(), sizeof(out.password));
        strncpy(out.targetIp, prefs.getString(KEY_IP, "").c_str(), sizeof(out.targetIp));
        out.targetPort = prefs.getUShort(KEY_PORT, 10514);

        ok = out.isValid();
    }

    prefs.end();
    return ok;
}

bool udp_cfg_save(const UdpLogConfig& cfg)
{
    Preferences prefs;
    if (!prefs.begin(NS, false))
        return false;

    prefs.putString(KEY_SSID, cfg.ssid);
    prefs.putString(KEY_PW, cfg.password);
    prefs.putString(KEY_IP, cfg.targetIp);
    prefs.putUShort(KEY_PORT, cfg.targetPort);

    prefs.end();
    return true;
}

bool udp_cfg_clear()
{
    Preferences prefs;
    if (!prefs.begin(NS, false))
        return false;

    prefs.clear();
    prefs.end();
    return true;
}
