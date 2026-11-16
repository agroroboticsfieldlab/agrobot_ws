# agrobot_odrive

Agrobot-specifieke aanvullingen op [ros_odrive](../ros_odrive) (ODrive Robotics). `ros_odrive` zelf blijft ongewijzigd, zodat het met upstream bijgewerkt kan worden.

Dit package bevat:

- **ros2_control-plugin `agrobot_odrive/GearedODriveHardwareInterface`**
  ([src/geared_odrive_hardware_interface.cpp](src/geared_odrive_hardware_interface.cpp)). Deze laadt de originele `odrive_ros2_control_plugin/ODriveHardwareInterface` via pluginlib en voegt alleen een overbrenging per joint toe. Alle CAN-communicatie doet de originele plugin.
- **ODrive-configuratie** in [config/odrive/](config/odrive/): back-ups van beide ODrives en de config van de leverancier.
- **Documentatie** in [docs/](docs/):
  - [ODRIVE.md](docs/ODRIVE.md): hardware, configuratie en waarom die zo is ingesteld.
  - [ODRIVE_CONFIG_VERGELIJKING.md](docs/ODRIVE_CONFIG_VERGELIJKING.md): verschillen met de leveranciersconfig.

De robotbeschrijving staat in `agrobot_description`, de launch- en controllerconfig in `agrobot_bringup`.

## GearedODriveHardwareInterface

Gebruik in de `ros2_control`-tag van de URDF:

```xml
<hardware>
  <plugin>agrobot_odrive/GearedODriveHardwareInterface</plugin>
  <param name="can">can0</param>
</hardware>
<joint name="left_wheel_joint">
  <param name="node_id">2</param>
  <param name="gear_ratio">50</param>
</joint>
```

Parameters:

- Alle parameters van de originele plugin (`can`, `node_id`) worden ongewijzigd doorgegeven.
- `gear_ratio` (per joint, optioneel, standaard `1.0`, moet > 0 zijn): ODrive-omwentelingen per jointomwenteling.

Omrekening:

| | ROS (joint) → ODrive | ODrive → ROS (joint) |
|---|---|---|
| positie, snelheid | × `gear_ratio` | ÷ `gear_ratio` |
| koppel | ÷ `gear_ratio` | × `gear_ratio` |

De interfaces (position, velocity, effort als command en state) en het gedrag bij het wisselen van controller zijn gelijk aan die van de originele plugin.

Waarom de overbrenging niet op de ODrive zelf wordt ingesteld, staat in [ODRIVE.md](docs/ODRIVE.md#waarom-niet-op-de-odrive-zelf-pos_vel_mapperconfigscale).
