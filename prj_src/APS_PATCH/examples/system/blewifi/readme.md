# Function
1. This example shows the procedure of OPL1000 using BLE to configure WIFI AP and then connect to specified AP
2. This example will post the status to Coolkit cloud server using http protocol when user do the operation for sensor device or sensor device have some status change
3. This example will do keep alive operation with Coolkit cloud server. Server can monitor whether device is always connected to Internet

# Work Flow
1. OPL1000 broadcast advertising message sostenuto 
2. open "OPL1000 netstrip" APP on mobile , it will scan OPL1000 device automatically. 
3. Once OPL1000 device (MAC address shall be default value 11:22:33:44:55:66) is found, app will ask OPL1000 to do scan operation.
4. OPL1000 send scan result to APP program and display them on mobile. 
5. User choose one AP and enter password on mobile. 
6. OPL1000 will connect to specified AP accordingly.     
7. If device is connected to Internet, device can do http post to cloud if have the data need to post or keep alive operation.

#The location for place this folder.
$OPULINK_A2_SDK_PATH/APS_PATCH/examples/system/

#Important files in this folder 
1.	opl1000_app_m3 .uvprojx  => project file, can build this example binary file
2.	main_patch.c => main function of the example
3.	blewifi_configuration.h => the configuration for BLE/WIFI setting.
4.	blewifi_data.c /.h => Handle the command from App on mobile device via BLE connection
5.	blewifi_ctrl.c /.h => do the operations of the command which is received in blewifi_data for connecting to WIFI internet
6.	sensor/sensor_https.c /.h => connect and do the operation for posting data to Coolkit cloud.

# Notes
1. Refer Demo\BLE_Config_AP\OPL1000-Demo-BLE-setup-network-guide.pdf to know detailed processing flow.
2. Refer Doc\OPL1000-BLEWIFI-Application-Dev-Guide.pdf  to know BLEWIFI example working principle.


