#!/usr/bin/env bash
set -e

# 1. Запуск Gazebo в фоне
gz sim -v4 -r iris_runway.sdf &
GZ_PID=$!
# Ждём инициализации мира и топиков камеры
sleep 10

gz topic -t "/world/iris_runway/model/iris_with_gimbal/model/gimbal/link/pitch_link/sensor/camera/image/enable_streaming" -m gz.msgs.Boolean -p "data: true"

# 2. Запуск ArduPilot SITL (без MAVProxy) в фоне
sim_vehicle.py -v ArduCopter -f gazebo-iris --model JSON --no-mavproxy &
SITL_PID=$!
sleep 10

# 3. Запуск mavlink-routerd
mavlink-routerd &
ROUTER_PID=$!

echo "Стенд запущен. Нажмите Ctrl+C для остановки всех процессов."
trap "kill -9 $GZ_PID $SITL_PID $ROUTER_PID 2>/dev/null" EXIT
wait
EOF
