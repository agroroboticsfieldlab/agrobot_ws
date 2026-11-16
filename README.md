# agrobot_ws

ROS 2 (Jazzy)-workspace voor de agrobots van de Agrorobotics Fieldlab. De workspace draait op een Raspberry Pi die via CAN-bus de ODrive-motorcontrollers van de wielen aanstuurt. Er zijn twee robots:

| Robot | Aandrijving |
|---|---|
| **Curio** | differential drive: 2 aangedreven wielen voor, 2 zwenkwielen achter |
| **ATD** | 4 aangedreven wielen, skid steering |

Elk aangedreven wiel heeft een eigen ODrive Pro (CAN-node 1 t/m 4), met een 5:1 planetaire overbrenging. De robot wordt bestuurd met een Logitech F710-gamepad of via snelheidscommando's op `/agrobot/cmd_vel`.

## Packages

| Package | Inhoud |
|---|---|
| [agrobot_bringup](src/agrobot_bringup) | Launch-bestanden, controllerconfig (diff_drive_controller), gamepad-teleop en de systemd-service voor automatisch starten. |
| [agrobot_description](src/agrobot_description) | URDF/xacro van Curio en ATD, inclusief de ros2_control-config (welke ODrive bij welk wiel). |
| [agrobot_odrive](src/agrobot_odrive) | ros2_control-plugin `GearedODriveHardwareInterface` (ODrive-plugin + overbrenging), back-ups van de ODrive-configs en documentatie over de ODrive-instellingen. |
| [ros_odrive](src/ros_odrive) | Git-submodule: de officiële ROS 2-packages van [ODrive Robotics](https://github.com/odriverobotics/ros_odrive). Ongewijzigd; agrobot-specifieke aanpassingen staan in `agrobot_odrive`. |

Elk package heeft een eigen README met de details.

```
agrobot_bringup ──> agrobot_description ──> agrobot_odrive ──> ros_odrive (odrive_ros2_control)
   launch, config       URDF + ros2_control     overbrenging        CAN-communicatie
```

## Installatie

```bash
git clone --recurse-submodules git@github.com:agroroboticsfieldlab/agrobot_ws.git ~/agrobot_ws
cd ~/agrobot_ws
source /opt/ros/jazzy/setup.bash
rosdep install --from-paths src --ignore-src -y
colcon build
```

Al gekloond zonder `--recurse-submodules`? Haal ros_odrive dan alsnog op met `git submodule update --init`.

### CAN-bus

De ODrives zitten op `can0` met 250 kbit/s. Op de Pi brengt systemd-networkd de interface bij het opstarten omhoog via `/etc/systemd/network/80-can.network`:

```ini
[Match]
Name=can*

[CAN]
BitRate=250000
```

Controleren: `ip link show can0` moet `state UP` tonen. Meer over de ODrive-configuratie staat in [ODRIVE.md](src/agrobot_odrive/docs/ODRIVE.md).

## Gebruik

```bash
source ~/agrobot_ws/install/setup.bash
ros2 launch agrobot_bringup bringup.launch.py               # Curio
ros2 launch agrobot_bringup bringup.launch.py robot:=atd    # ATD
ros2 launch agrobot_bringup bringup.launch.py use_mock_hardware:=true   # zonder robot/CAN
```

- **Gamepad (F710, schakelaar op X):** LB vasthouden = rijden, LB + RB = turbo. Linker stick vooruit/achteruit, rechter stick draaien.
- **Commando's:** `geometry_msgs/msg/TwistStamped` op `/agrobot/cmd_vel`.
- **Odometrie:** `/agrobot/odom` en de tf `odom` → `base_footprint`.

Op de Curio start de bringup automatisch bij het opstarten via de systemd-user-service `agrobot-bringup`. Stop die voordat je handmatig start:

```bash
systemctl --user stop agrobot-bringup
journalctl --user -u agrobot-bringup -f    # logs bekijken
```

Zie [agrobot_bringup](src/agrobot_bringup/README.md) voor alle launch-argumenten, topics, snelheidslimieten en de service.

## ros_odrive bijwerken

`src/ros_odrive` staat vast op een bepaalde upstream-commit. Bijwerken naar de nieuwste versie:

```bash
cd src/ros_odrive && git pull origin main && cd ../..
colcon build
git add src/ros_odrive && git commit -m "Update ros_odrive"
```

Wijzig niets in `src/ros_odrive` zelf. Aanpassingen horen in `agrobot_odrive`.
