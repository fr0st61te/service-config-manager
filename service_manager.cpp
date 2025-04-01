#include "service_manager.hpp"

namespace phosphor
{
namespace service
{

bool Service::enabled(bool value)
{
    if (Base::enabled() != value)
        systemBus->async_method_call(
            [this](boost::system::error_code ec) {
                if (ec)
                {
                    lg2::error(
                        "Failed to save configuration enabled={ENABLED} for service={SERVICE}: {ERR}",
                        "ENABLED", Base::enabled(), "SERVICE", protocol, "ERR",
                        ec.message());
                }
            },
            settingsName, protocolPaths[protocol].c_str(),
            "org.freedesktop.DBus.Properties", "Set", settingsInterface,
            "Enabled", std::variant<bool>(value));

    if (Base::enabled() == value)
        return value;

    if (value)
    {
        for (auto& unitName : unitNames)
        {
            lg2::info("Unit {UNIT} will be enabled...", "UNIT", unitName);
            systemBus->async_method_call(
                [this, unitName](boost::system::error_code ec) {
                    if (ec)
                    {
                        lg2::error("Failed to enable unit: {ERR}", "ERR",
                                   ec.message());
                    }
                    else
                    {
                        Base::enabled(true);
                        reload();
                    }
                },
                systemdBusname, systemdPath, systemdInterface,
                "EnableUnitFiles", std::array<const char*, 1>{unitName.c_str()},
                false, false);
        }
    }
    else
    {
        for (auto& unitName : unitNames)
        {
            lg2::info("Unit {UNIT} will be disabled...", "UNIT", unitName);
            systemBus->async_method_call(
                [this](boost::system::error_code ec) {
                    if (ec)
                    {
                        lg2::error("Failed to disable service: {ERR}", "ERR",
                                   ec.message());
                    }
                    else
                    {
                        Base::enabled(false);
                        reload();
                    }
                },
                systemdBusname, systemdPath, systemdInterface,
                "DisableUnitFiles",
                std::array<const char*, 1>{unitName.c_str()}, false);
        }
    }
    return value;
}

bool Service::running(bool value)
{
    std::string action;
    for (auto& unitName : unitNames)
    {
        if (value)
        {
            action = "StartUnit";
            lg2::info("Unit {UNIT} will be started...", "UNIT", unitName);
            Base::running(true);
        }
        else
        {
            action = "StopUnit";
            lg2::info("Unit {UNIT} will be stopped...", "UNIT", unitName);
            Base::running(false);
        }
        systemBus->async_method_call(
            [unitName](boost::system::error_code ec) {
                if (ec)
                {
                    lg2::error("Failed to start unit: {ERR}", "ERR",
                               ec.message());
                }
            },
            systemdBusname, systemdPath, systemdInterface, action,
            unitName.c_str(), "replace");
    }

    return value;
}

void Service::reload()
{
    systemBus->async_method_call(
        [](boost::system::error_code ec) {
            if (ec)
            {
                lg2::error("Failed to systemd reload: {ERR}", "ERR",
                           ec.message());
            }
        },
        systemdBusname, systemdPath, systemdInterface, "Reload");
}

bool Service::isRunning()
{
    for (auto& unitName : unitNames)
    {
        sdbusplus::message::object_path unitNamePathObj;
        auto method = systemBus->new_method_call(systemdBusname, systemdPath,
                                                 systemdInterface, "GetUnit");
        method.append(unitName.c_str());
        sdbusplus::message_t reply;

        try
        {
            reply = systemBus->call(method);
            reply.read(unitNamePathObj);
        }
        catch (const std::exception& e)
        {
            // Unit is not running and not enabled
            return false;
        }

        auto unitNamePath = static_cast<std::string>(unitNamePathObj);

        if (unitNamePath.size() > 0)
        {
            auto activeState = getPropertySync<std::string>(
                systemBus, systemdBusname, unitNamePath.c_str(),
                "org.freedesktop.systemd1.Unit", "ActiveState");
            if (activeState != "active")
                return false;
        }
    }

    return true;
}

std::list<Service> ServiceManager::getServices()
{
    std::list<Service> _services;
    for (auto& protocol : protocols)
    {
        try
        {
            auto units =
                getPropertySync<Units>(systemBus, settingsName,
                                   protocolPaths[protocol].c_str(),
                                   settingsInterface, "Units");
            auto enabled =
                getPropertySync<bool>(systemBus, settingsName,
                                  protocolPaths[protocol].c_str(),
                                  settingsInterface, "Enabled");
            _services.emplace_back(units, enabled, protocol);
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed to parse system protocol: {ERR}", "ERR", e);
        }
    }

    return _services;
}

ServiceManager::ServiceManager()
{
    services = getServices();
    for (auto& serv : services)
    {
        if (serv.isEnabled())
        {
            serv.enabled(true);
            if (!serv.isRunning())
                serv.running(true);
        }
        else
        {
            serv.enabled(false);
            if (serv.isRunning())
                serv.running(false);
        }
    }
}

} // namespace service
} // namespace phosphor

int main()
{
    phosphor::service::systemBus =
        std::make_shared<sdbusplus::asio::connection>(phosphor::service::io);
    phosphor::service::systemBus->request_name(
        phosphor::service::serviceManagerName);

    sdbusplus::asio::object_server objserv(phosphor::service::systemBus);
    objserv.add_manager(phosphor::service::serviceManagerBasePath);

    phosphor::service::objectServer = &objserv;
    phosphor::service::objectServer->add_manager(
        phosphor::service::serviceManagerBasePath);
    phosphor::service::ServiceManager sm;

    phosphor::service::io.run();
    return 0;
}
