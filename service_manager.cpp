#include "service_manager.hpp"

namespace phosphor
{
namespace service
{

void Service::saveSetting(const char* settingName, bool value,
                          std::string& _protocol)
{
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
        settingsName, _protocol.c_str(), "org.freedesktop.DBus.Properties",
        "Set", settingsInterface, settingName, std::variant<bool>(value));
}

void Service::unmaskUnitFiles()
{
    for (auto& unitName : unitNames)
    {
        lg2::info("Unit {UNIT} will be unmasked...", "UNIT", unitName);
        auto _reload = unitName == unitNames.at(unitNames.size() - 1);
        systemBus->async_method_call(
            [this, _reload](boost::system::error_code ec) {
                if (ec)
                {
                    lg2::error("Failed to unmak service: {ERR}", "ERR",
                               ec.message());
                }
                else
                {
                    Base::masked(false);
                    if (_reload)
                        reload();
                }
            },
            systemdBusname, systemdPath, systemdInterface, "UnmaskUnitFiles",
            std::array<const char*, 1>{unitName.c_str()}, false);
    }
}

void Service::disableUnitFiles()
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
            systemdBusname, systemdPath, systemdInterface, "DisableUnitFiles",
            std::array<const char*, 1>{unitName.c_str()}, false);
    }
}

void Service::enableUnitFiles()
{
    for (auto& unitName : unitNames)
    {
        lg2::info("Unit {UNIT} will be enabled...", "UNIT", unitName);
        auto _reload = unitName == unitNames.at(unitNames.size() - 1);
        systemBus->async_method_call(
            [this, _reload](boost::system::error_code ec) {
                if (ec)
                {
                    lg2::error("Failed to enable unit: {ERR}", "ERR",
                               ec.message());
                }
                else
                {
                    Base::enabled(true);
                    if (_reload)
                        reload();
                }
            },
            systemdBusname, systemdPath, systemdInterface, "EnableUnitFiles",
            std::array<const char*, 1>{unitName.c_str()}, false, false);
    }
}

void Service::maskUnitFiles()
{
    for (auto& unitName : unitNames)
    {
        lg2::info("Unit {UNIT} will be masked...", "UNIT", unitName);
        auto _reload = unitName == unitNames.at(unitNames.size() - 1);
        systemBus->async_method_call(
            [this, _reload](boost::system::error_code ec) {
                if (ec)
                {
                    lg2::error("Failed to mask unit: {ERR}", "ERR",
                               ec.message());
                }
                else
                {
                    Base::masked(true);
                    if (_reload)
                        reload();
                }
            },
            systemdBusname, systemdPath, systemdInterface, "MaskUnitFiles",
            std::array<const char*, 1>{unitName.c_str()}, false, false);
    }
}

bool Service::enabled(bool value)
{
    if (Base::enabled() != value)
        saveSetting("Enabled", value, protocolPaths[protocol]);

    if (Base::enabled() == value)
        return value;

    if (Base::masked())
    {
        unmaskUnitFiles();
        saveSetting("Masked", false, protocolPaths[protocol]);
    }

    if (value)
        enableUnitFiles();
    else
        disableUnitFiles();

    return value;
}

bool Service::masked(bool value)
{
    if (Base::masked() != value)
        saveSetting("Masked", value, protocolPaths[protocol]);

    if (Base::masked() == value)
        return value;

    if (Base::enabled() && value)
    {
        disableUnitFiles();
        saveSetting("Enabled", false, protocolPaths[protocol]);
    }

    if (value)
        maskUnitFiles();
    else
        unmaskUnitFiles();

    return value;
}

bool Service::running(bool value)
{
    if (Base::masked())
    {
        lg2::info("Protocol {PROTOCOL} is masked and units can't be started...",
                  "PROTOCOL", protocol);
        return false;
    }

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
    std::vector<std::string> settingsPaths;

    auto method =
        systemBus->new_method_call(objectMapper, objectMapperPath,
                                   objectMapperInterface, "GetSubTreePaths");
    method.append(serviceManagerBasePath);
    // depth 0
    method.append(0);
    // array with size 1
    method.append(std::array<const char*, 1>{settingsInterface});

    try
    {
        auto reply = systemBus->call(method);
        reply.read(settingsPaths);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to get protocols from phosphor-settingsd");
        return _services;
    }

    if (settingsPaths.size() == 0)
    {
        lg2::error("Failed to get protocols from phosphor-settingsd");
        return _services;
    }

    for (auto& path : settingsPaths)
    {
        auto protocol = path.substr(path.rfind("/") + 1);
        protocols.push_back(protocol);
        protocolPaths[protocol] = path;
    }

    for (auto& protocol : protocols)
    {
        try
        {
            auto units = getPropertySync<Units>(systemBus, settingsName,
                                                protocolPaths[protocol].c_str(),
                                                settingsInterface, "Units");
            auto enabled = getPropertySync<bool>(
                systemBus, settingsName, protocolPaths[protocol].c_str(),
                settingsInterface, "Enabled");

            auto masked = getPropertySync<bool>(systemBus, settingsName,
                                                protocolPaths[protocol].c_str(),
                                                settingsInterface, "Masked");

            _services.emplace_back(units, enabled, masked, protocol);
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
        if (serv.isMasked())
        {
            serv.masked(true);
            continue;
        }

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
