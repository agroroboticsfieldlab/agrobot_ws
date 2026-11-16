# agrobot_bringup

Launch-bestanden, controllerconfig en de systemd-service om een agrobot op te starten. De robotbeschrijving staat in [agrobot_description](../agrobot_description), de ODrive-plugin en ODrive-documentatie in [agrobot_odrive](../agrobot_odrive).

Dit package bevat:

- [launch/bringup.launch.py](launch/bringup.launch.py): start de hele robot (ros2_control, robot_state_publisher, controllers en gamepad-teleop).
- [launch/teleop_f710.launch.py](launch/teleop_f710.launch.py): besturing met een Logitech F710-gamepad.
- [config/](config/): controllerconfig per robot en de gamepadconfig.
- [systemd/agrobot-bringup.service](systemd/agrobot-bringup.service): start de bringup automatisch bij het opstarten van de Raspberry Pi.

## Robots

| `robot:=` | Aandrijving | Wielen (joints) | Controllerconfig |
|---|---|---|---|
| `curio` (standaard) | differential drive | `left_wheel_joint`, `right_wheel_joint` + 2 zwenkwielen | [agrobot_curio_controllers.yaml](config/agrobot_curio_controllers.yaml) |
| `atd` | 4 wielen, skid steering | `{front,rear}_{left,right}_wheel_joint` | [agrobot_atd_controllers.yaml](config/agrobot_atd_controllers.yaml) |

## Starten

```bash
source /opt/ros/jazzy/setup.bash
source ~/agrobot_ws/install/setup.bash
ros2 launch agrobot_bringup bringup.launch.py               # Curio
ros2 launch agrobot_bringup bringup.launch.py robot:=atd    # ATD
```

Launch-argumenten:

| Argument | Standaard | Betekenis |
|---|---|---|
| `robot` | `curio` | `curio` of `atd`. Kiest `agrobot_<robot>.urdf.xacro` en `agrobot_<robot>_controllers.yaml`. |
| `use_mock_hardware` | `false` | `true`: `mock_components/GenericSystem` in plaats van de ODrives. Er beweegt niets; handig om zonder robot of CAN-bus te testen. |
| `use_teleop` | `true` | Start `joy_node` en `teleop_twist_joy` voor de F710. |

Zonder mock-hardware moet `can0` up zijn (zie [ODRIVE.md](../agrobot_odrive/docs/ODRIVE.md)).

### Wat er gestart wordt

1. `ros2_control_node` (controller_manager) met de URDF en de controllerconfig. De hardware-interface is `agrobot_odrive/GearedODriveHardwareInterface` op `can0`.
2. `robot_state_publisher`.
3. Spawner voor `joint_state_broadcaster`.
4. Pas als die klaar is: spawner voor de diff-drive-controller `agrobot` (`diff_drive_controller/DiffDriveController`).
5. Optioneel [teleop_f710.launch.py](launch/teleop_f710.launch.py).

### Topics

| Topic | Type | Richting |
|---|---|---|
| `/agrobot/cmd_vel` | `geometry_msgs/msg/TwistStamped` | in: snelheidscommando's (lineair x, hoek z) |
| `/agrobot/odom` | `nav_msgs/msg/Odometry` | uit: odometrie uit de wielencoders |
| `/tf` | | uit: `odom` → `base_footprint` (door de controller) en de robotframes (door robot_state_publisher) |
| `/joint_states` | `sensor_msgs/msg/JointState` | uit: wielposities en -snelheden |

Let op: de controller verwacht **TwistStamped**, geen Twist. Handmatig een commando sturen:

```bash
ros2 topic pub -r 10 /agrobot/cmd_vel geometry_msgs/msg/TwistStamped \
  "{header: {frame_id: base_footprint}, twist: {linear: {x: 0.2}, angular: {z: 0.0}}}"
```

Zonder nieuwe commando's stopt de robot na `cmd_vel_timeout` (0,5 s).

