<?xml version="1.0" encoding="UTF-8"?>
<app:capabilities xmlns:app="http://www.sierrawireless.com/airvantage/application/1.0">
  <data>
    <encoding type="LWM2M">
      <asset id="lwm2m">
        <node default-label="Extended Connectivity Statistics" path="10242">
          <variable default-label="Signal bars" path="0" type="int"></variable>
          <variable default-label="Cellular technology" path="1" type="string"></variable>
          <variable default-label="Roaming indicator" path="2" type="boolean"></variable>
          <variable default-label="EcIo" path="3" type="int"></variable>
          <variable default-label="RSRP" path="4" type="int"></variable>
          <variable default-label="RSRQ" path="5" type="int"></variable>
          <variable default-label="RSCP" path="6" type="int"></variable>
          <variable default-label="Device temperature" path="7" type="int"></variable>
          <variable default-label="Unexpected Reset Counter" path="8" type="int"></variable>
          <variable default-label="Total Reset Counter" path="9" type="int"></variable>
          <variable default-label="LAC" path="10" type="int"></variable>
          <variable default-label="TAC" path="11" type="int"></variable>
        </node>
        <node default-label="Subscription" path="10241">
          <variable default-label="Module identity (IMEI)" path="0" type="string"></variable>
          <variable default-label="SIM card identifier (ICCID)" path="1" type="string"></variable>
          <variable default-label="Subscription identity (IMSI/ESN/MEID)" path="2" type="string"></variable>
          <variable default-label="Subscription phone number (MSISDN)" path="3" type="string"></variable>
          <command default-label="Change SIM mode" path="4"><parameter id="1" default-value="0" type="int"></parameter></command>
          <variable default-label="Current SIM card" path="5" type="int"></variable>
          <variable default-label="Current SIM mode" path="6" type="int"></variable>
          <variable default-label="Last SIM switch status" path="7" type="int"></variable>
        </node>

        <node default-label="SSL certificates" path="10243">
          <setting default-label="Certificate" path="0" type="binary"></setting>
        </node>

        <node default-label="Clock Time Configuration" path="33405">
          <setting default-label="Time server source" path="0" type="int"></setting>
          <setting default-label="Time server addresses" path="1" type="string"></setting>
          <command default-label="Time update operation" path="2"></command>
          <variable default-label="Time update status" path="3" type="int"></variable>
        </node>

        <node default-label="File Transfer Object" path="33406">
          <setting default-label="File Name" path="0" type="string"></setting>
          <setting default-label="File Class" path="1" type="string"></setting>
          <setting default-label="File URI" path="2" type="string"></setting>
          <setting default-label="Hash" path="3" type="string"></setting>
          <setting default-label="Direction" path="4" type="int"></setting>
          <variable default-label="State" path="5" type="int"></variable>
          <variable default-label="Result" path="6" type="int"></variable>
          <variable default-label="Percentage of completion" path="7" type="int"></variable>
          <variable default-label="Failure reason" path="8" type="string"></variable>
        </node>

        <node default-label="File Sync Object" path="33407">
          <variable default-label="File Name" path="0" type="string"></variable>
          <variable default-label="File Class" path="1" type="string"></variable>
          <variable default-label="Hash" path="2" type="string"></variable>
          <variable default-label="Origin" path="3" type="int"></variable>
        </node>

        <node default-label="SIM APDU Configuration" path="33408">
          <setting default-label="SIM APDU Config" path="0" type="binary"></setting>
          <command default-label="SIM APDU operation" path="1"></command>
          <variable default-label="SIM APDU response" path="2" type="binary"></variable>
        </node>

      </asset>
    </encoding>
  </data>
</app:capabilities>
