#pragma once

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Control/Service/Attributes/server.hpp>

#include <chrono>
#include <ctime>
#include <list>
#include <string>
#include <vector>

namespace phosphor
{
namespace service
{

/*
Service xyz.openbmc_project.Control.Service.Manager:
`- /xyz
  `- /xyz/openbmc_project
    `- /xyz/openbmc_project/control
      `- /xyz/openbmc_project/control/service
      `- /xyz/openbmc_project/control/service/IPMINET
      `- /xyz/openbmc_project/control/service/SSH
*/

static constexpr auto serviceManagerName =
    "xyz.openbmc_project.Control.Service.Manager";
static constexpr auto serviceAttributeName =
    "xyz.openbmc_project.Control.Service.Attributes";
static constexpr auto serviceManagerBasePath =
    "/xyz/openbmc_project/control/service";

static constexpr auto ipmiNetProtocolPath =
    "/xyz/openbmc_project/control/service/IPMINET";
static constexpr auto sshProtocolPath =
    "/xyz/openbmc_project/control/service/SSH";

static constexpr auto settingsName = "xyz.openbmc_project.Settings";
static constexpr auto settingsInterface =
    "xyz.openbmc_project.Control.Service.Protocol";

static constexpr auto systemdBusname = "org.freedesktop.systemd1";
static constexpr auto systemdPath = "/org/freedesktop/systemd1";
static constexpr auto systemdInterface = "org.freedesktop.systemd1.Manager";

// Need to make getting protocols from dbus instead of hard coded values.
// At bmcweb side it should populated too, both sides should be connected about
// getting right groups/protocols from one place.
static std::map<std::string, std::string> protocolPaths{
    {"IPMINET", ipmiNetProtocolPath}, {"SSH", sshProtocolPath}};
static std::vector<std::string> protocols{"IPMINET", "SSH"};

using Base =
    sdbusplus::xyz::openbmc_project::Control::Service::server::Attributes;
static boost::asio::io_context io;
static sdbusplus::asio::object_server* objectServer;
std::shared_ptr<sdbusplus::asio::connection> systemBus;

using Units = std::vector<std::string>;

template <typename T>
T getPropertySync(std::shared_ptr<sdbusplus::asio::connection> bus,
              const char* objectName, const char* objectPath,
              const char* objectIntf, const char* propertyName)
{
    auto method = bus->new_method_call(
        objectName, objectPath, "org.freedesktop.DBus.Properties", "Get");

    method.append(objectIntf, propertyName);
    auto reply = systemBus->call(method);
    std::variant<T> property;
    reply.read(property);
    return std::get<T>(property);
}

class Service : public Base
{
  public:
    Service(std::vector<std::string> _unitNames, bool _sbEnabled,
            std::string _protocol) :

        Base(static_cast<sdbusplus::bus_t&>(*systemBus),
             (std::string(serviceManagerBasePath) + "/" + _protocol).c_str()),
        unitNames(std::move(_unitNames)), sbEnabled(_sbEnabled),
        protocol(std::move(_protocol))
    {}

    Service() = delete;
    ~Service() = default;
    Service(const Service&) = default;

    Service& operator=(const Service&) = delete;
    Service(Service&&) = delete;
    Service& operator=(Service&&) = default;

    // TODO: add masked
    bool enabled(bool value) override;
    bool running(bool value) override;

    bool isEnabled()
    {
        return sbEnabled;
    }

    bool isRunning();

  private:
    std::vector<std::string> unitNames;
    bool sbEnabled;
    std::string protocol;
    std::shared_ptr<sdbusplus::asio::dbus_interface> attributesIface;

    void reload();
};

class ServiceManager
{
  public:
    ~ServiceManager() = default;
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;
    ServiceManager(ServiceManager&&) = delete;
    ServiceManager& operator=(ServiceManager&&) = delete;

    ServiceManager();

    std::list<Service> getServices();

  private:
    std::list<Service> services;
};

} // namespace service
} // namespace phosphor