## Controllerconfig

Beide bestanden hebben dezelfde opbouw; `update_rate` van de controller_manager is 10 Hz, odometrie wordt met 50 Hz gepubliceerd.

| Parameter | Curio | ATD |
|---|---|---|
| `wheel_separation` [m] | 1,335 | 1,552 |
| `wheel_radius` [m] | 0,1257 | 0,2032 |
| `linear.x.max_velocity` [m/s] | 1,0 | 1,2 |
| `linear.x.max_acceleration` [m/s²] | 1,0 | 1,0 |
| `angular.z.max_velocity` [rad/s] | 1,0 | 1,0 |
| `angular.z.max_acceleration` [rad/s²] | 1,0 | 1,0 |

- **Curio**: 1,2 m/s (76 ODrive-omwentelingen/s) loopt bij 48 V tegen de modulatielimiet van de ODrive aan, daarom is de maximumsnelheid 1,0 m/s (63 omw/s).
- **ATD**: skid steering slipt in bochten, waardoor de effectieve spoorbreedte groter is dan de echte. Stel `wheel_separation_multiplier` op de grond af: laat de robot 360° ter plaatse draaien en schaal tot de odometrie klopt.

`wheel_separation` en `wheel_radius` moeten overeenkomen met de URDF in [agrobot_description](../agrobot_description). De overbrenging (50:1) zit niet hier maar in de URDF (`gear_ratio`), zodat de controller in wielomwentelingen rekent.

## Gamepad (Logitech F710)

Zet de schakelaar aan de achterkant van de F710 op **X** (XInput).

| Bediening | Functie |
|---|---|
| LB vasthouden | rijden (deadman; loslaten = stoppen) |
| LB + RB vasthouden | turbo |
| linker stick omhoog/omlaag | vooruit/achteruit |
| rechter stick links/rechts | draaien |

| | Normaal | Turbo |
|---|---|---|
| lineair [m/s] | 0,5 | 1,0 |
| hoek [rad/s] | 0,6 | 1,0 |

De config staat in [config/f710.yaml](config/f710.yaml), inclusief de nummering van alle assen en knoppen.

`bringup.launch.py` start de teleop al. Start [teleop_f710.launch.py](launch/teleop_f710.launch.py) alleen los als de robot met `use_teleop:=false` is gestart, anders draait `joy_node` twee keer:

```bash
ros2 launch agrobot_bringup teleop_f710.launch.py
```

Argumenten: `joy_config` (parameterbestand, standaard `config/f710.yaml`) en `cmd_vel_topic` (standaard `/agrobot/cmd_vel`).

## Automatisch starten (systemd)

[agrobot-bringup.service](systemd/agrobot-bringup.service) is een systemd-*user*-service voor gebruiker `agrobot`. Linger staat aan (`loginctl enable-linger agrobot`), dus de service start bij het booten zonder dat iemand inlogt. De service wacht maximaal 60 s tot `can0` up is (dat regelt systemd-networkd via `/etc/systemd/network/80-can.network`) en start dan `bringup.launch.py` met de standaardargumenten (Curio, echte hardware, teleop aan).

```bash
# eenmalig installeren
systemctl --user enable ~/agrobot_ws/src/agrobot_bringup/systemd/agrobot-bringup.service

# gebruik
systemctl --user start|stop|restart|status agrobot-bringup
journalctl --user -u agrobot-bringup -f

# niet meer automatisch starten
systemctl --user disable agrobot-bringup
```

Stop de service voordat je de bringup handmatig start, anders draaien er twee controller_managers op dezelfde CAN-bus. Na een `colcon build` is een `restart` genoeg; de service gebruikt `~/agrobot_ws/install`. `enable` met een pad maakt een symlink naar het bestand in dit package, dus wijzigingen in de service (bijv. `robot:=atd` toevoegen aan `ExecStart`) gelden na `systemctl --user daemon-reload` en een `restart`.
