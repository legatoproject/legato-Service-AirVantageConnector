<?xml version="1.0" encoding="UTF-8"?>
<app:application xmlns:app="http://www.sierrawireless.com/airvantage/application/1.0" type="" name="" revision="">
    <capabilities>
        <communication>
            <protocol comm-id="IMEI" type="LWM2M">
                <parameter name="dtls" value="psk"/>
                <parameter name="version" value="core=1_0-20151214,sw=1_0-20151201"/>
            </protocol>
        </communication>
        <dm>
            <action impl="LWM2M_SYNCHRONIZE"/>
            <action impl="LWM2M_REBOOT"/>
            <action impl="LWM2M_SW_UPDATE"/>
            <action impl="LWM2M_SW_UNINSTALL"/>
            <action impl="LWM2M_SW_START"/>
            <action impl="LWM2M_SW_STOP"/>
            <action impl="LWM2M_OBSERVE"/>
            <action impl="LWM2M_CONFIGURE_HEARTBEAT"/>
            <action impl="LWM2M_AIRPRIME_BUNDLE_INSTALL"/>
            <action impl="LWM2M_LEGATO_SYSTEM_UPDATE"/>
        </dm>
        <include>
            <file name="oma.cp"/>
            <file name="swir.cp"/>
        </include>
    </capabilities>
    <application-manager use="LWM2M_AIRPRIME_FW"/>
</app:application>