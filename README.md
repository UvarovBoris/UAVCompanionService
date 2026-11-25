1) Gazebo 
install 
https://gazebosim.org/docs/latest/install/

run
gz sim -v4 -r iris_runway.sdf

Enable camera (every time after relaunch gazebo)

gz topic -t "/world/iris_runway/model/iris_with_gimbal/model/gimbal/link/pitch_link/sensor/camera/image/enable_streaming" -m gz.msgs.Boolean -p "data: true"

Gazebo stream video on default port 5600, CameraStream.cpp take video from port 5600, add telemetry and stream updated video in port 5601. Setup QGroundControll to take video not from default 5600, but from 5601.

update camera fps in gazebo to 30
vim ~/install_dir/ardupilot_gazebo/models/gimbal_small_3d/model.sdf

2) Ardupilot SITL 
install
https://ardupilot.org/dev/docs/setting-up-sitl-on-linux.html

setup ardupilot + gazebo
https://ardupilot.org/dev/docs/sitl-with-gazebo.html

run
sim_vehicle.py -v ArduCopter -f gazebo-iris --model JSON

update camera fps in 

3) QGroundControll
install
https://docs.qgroundcontrol.com/master/en/qgc-user-guide/getting_started/download_and_install.html
