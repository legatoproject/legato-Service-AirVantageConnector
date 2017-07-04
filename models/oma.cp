<?xml version="1.0" encoding="UTF-8"?>
<app:capabilities xmlns:app="http://www.sierrawireless.com/airvantage/application/1.0">
  <data>
    <encoding type="LWM2M">
      <asset id="lwm2m">
        <node default-label="LWM2M Server" path="1">
          <variable default-label="Short server ID" path="0" type="int"></variable>
          <setting default-label="Lifetime" path="1" type="int"></setting>
          <setting default-label="Default minimum period" path="2" type="int"></setting>
          <setting default-label="Default maximum period" path="3" type="int"></setting>
          <setting default-label="Notification storing when disabled or offline" path="6" type="boolean"></setting>
          <setting default-label="Binding" path="7" type="string"></setting>
          <command default-label="Registration update trigger" path="8"></command>
        </node>
        <node default-label="Device" path="3">
          <variable default-label="Manufacturer" path="0" type="string"></variable>
          <variable default-label="Model number" path="1" type="string"></variable>
          <variable default-label="Serial number" path="2" type="string"></variable>
          <variable default-label="Firmware version" path="3" type="string"></variable>
          <!--Reboot device is supported by DM command: <action impl="LWM2M_REBOOT"/>
          <command default-label="Reboot device" path="4"></command>-->
          <variable default-label="Available power sources" path="6" type="int"></variable>
          <variable default-label="Power source voltage" path="7" type="int"></variable>
          <variable default-label="Battery level" path="9" type="int"></variable>
          <variable default-label="Memory free" path="10" type="int"></variable>
          <variable default-label="Error code" path="11" type="int"></variable>
          <command default-label="Reset error code log" path="12"></command>
          <setting default-label="Current time" path="13" type="date"></setting>
          <setting default-label="UTC offset" path="14" type="string"></setting>
          <variable default-label="Supported binding and modes" path="16" type="string"></variable>
        </node>
        <node default-label="Connectivity Monitoring" path="4">
          <variable default-label="Network bearer" path="0" type="int"></variable>
          <variable default-label="Available network bearer" path="1" type="int"></variable>
          <variable default-label="Radio Signal Strength" path="2" type="int"></variable>
          <variable default-label="Link Quality" path="3" type="string"></variable>
          <variable default-label="IP addresses" path="4" type="string"></variable>
          <variable default-label="APN" path="7" type="string"></variable>
          <variable default-label="Cell ID" path="8" type="int"></variable>
          <variable default-label="SMNC" path="9" type="int"></variable>
          <variable default-label="SMCC" path="10" type="int"></variable>
        </node>
        <node default-label="Firmware Update" path="5">
          <setting default-label="Firmware package URI" path="1" type="string"></setting>
          <!--Update FW is supported by DM command: <action impl="LWM2M_FW_UPDATE"/>
          <command default-label="Update firmware" path="2"></command>-->
          <variable default-label="Firmware update state" path="3" type="int"></variable>
          <setting default-label="Firmware update supported objects" path="4" type="boolean"></setting>
          <variable default-label="Firmware update result" path="5" type="int"></variable>
        </node>
        <node default-label="Location" path="6">
          <variable default-label="Latitude" path="0" type="string"></variable>
          <variable default-label="Longitude" path="1" type="string"></variable>
          <variable default-label="Altitude" path="2" type="string"></variable>
          <variable default-label="Uncertainty" path="3" type="string"></variable>
          <variable default-label="Velocity" path="4" type="binary"></variable>
          <variable default-label="Timestamp" path="5" type="date"></variable>
        </node>
        <node default-label="Connectivity Statistics" path="7">
          <variable default-label="SMS Tx counter" path="0" type="int"></variable>
          <variable default-label="SMS Rx counter" path="1" type="int"></variable>
          <variable default-label="Tx data" path="2" type="int"></variable>
          <variable default-label="Rx data" path="3" type="int"></variable>
          <command default-label="Reset SMS and data counters" path="6"></command>
        </node>
        <node default-label="Software Management" path="9">
          <variable default-label="Application package name" path="0" type="string"></variable>
          <variable default-label="Application package version" path="1" type="string"></variable>
          <setting default-label="Application package URI" path="3" type="string"></setting>
          <!--Install application is supported by DM command: <action impl="LWM2M_SW_UPDATE"/>
          <command default-label="Install application" path="4"></command>-->
          <!--Uninstall application is supported by DM command: <action impl="LWM2M_SW_UNINSTALL"/>
          <command default-label="Uninstall application" path="6"></command>-->
          <variable default-label="Application update state" path="7" type="int"></variable>
          <setting default-label="Application update supported objects" path="8" type="boolean"></setting>
          <variable default-label="Application update result" path="9" type="int"></variable>
          <!--Start application is supported by DM command: <action impl="LWM2M_SW_START"/>
          <command default-label="Activate application" path="10"></command>-->
          <!--Stop application is supported by DM command: <action impl="LWM2M_SW_STOP"/>
          <command default-label="Deactivate application" path="11"></command>-->
          <variable default-label="Application activation result" path="12" type="boolean"></variable>
        </node>
      </asset>
    </encoding>
  </data>
</app:capabilities>