# ODrive-config: leverancier vs. huidig

Dit document vergelijkt de configuratie die de leverancier meeleverde ([odrive_config_leverancier.json](../config/odrive/odrive_config_leverancier.json)) met de configuratie die nu op beide ODrives staat. Het legt per wijziging uit waarom die is gedaan.

- Vergeleken op 13-11-2026. Alle 353 sleutels uit het leveranciersbestand zijn via CAN uitgelezen op beide ODrives.
- Rechts = node 1, links = node 2 (zie [ODRIVE.md](ODRIVE.md)).
- Waarden die door de kalibratie zijn bepaald, staan niet in de hoofdtabel. Zie [Kalibratiewaarden](#kalibratiewaarden-niet-meegenomen).
- Alle andere sleutels zijn gelijk aan de leveranciersconfig, op beide ODrives.

## Samenvatting

| Groep | Aantal | Belangrijkste reden |
|---|---|---|
| CAN-communicatie | 8 | De leverancier had CAN uitgeschakeld. ROS stuurt de ODrives via CAN aan. |
| Onbedoelde aansturing uitgeschakeld | 3 | Een zwevende analoge ingang liet de wielen draaien bij setpoint 0. |
| Regeling en beveiliging | 7 | Een juiste koppelconstante, valse spinout-uitval voorkomen, de watchdog en een hogere snelheidslimiet. |

## Alle wijzigingen

Rechts en links hebben dezelfde waarde, behalve bij `node_id`.

### CAN-communicatie

| Parameter | Leverancier | Nu | Waarom |
|---|---|---|---|
| `can.config.baud_rate` | `0` (uit) | `250000` | CAN aanzetten op dezelfde snelheid als `can0` op de Raspberry Pi. |
| `can.config.protocol` | `0` (geen) | `1` (CAN Simple) | Het protocol dat `odrive_ros2_control` gebruikt. |
| `axis0.config.can.node_id` | `63` | rechts `1`, links `2` | Elke ODrive heeft een eigen adres nodig. De koppeling staat in [agrobot_curio.ros2_control.xacro](../../agrobot_description/urdf/agrobot_curio.ros2_control.xacro). |
| `axis0.config.can.iq_msg_rate_ms` | `0` | `10` | Stroomterugkoppeling (Iq) naar ROS, 100 Hz. |
| `axis0.config.can.torques_msg_rate_ms` | `0` | `10` | Koppelterugkoppeling, 100 Hz. ROS gebruikt die als `effort`-state van de joints. |
| `axis0.config.can.error_msg_rate_ms` | `0` | `100` | Foutstatus (`active_errors`, `disarm_reason`) periodiek op de bus. |
| `axis0.config.can.temperature_msg_rate_ms` | `0` | `500` | FET- en motortemperatuur periodiek op de bus. |
| `axis0.config.can.bus_voltage_msg_rate_ms` | `0` | `500` | Busspanning en -stroom periodiek op de bus. |

Wie deze CAN-instellingen heeft gezet, is niet vastgelegd. Ze zijn nodig voor de ROS-aansturing en staan vermeld in [odrive_node/README.md](../../ros_odrive/odrive_node/README.md). Waarschijnlijk zijn ze bij de eerste installatie ingesteld.

### Onbedoelde aansturing uitgeschakeld

| Parameter | Leverancier | Nu | Waarom |
|---|---|---|---|
| `config.gpio16_mode` | `3` (ANALOG_IN) | `17` (AUTO) | GPIO16 was een analoge snelheidsingang. De pin is niet aangesloten en zweefde. |
| `config.gpio16_analog_mapping.endpoint` | `axis0.controller.input_vel` | `None` | De zwevende ingang overschreef elk CAN-setpoint. Daardoor draaide het wiel met ongeveer −11 omw/s terwijl ROS 0 vroeg. |
| `axis0.config.startup_closed_loop_control` | `True` | `False` | De wielen gingen bij het aanzetten direct in closed loop, ook zonder ROS. Nu doet ROS dat pas bij het activeren van de controller. |

Details staan in [ODRIVE.md – Probleem: wielen liepen weg bij snelheid 0](ODRIVE.md#probleem-wielen-liepen-weg-bij-snelheid-0).

### Regeling en beveiliging

| Parameter | Leverancier | Nu | Waarom |
|---|---|---|---|
| `axis0.controller.config.vel_limit` | `30` omw/s | `100` omw/s | Met `30` was de robot beperkt tot ongeveer 0,47 m/s. De motoren halen ongeveer 84 omw/s (≈ 1,33 m/s). ROS beperkt de snelheid tot 1,2 m/s (76 omw/s). |
| `axis0.config.motor.torque_constant` | `1.0` | `0.0875` Nm/A | `1.0` is een niet-ingestelde standaardwaarde. De echte waarde is gemeten via de tegen-EMK (KV van de rotor ≈ 9,5 rpm/V, in ODrive-eenheden met 1 poolpaar). Met `1.0` rekende de ODrive het mechanische vermogen ongeveer 11 keer te hoog. |
| `axis0.controller.config.vel_gain` | `1.0` | `0.0875` | Deze gain is in Nm per (omw/s), dus hij moet meeschalen met `torque_constant`. Zo geeft de regelaar per snelheidsfout dezelfde stroom als met de leveranciersinstelling. Zonder dit schalen werd de regeling 11 keer te agressief. |
| `axis0.controller.config.vel_integrator_gain` | `1.0` | `0.0875` | Dezelfde reden als bij `vel_gain`. |
| `axis0.controller.config.spinout_electrical_power_threshold` | `10` W | `500` W | Valse `SPINOUT_DETECTED` bij omkeren van de draairichting. Bij hard remmen op lage snelheid maken de koperverliezen (tot ongeveer 290 W gemeten) het elektrische vermogen positief. `500` W ligt boven het maximale koperverlies bij 30 A (≈ 460 W). |
| `axis0.config.enable_watchdog` | `False` | `True` | Als ROS crasht, de Pi hangt of de CAN-verbinding wegvalt, bleven de wielen draaien op de laatste opdracht. Nu schakelt de ODrive zelf uit. |
| `axis0.config.watchdog_timeout` | `0.0` | `1.0` s | ROS stuurt op 10 Hz. 1 s geeft ruimte voor korte haperingen. |
| `axis0.controller.config.input_mode` | `2` (VEL_RAMP) | `1` (PASSTHROUGH) | Zie hieronder. |

**Over `input_mode`.** Deze wijziging is niet bewust gedaan. `odrive_ros2_control` zet bij elke activering van de controller `INPUT_MODE_PASSTHROUGH` ([odrive_hardware_interface.cpp](../../ros_odrive/odrive_ros2_control/src/odrive_hardware_interface.cpp), `set_axis_command_mode`). Bij een `save_configuration()` terwijl ROS actief was, is die waarde in het flashgeheugen terechtgekomen. In de praktijk maakt het niet uit: onder ROS staat de ODrive altijd op PASSTHROUGH, wat er ook is opgeslagen. Het afvlakken van de snelheid gebeurt in ROS (`max_acceleration` in [agrobot_curio_controllers.yaml](../../agrobot_bringup/config/agrobot_curio_controllers.yaml)). De ramp van de leverancier (`vel_ramp_rate` = 10 omw/s², ≈ 0,16 m/s²) zou de robot ook erg traag laten optrekken.

## Kalibratiewaarden (niet meegenomen)

Deze waarden worden bij de motor- en encoderkalibratie gemeten. Ze verschillen per motor en per kalibratie, dus een verschil is hier normaal.

| Parameter | Leverancier | Rechts | Links |
|---|---|---|---|
| `axis0.config.motor.phase_resistance` | 0,3282 Ω | 0,3379 Ω | 0,3351 Ω |
| `axis0.config.motor.phase_inductance` | 0,919 mH | 0,922 mH | 0,879 mH |
| `axis0.commutation_mapper.config.offset` | 0,2367 | 0,2194 | 0,2287 |

De kalibratiewaarden in het leveranciersbestand komen met geen van beide ODrives exact overeen. Het bestand is dus waarschijnlijk een algemene config, of gemaakt vóór een latere kalibratie.

## Vergelijking opnieuw uitvoeren

Gebruik de odrive-venv uit [ODRIVE.md](ODRIVE.md#config-uitlezen-of-wijzigen-via-usb). Lees elke sleutel uit [odrive_config_leverancier.json](../config/odrive/odrive_config_leverancier.json) via `find_sync(count=2, interfaces=["can:can0"])` uit beide ODrives en vergelijk de waarden. Dat kan terwijl ROS draait: het uitlezen verandert niets.
