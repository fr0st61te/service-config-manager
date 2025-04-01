# service-manager

This is simplified version of https://github.com/openbmc/service-config-manager which works with only Units, using [attributes interface](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/Control/Service/Attributes.interface.yaml) and Protocol interface at dbus-interface directory. The configuration settings are intended to persist across BMC reboots with phosphor-settings.

The service config manager generally makes configuration changes to systemd units via D-Bus interface.
The design pattern to add new services or controls is:
>    Control protocol types defined in groups of .socket/.service files in phosphor-settings.
>    Control start/stop/enable/disable actions via dbus property change.

Using groups for protocol types like for SSH, IPMI NET instead of acting with direct .socket @.service files.
